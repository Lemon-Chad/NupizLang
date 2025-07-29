#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../util/common.h"
#include "../compiler/compiler.h"
#include "../util/debug.h"
#include "../util/memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
}

static void runtimeError(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = 0; i < vm->frameCount; i++) {
        CallFrame* frame = &vm->frames[i];
        ObjFunction* func = frame->closure->function;
        size_t inst = frame->ip - func->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", getLine(&func->chunk, inst));
        if (func->name == NULL)
            fprintf(stderr, "script\n");
        else
            fprintf(stderr, "%s()\n", func->name->chars);
    }

    resetStack(vm);
}

static void defineNative(VM* vm, const char* name, NativeFn func) {
    push(vm, OBJ_VAL(copyString(vm, name, strlen(name))));
    push(vm, OBJ_VAL(newNative(vm, func)));
    tableSet(vm, &vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop(vm);
    pop(vm);
}

static Value printNative(VM* vm, int argc, Value* args) {
    for (int i = 0; i < argc; i++) {
        printValue(args[i]);
        if (i + 1 < argc)
            printf(" ");
    }
    return NULL_VAL;
}

static Value printlnNative(VM* vm, int argc, Value* args) {
    printNative(vm, argc, args);
    printf("\n");
    return NULL_VAL;
}

static Value asStringNative(VM* vm, int argc, Value* args) {
    if (argc != 1) {
        runtimeError(vm, "Expected 1 arg, got %d.", argc);
        return NULL_VAL;
    }

    Value val = OBJ_VAL(strValue(vm, args[0]));
    return val;
}

static Value lengthNative(VM* vm, int argc, Value* args) {
    if (argc != 1) {
        runtimeError(vm, "Expected 1 arg, got %d.", argc);
        return NULL_VAL;
    }

    Value arg = args[0];
    if (IS_STRING(arg)) {
        return NUMBER_VAL(strlen(AS_CSTRING(arg)));
    } else if (IS_LIST(arg)) {
        return NUMBER_VAL(AS_LIST(arg)->list.count);
    }

    runtimeError(vm, "Cannot measure length of given type.", argc);
    return NULL_VAL;
}

static Value appendNative(VM* vm, int argc, Value* args) {
    if (argc != 2) {
        runtimeError(vm, "Expected 2 args, got %d.", argc);
        return NULL_VAL;
    }

    Value list = args[0];
    Value ele = args[1];
    if (!IS_LIST(list)) {
        runtimeError(vm, "Expected a list as a first arg.");
        return NULL_VAL;
    }

    push(vm, list);
    push(vm, ele);
    writeValueArray(vm, &AS_LIST(list)->list, ele);
    popn(vm, 2);

    return NUMBER_VAL(AS_LIST(list)->list.count);
}

static Value removeNative(VM* vm, int argc, Value* args) {
    if (argc != 2) {
        runtimeError(vm, "Expected 2 args, got %d.", argc);
        return NULL_VAL;
    }

    Value list = args[0];
    Value ele = args[1];
    if (!IS_LIST(list)) {
        runtimeError(vm, "Expected a list as a first arg.");
        return NULL_VAL;
    }
    if (!IS_NUMBER(ele)) {
        runtimeError(vm, "Expected a number index as a second arg.");
        return NULL_VAL;
    }

    int idx = (int) AS_NUMBER(ele);
    ValueArray* array = &AS_LIST(list)->list;
    if (idx < 0)
        idx += array->count;
    
    if (idx < 0 || idx >= array->count) {
        runtimeError(vm, "Index out of bounds.");
        return NULL_VAL;
    }

    memcpy(array->values + idx, array->values + idx + 1, 
        sizeof(Value) * (array->count-- - idx));

    return NUMBER_VAL(array->count);
}

static Value popNative(VM* vm, int argc, Value* args) {
    if (argc != 1) {
        runtimeError(vm, "Expected 1 arg, got %d.", argc);
        return NULL_VAL;
    }

    Value list = args[0];
    if (!IS_LIST(list)) {
        runtimeError(vm, "Expected a list as a first arg.");
        return NULL_VAL;
    }

    ValueArray* array = &AS_LIST(list)->list;
    if (array->count == 0) {
        runtimeError(vm, "Given list is empty.");
        return NULL_VAL;
    }

    array->count--;
    return array->values[array->count];
}

static Value clockNative(VM* vm, int argc, Value* args) {
    return NUMBER_VAL(((double) clock()) / CLOCKS_PER_SEC);
}

void initVM(VM* vm) {
    resetStack(vm);
    vm->objects = NULL;
    vm->compiler = NULL;

    initTable(&vm->globals);
    initTable(&vm->strings);

    vm->grayStack = NULL;
    vm->grayCount = 0;
    vm->grayCapacity = 0;

    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024;

    defineNative(vm, "print", printNative);
    defineNative(vm, "println", printlnNative);
    defineNative(vm, "asString", asStringNative);
    defineNative(vm, "length", lengthNative);

    defineNative(vm, "append", appendNative);
    defineNative(vm, "remove", removeNative);
    defineNative(vm, "pop", popNative);

    defineNative(vm, "clock", clockNative);
}

void freeVM(VM* vm) {
    freeObjects(vm);

    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->strings);
}

static Value peek(VM* vm, int dist) {
    return vm->stackTop[-1 - dist];
}

static bool call(VM* vm, ObjClosure* clos, int argc) {
    if (argc != clos->function->arity) {
        runtimeError(vm, "Expected %d arguments, but recieved %d.", clos->function->arity, argc);
        return false;
    }

    if (vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "Call stack overflow.");
        return false;
    }

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = clos;
    frame->ip = clos->function->chunk.code;
    frame->slots = vm->stackTop - argc - 1;
    return true;
}

static bool callValue(VM* vm, Value callee, int argc) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_NATIVE: {
                NativeFn func = AS_NATIVE(callee);
                Value res = func(vm, argc, vm->stackTop - argc);
                vm->stackTop -= argc + 1;
                push(vm, res);
                return true;
            }
            case OBJ_CLOSURE: {
                return call(vm, AS_CLOSURE(callee), argc);
            }
            case OBJ_CLASS: {
                ObjClass* clazz = AS_CLASS(callee);
                vm->stackTop[-argc - 1] = OBJ_VAL(newInstance(vm, clazz));
                if (clazz->constructor != NULL) {
                    return call(vm, clazz->constructor, argc);
                } else if (argc != 0) {
                    runtimeError(vm, "Expected 0 args but got %d.", argc);
                    return false;
                }
                return true;
            }
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stackTop[-argc - 1] = bound->reciever;
                return call(vm, bound->method, argc);
            }
            default:
                break;
        }
    }
    runtimeError(vm, "Cannot call non-function object.");
    return false;
}

static bool invokeFromClass(VM* vm, ObjClass* clazz, ObjString* name, int argc) {
    Value method;
    if (!tableGet(&clazz->methods, name, &method)) {
        runtimeError(vm, "Undefined property '%s'.", name->chars);
        return false;
    }

    return call(vm, AS_CLOSURE(method), argc);
}

static bool invoke(VM* vm, ObjString* name, int argc) {
    Value reciever = peek(vm, argc);

    if (!IS_INSTANCE(reciever)) {
        runtimeError(vm, "Methods may only be invoked from instances.");
        return false;
    }

    ObjInstance* inst = AS_INSTANCE(reciever);

    Value value;
    if (tableGet(&inst->fields, name, &value)) {
        vm->stackTop[-argc - 1] = value;
        return callValue(vm, value, argc);
    }

    return invokeFromClass(vm, inst->clazz, name, argc);
}

static bool bindMethod(VM* vm, ObjClass* clazz, ObjString* name) {
    Value method;
    if (!tableGet(&clazz->methods, name, &method)) {
        runtimeError(vm, "Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(method));
    pop(vm);
    push(vm, OBJ_VAL(bound));
    return true;
}

NativeResult callDefaultMethod(VM* vm, ObjInstance* inst, int idx, Value* args, int argc) {
    if (inst->clazz->defaultMethods[idx] == NULL) {
        return NATIVE_FAIL;
    }

    push(vm, OBJ_VAL(inst));
    for (int i = 0; i < argc; i++)
        push(vm, args[i]);

    ObjClosure* clos = inst->clazz->defaultMethods[idx];
    if (!call(vm, clos, argc))
        return NATIVE_FAIL;

    InterpretResult res = run(vm);
    if (res == INTERPRET_RUNTIME_ERR)
        return NATIVE_FAIL;
    
    Value val = pop(vm);
    return NATIVE_OK(val);
}

static ObjUpvalue* captureUpvalue(VM* vm, Value* local) {
    ObjUpvalue* prev = NULL;
    ObjUpvalue* curr = vm->openUpvalues;
    while (curr != NULL && curr->location > local) {
        prev = curr;
        curr = curr->next;
    }

    if (curr != NULL && curr->location == local) {
        return curr;
    }

    ObjUpvalue* upvalue = newUpvalue(vm, local);
    
    // Insert into upvalue list
    upvalue->next = curr;
    if (prev == NULL) {
        vm->openUpvalues = upvalue;
    } else {
        prev->next = upvalue;
    }

    return upvalue;
}

static void closeUpvalues(VM* vm, Value* last) {
    while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
        ObjUpvalue* upv = vm->openUpvalues;
        upv->closed = *upv->location;
        upv->location = &upv->closed;
        vm->openUpvalues = upv->next;
    }
}

static bool isFalsey(Value value) {
    return IS_NULL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM* vm) {
    ObjString* b = AS_STRING(peek(vm, 0));
    ObjString* a = AS_STRING(peek(vm, 1));

    int len = a->length + b->length;
    char* chars = ALLOCATE(vm, char, len + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[len] = '\0';

    ObjString* res = takeString(vm, chars, len);
    popn(vm, 2);
    push(vm, OBJ_VAL(res));
}

static void defineMethod(VM* vm, ObjString* name) {
    Value method = peek(vm, 0);
    ObjClass* clazz = AS_CLASS(peek(vm, 1));
    tableSet(vm, &clazz->methods, name, method);
    pop(vm);
}

static void defineDefMethod(VM* vm, int idx) {
    Value method = peek(vm, 0);
    ObjClass* clazz = AS_CLASS(peek(vm, 1));
    clazz->defaultMethods[idx] = AS_CLOSURE(method);

    ObjString* name = NULL;
    switch (idx) {
        case DEFMTH_STRING:
            name = formatString(vm, "string");
            break;
        default:
            runtimeError(vm, "Unkown default method '%d'.", idx);
            break;
    }
    tableSet(vm, &clazz->methods, name, method);
    
    pop(vm);
}

static void defineBuilder(VM* vm) {
    Value method = peek(vm, 0);
    ObjClass* clazz = AS_CLASS(peek(vm, 1));
    clazz->constructor = AS_CLOSURE(method);
    pop(vm);
}

InterpretResult run(VM* vm) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    int exitLevel = vm->frameCount - 1;
    #define READ_BYTE() (*(frame->ip++))
    #define READ_SHORT() (frame->ip += 2, (uint16_t) ((frame->ip[-2] << 8) | frame->ip[-1]))
    #define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
    #define READ_LONG_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE() + (READ_BYTE() << 8) + (READ_BYTE() << 16)])
    #define READ_STRING() AS_STRING(READ_CONSTANT())
    #define BINARY_OP(velcro, op) \
        do { \
            if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
                runtimeError(vm, "DTypeErr: Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERR; \
            } \
            double b = AS_NUMBER(pop(vm)); \
            double a = AS_NUMBER(pop(vm)); \
            push(vm, velcro(a op b)); \
        } while (false)

    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk, 
                (int) (frame->ip - frame->closure->function->chunk.code));
        #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_CONSTANT_LONG: {
                Value constant = READ_LONG_CONSTANT();
                push(vm, constant);
                break;
            }
            case OP_TRUE:
                push(vm, BOOL_VAL(true));
                break;
            case OP_FALSE:
                push(vm, BOOL_VAL(false));
                break;
            case OP_NULL:
                push(vm, NULL_VAL);
                break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(vm, 0))) {
                    runtimeError(vm, "DTypeErr: Operand must be a number.");
                    return INTERPRET_RUNTIME_ERR;
                }
                vm->stackTop[-1].as.number = -vm->stackTop[-1].as.number;
                break;
            case OP_NOT:
                push(vm, BOOL_VAL(isFalsey(pop(vm))));
                break;
            case OP_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_NOT_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(!valuesEqual(a, b)));
                break;
            }
            case OP_ADD:
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate(vm);
                } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtimeError(vm, "Operands must be of the same type.");
                    return INTERPRET_RUNTIME_ERR;
                }
                break;

            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_GREATER_EQUAL: BINARY_OP(BOOL_VAL, >=); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_LESS_EQUAL: BINARY_OP(BOOL_VAL, <=); break;
            case OP_RETURN: {
                Value res = pop(vm);
                closeUpvalues(vm, frame->slots);
                vm->frameCount--;
                if (vm->frameCount == 0) {
                    pop(vm);
                    return INTERPRET_OK;
                }

                vm->stackTop = frame->slots;
                push(vm, res);
                frame = &vm->frames[vm->frameCount - 1];

                if (vm->frameCount == exitLevel) {
                    return INTERPRET_OK;
                }

                break;
            }
            
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(vm, &vm->globals, name, peek(vm, 0));
                pop(vm);
                break;
            }

            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(vm, &vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    runtimeError(vm, "Global variable '%s' is undefined.", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }

            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value val;
                if (!tableGet(&vm->globals, name, &val)) {
                    runtimeError(vm, "Global variable '%s' is undefined.", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                push(vm, val);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm, frame->slots[slot]);
                break;
            }

            case OP_LOOP: {
                uint16_t offs = READ_SHORT();
                frame->ip -= offs;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offs = READ_SHORT();
                if (isFalsey(peek(vm, 0)))
                    frame->ip += offs;
                break;
            }

            case OP_JUMP_IF_TRUE: {
                uint16_t offs = READ_SHORT();
                if (!isFalsey(peek(vm, 0)))
                    frame->ip += offs;
                break;
            }

            case OP_JUMP: {
                uint16_t offs = READ_SHORT();
                frame->ip += offs;
                break;
            }

            case OP_POP:
                pop(vm);
                break;
            
            case OP_POP_N:
                popn(vm, READ_BYTE());
                break;

            case OP_CALL: {
                int argc = READ_BYTE();
                if (!callValue(vm, peek(vm, argc), argc)) {
                    return INTERPRET_RUNTIME_ERR;
                }
                frame = &vm->frames[vm->frameCount - 1];
                break;
            }

            case OP_CLOSURE: {
                bool isLong = READ_BYTE() == OP_CONSTANT_LONG;
                ObjFunction* func = AS_FUNCTION(isLong ? READ_LONG_CONSTANT() : READ_CONSTANT());
                ObjClosure* clos = newClosure(vm, func);
                push(vm, OBJ_VAL(clos));
                
                for (int i = 0; i < clos->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t idx = READ_BYTE();
                    if (isLocal) {
                        clos->upvalues[i] = captureUpvalue(vm, frame->slots + idx);
                    } else {
                        clos->upvalues[i] = frame->closure->upvalues[idx];
                    }
                }

                break;
            }

            case OP_CLOSE_UPVALUE:
                closeUpvalues(vm, vm->stackTop - 1);
                pop(vm);
                break;

            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(vm, *frame->closure->upvalues[slot]->location);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(vm, 0);
                break;
            }
        
            case OP_CLASS: {
                push(vm, OBJ_VAL(newClass(vm, READ_STRING())));
                break;
            }
        
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(vm, 0))) {
                    runtimeError(vm, "Cannot access property of non-instance.");
                    return INTERPRET_RUNTIME_ERR;
                }

                ObjInstance* inst = AS_INSTANCE(peek(vm, 0));
                ObjString* name = READ_STRING();

                Value val;
                if (tableGet(&inst->fields, name, &val)) {
                    pop(vm);
                    push(vm, val);
                    break;
                }

                if (!bindMethod(vm, inst->clazz, name)) {
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }

            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(vm, 1))) {
                    runtimeError(vm, "Cannot set property of non-instance.");
                    return INTERPRET_RUNTIME_ERR;
                }

                ObjInstance* inst = AS_INSTANCE(peek(vm, 1));
                tableSet(vm, &inst->fields, READ_STRING(), peek(vm, 0));
                Value val = pop(vm);
                pop(vm);
                push(vm, val);
                break;
            }

            case OP_METHOD: {
                int methodType = READ_BYTE();
                if (methodType == 1) {
                    defineBuilder(vm);
                } else if (methodType == 2) {
                    defineDefMethod(vm, READ_BYTE());
                } else {
                    defineMethod(vm, READ_STRING());
                }
                break;
            }

            case OP_INVOKE: {
                ObjString* method = READ_STRING();
                int argc = READ_BYTE();
                if (!invoke(vm, method, argc)) {
                    return INTERPRET_RUNTIME_ERR;
                }

                frame = &vm->frames[vm->frameCount - 1];
                break;
            }

            case OP_INHERIT: {
                Value val = peek(vm, 1);
                if (!IS_CLASS(val)) {
                    runtimeError(vm, "Cannot inherit from non-class objects.");
                    return INTERPRET_RUNTIME_ERR;
                }

                ObjClass* subclass = AS_CLASS(peek(vm, 0));
                ObjClass* superclass = AS_CLASS(val);

                tableAddAll(vm, &superclass->methods, &subclass->methods);
                for (int i = 0; i < DEFAULT_METHOD_COUNT; i++)
                    subclass->defaultMethods[i] = superclass->defaultMethods[i];    

                pop(vm);
                break;
            }

            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop(vm));
                
                if (!bindMethod(vm, superclass, name)) {
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }

            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argc = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop(vm));

                if (!invokeFromClass(vm, superclass, method, argc)) {
                    return INTERPRET_RUNTIME_ERR;
                }

                frame = &vm->frames[vm->frameCount - 1];
                break;
            }

            case OP_MAKE_LIST: {
                int argc = READ_BYTE();
                ObjList* list = newList(vm);
                for (int i = 0; i < argc; i++)
                    writeValueArray(vm, &list->list, peek(vm, argc - i - 1));
                popn(vm, argc);
                push(vm, OBJ_VAL(list));
                break;
            }

            case OP_GET_INDEX: {
                Value b = pop(vm);
                Value a = pop(vm);
                if (IS_LIST(a) && IS_NUMBER(b)) {
                    ObjList* lst = AS_LIST(a);
                    int idx = (int) AS_NUMBER(b);
                    if (idx < 0)
                        idx += lst->list.count;
                    
                    if (idx >= lst->list.count || idx < 0) {
                        runtimeError(vm, "Index out of bounds.");
                        return INTERPRET_RUNTIME_ERR;
                    }
                    push(vm, lst->list.values[idx]);
                } else {
                    runtimeError(vm, "Invalid index getting operation recipients.");
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }

            case OP_SET_INDEX: {
                Value newVal = peek(vm, 0);
                Value b = peek(vm, 1);
                Value a = peek(vm, 2);
                if (IS_LIST(a) && IS_NUMBER(b)) {
                    ObjList* lst = AS_LIST(a);
                    int idx = (int) AS_NUMBER(b);
                    if (idx < 0)
                        idx += lst->list.count;
                    
                    if (idx >= lst->list.count || idx < 0) {
                        runtimeError(vm, "Index out of bounds.");
                        return INTERPRET_RUNTIME_ERR;
                    }

                    lst->list.values[idx] = newVal;

                    popn(vm, 3);
                    push(vm, newVal);
                } else {
                    runtimeError(vm, "Invalid index setting operation recipients.");
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }
        }
    }

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}

InterpretResult runFunc(VM* vm, ObjFunction* func) {
    push(vm, OBJ_VAL(func));
    ObjClosure* clos = newClosure(vm, func);
    pop(vm);
    push(vm, OBJ_VAL(clos));
    call(vm, clos, 0);

    return run(vm);
}

InterpretResult interpret(VM* vm, const char* src) {
    ObjFunction* func = compile(vm, src);
    if (func == NULL)
        return INTERPRET_COMPILE_ERR;
    
    return runFunc(vm, func);
}

void push(VM* vm, Value value) {
    *vm->stackTop = value;
    vm->stackTop++;
}

Value pop(VM* vm) {
    vm->stackTop--;
    return *vm->stackTop;
}

void popn(VM* vm, int n) {
    vm->stackTop -= n;
}
