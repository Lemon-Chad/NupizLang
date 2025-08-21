#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/common.h"
#include "../compiler/compiler.h"
#include "../util/debug.h"
#include "../libraries/core/manager.h"
#include "../util/memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

static void resetStack(VM* vm) {
    vm->stackTop = vm->stack;
    vm->frameCount = 0;
    vm->openUpvalues = NULL;
}

void runtimeError(VM* vm, const char* format, ...) {
    if (vm->safeMode > 0) return;

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

void initVM(VM* vm, const char* name) {
    resetStack(vm);
    vm->objects = NULL;
    vm->compiler = NULL;
    vm->safeMode = 0;
    vm->pauseGC = 0;
    vm->isMain = false;
    vm->mainFunc = NULL;
    vm->nspace = NULL;

    initTable(&vm->globals);
    initTable(&vm->strings);
    initTable(&vm->libraries);
    initTable(&vm->importedFiles);

    vm->grayStack = NULL;
    vm->grayCount = 0;
    vm->grayCapacity = 0;

    vm->bytesAllocated = 0;
    vm->nextGC = 1024 * 1024;

    defineAllLibraries(vm);

    vm->argv = NULL;
    vm->argc = 0;

    ObjString* str = copyString(vm, name, strlen(name));
    push(vm, OBJ_VAL(str));
    vm->nspace = newNamespace(vm, str);
    pop(vm);
}

static void endVM(VM* vm) {
    freeTable(vm, &vm->strings);   
    freeTable(vm, &vm->globals);
    freeTable(vm, &vm->importedFiles);
}

void freeVM(VM* vm) {
    freeObjects(vm);

    endVM(vm);
}

void decoupleVM(VM* vm) {
    vm->mainFunc = NULL;

    endVM(vm);
}

static bool getBound(VM* vm, Value bound, ObjString* name) {
    if (IS_OBJ(bound)) {
        switch (OBJ_TYPE(bound)) {
            case OBJ_INSTANCE: {
                ObjInstance* inst = AS_INSTANCE(bound);

                Value val;
                vm->safeMode++;
                if (getInstanceField(vm, inst, name, &val, true) ||
                        getInstanceMethod(vm, inst, name, &val, true)) {
                    vm->safeMode--;
                    push(vm, val);
                    return true;
                }
                vm->safeMode--;

                if (!IS_NULL(inst->bound))
                    return getBound(vm, inst->bound, name);

                return false;
            }

            case OBJ_CLASS: {
                ObjClass* clazz = AS_CLASS(bound);

                Value val;
                vm->safeMode++;
                if (getClassField(vm, clazz, name, &val, true) ||
                        getClassMethod(vm, clazz, name, &val, true)) {
                    vm->safeMode--;
                    push(vm, val);
                    return true;
                }
                vm->safeMode--;

                if (!IS_NULL(clazz->bound))
                    return getBound(vm, clazz->bound, name);

                return false;
            }

            case OBJ_NAMESPACE: {
                ObjNamespace* nspace = AS_NAMESPACE(bound);

                Value val;
                if (!getNamespace(vm, nspace, name, &val, true)) {
                    runtimeError(vm, "Undefined attribute '%s'.", name->chars);
                    return false;
                }

                if (IS_CLASS(val))
                    AS_CLASS(val)->bound = bound;

                push(vm, val);
                return true;
            }

            default:
                break;
        }
    }
    
    runtimeError(vm, "Requesting attribute outside of class or instance context.");
    return false;
}

static bool setBound(VM* vm, ObjString* name, Value val) {
    CallFrame* frame = &vm->frames[vm->frameCount - 1];
    if (IS_OBJ(frame->bound)) {
        switch (OBJ_TYPE(frame->bound)) {
            case OBJ_INSTANCE: 
                return setInstanceField(vm, 
                    AS_INSTANCE(frame->bound), name, val, true);
            
            case OBJ_CLASS: 
                return setClassField(vm, 
                    AS_CLASS(frame->bound), name, val, true);
            
            case OBJ_NAMESPACE: {
                ObjNamespace* nspace = AS_NAMESPACE(frame->bound);

                if (!writeNamespace(vm, nspace, name, val, true)) {
                    runtimeError(vm, "Undefined attribute '%s'.", name->chars);
                    return false;
                }

                push(vm, val);
                return true;
            }
            
            default:
                break;
        }
    }
    
    runtimeError(vm, "Requesting attribute outside of class or instance context.");
    return false;
}

static Value peek(VM* vm, int dist) {
    return vm->stackTop[-1 - dist];
}

static bool call(VM* vm, ObjClosure* clos, int argc, Value binder) {
    if (argc != clos->function->arity) {
        runtimeError(vm, "Expected %d arguments, but recieved %d.", clos->function->arity, argc);
        return false;
    }

    if (vm->frameCount == FRAMES_MAX) {
        runtimeError(vm, "Call stack overflow.");
        return false;
    }

    if (IS_NULL(binder) && vm->frameCount > 0)
        binder = vm->frames[vm->frameCount - 1].bound;

    CallFrame* frame = &vm->frames[vm->frameCount++];
    frame->closure = clos;
    frame->ip = clos->function->chunk.code;
    frame->slots = vm->stackTop - argc - 1;
    frame->bound = binder;
    return true;
}

void callFunc(VM* vm, ObjClosure* clos, int argc, Value binder) {
    call(vm, clos, argc, binder);
}

static bool callValue(VM* vm, Value callee, int argc) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_NATIVE: {
                NativeFn func = AS_NATIVE(callee);

                NativeResult res = func(vm, argc, vm->stackTop - argc);
                if (!res.success)
                    return false;
                
                vm->stackTop -= argc + 1;
                push(vm, res.val);
                return true;
            }
            case OBJ_CLOSURE: {
                return call(vm, AS_CLOSURE(callee), argc, NULL_VAL);
            }
            case OBJ_CLASS: {
                ObjClass* clazz = AS_CLASS(callee);

                Value inst = OBJ_VAL(newInstance(vm, clazz));
                vm->stackTop[-argc - 1] = inst;

                if (clazz->constructor != NULL) {
                    return call(vm, clazz->constructor, argc, inst);
                } else if (argc != 0) {
                    runtimeError(vm, "Expected 0 args but got %d.", argc);
                    return false;
                }
                return true;
            }
            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm->stackTop[-argc - 1] = bound->reciever;
                return call(vm, bound->method, argc, bound->reciever);
            }
            default:
                break;
        }
    }
    runtimeError(vm, "Cannot call non-function object.");
    return false;
}

static bool invokeFromClass(VM* vm, ObjClass* clazz, ObjString* name, int argc, Value inst) {
    Value method;
    if (!getInstanceClassMethod(vm, clazz, name, &method, false)) {
        return false;
    }

    return call(vm, AS_CLOSURE(method), argc, inst);
}

static bool invoke(VM* vm, ObjString* name, int argc) {
    Value reciever = peek(vm, argc);

    if (!IS_OBJ(reciever))
        return false;
    
    switch (OBJ_TYPE(reciever)) {
        case OBJ_INSTANCE: {
            ObjInstance* inst = AS_INSTANCE(reciever);

            Value value;
            vm->safeMode++;
            if (getInstanceField(vm, inst, name, &value, false)) {
                vm->safeMode--;
                vm->stackTop[-argc - 1] = value;
                return callValue(vm, value, argc);
            }
            vm->safeMode--;

            return invokeFromClass(vm, inst->clazz, name, argc, reciever);
        }

        case OBJ_CLASS: {
            ObjClass* clazz = AS_CLASS(reciever);

            Value value;
            vm->safeMode++;
            if (getClassField(vm, clazz, name, &value, false)) {
                vm->safeMode--;
                vm->stackTop[-argc - 1] = value;
                return callValue(vm, value, argc);
            }
            vm->safeMode--;

            Value method;
            if (!getClassMethod(vm, clazz, name, &method, false)) {
                return false;
            }

            return call(vm, AS_CLOSURE(method), argc, reciever);
        }

        case OBJ_NAMESPACE: {
            ObjNamespace* nspace = AS_NAMESPACE(reciever);

            Value value;
            if (!getNamespace(vm, nspace, name, &value, false)) {
                runtimeError(vm, "Undefined attribute '%s'.", name->chars);
                return false;
            }

            if (IS_CLOSURE(value)) {
                return call(vm, AS_CLOSURE(value), argc, reciever);
            }

            if (IS_CLASS(value))
                AS_CLASS(value)->bound = reciever;

            return callValue(vm, value, argc);
        }

        default:
            runtimeError(vm, "Methods may not be invoked on the given type.");
            return false;
    }
}

static bool bindMethod(VM* vm, ObjClass* clazz, ObjString* name, bool internal) {
    Value method;
    if (!getInstanceClassMethod(vm, clazz, name, &method, internal)) {
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

    Value instVal = OBJ_VAL(inst);
    push(vm, instVal);
    for (int i = 0; i < argc; i++)
        push(vm, args[i]);

    ObjClosure* clos = inst->clazz->defaultMethods[idx];
    if (!call(vm, clos, argc, instVal))
        return NATIVE_FAIL;

    InterpretResult res = run(vm);
    if (res == INTERPRET_RUNTIME_ERR)
        return NATIVE_FAIL;
    
    Value val = pop(vm);
    return NATIVE_VAL(val);
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

static void addLists(VM* vm) {
    ObjList* b = AS_LIST(peek(vm, 0));
    ObjList* a = AS_LIST(peek(vm, 1));

    ObjList* list = newList(vm);
    push(vm, OBJ_VAL(list));

    for (int i = 0; i < a->list.count; i++)
        writeValueArray(vm, &list->list, a->list.values[i]);
    for (int i = 0; i < b->list.count; i++)
        writeValueArray(vm, &list->list, b->list.values[i]);

    popn(vm, 3);
    push(vm, OBJ_VAL(list));
}

static bool defineMethod(VM* vm, ObjString* name, bool isPublic, bool isStatic) {
    Value method = peek(vm, 0);
    ObjClass* clazz = AS_CLASS(peek(vm, 1));
    if (!declareClassMethod(vm, clazz, name, method, isPublic, isStatic))
        return false;
    pop(vm);
    return true;
}

static bool defineAttribute(VM* vm, ObjString* name, bool isConstant,
        bool isPublic, bool isStatic) {
    Value val = peek(vm, 0);
    ObjClass* clazz = AS_CLASS(peek(vm, 1));
    if (!declareClassField(vm, clazz, name, val, isPublic, isStatic, isConstant))
        return false;
    pop(vm);
    return true;
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
        case DEFMTH_EQ:
            name = formatString(vm, "eq");
            break;
        case DEFMTH_HASH:
            name = formatString(vm, "hash");
            break;
        default:
            runtimeError(vm, "Unkown default method '%d'.", idx);
            break;
    }
    push(vm, OBJ_VAL(name));
    declareClassMethod(vm, clazz, name, method, true, false);
    
    popn(vm, 2);
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
    #define BINARY_NUMBER_OP(velcro, op) \
        do { \
            if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
                runtimeError(vm, "DTypeErr: Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERR; \
            } \
            double b = AS_NUMBER(pop(vm)); \
            double a = AS_NUMBER(pop(vm)); \
            push(vm, velcro(a op b)); \
        } while (false)
    #define BINARY_JOINT_OP(num_op, str_op) \
        do { \
            if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) { \
                char* b = AS_CSTRING(pop(vm)); \
                char* a = AS_CSTRING(pop(vm)); \
                push(vm, str_op); \
            } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) { \
                double b = AS_NUMBER(pop(vm)); \
                double a = AS_NUMBER(pop(vm)); \
                push(vm, num_op); \
            } else { \
                runtimeError(vm, "Operands must be of the same type."); \
                return INTERPRET_RUNTIME_ERR; \
            } \
            break; \
        } while(false)

    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
            printf("          ");
            for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
                printf("[ ");
                if (IS_STRING(*slot)) {
                    printf("%.*s", AS_STRING(*slot)->length < 10 ? AS_STRING(*slot)->length : 10, AS_CSTRING(*slot));
                } else {
                    printValue(*slot);
                }
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
                push(vm, BOOL_VAL(valuesEqual(vm, a, b)));
                break;
            }
            case OP_NOT_EQUAL: {
                Value b = pop(vm);
                Value a = pop(vm);
                push(vm, BOOL_VAL(!valuesEqual(vm, a, b)));
                break;
            }
            case OP_ADD:
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
                    concatenate(vm);
                } else if (IS_LIST(peek(vm, 0)) && IS_LIST(peek(vm, 0))) {
                    addLists(vm);
                } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
                    double b = AS_NUMBER(pop(vm));
                    double a = AS_NUMBER(pop(vm));
                    push(vm, NUMBER_VAL(a + b));
                } else {
                    runtimeError(vm, "Operands must be of the same type.");
                    return INTERPRET_RUNTIME_ERR;
                }
                break;

            case OP_SUBTRACT: BINARY_NUMBER_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_NUMBER_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_NUMBER_OP(NUMBER_VAL, /); break;
            case OP_GREATER: 
                BINARY_JOINT_OP(BOOL_VAL(a > b), BOOL_VAL(strcmp(a, b) > 0)); 
                break;
            case OP_GREATER_EQUAL:
                BINARY_JOINT_OP(BOOL_VAL(a >= b), BOOL_VAL(strcmp(a, b) >= 0)); 
                break;
            case OP_LESS:
                BINARY_JOINT_OP(BOOL_VAL(a < b), BOOL_VAL(strcmp(a, b) < 0)); 
                break;
            case OP_LESS_EQUAL:
                BINARY_JOINT_OP(BOOL_VAL(a <= b), BOOL_VAL(strcmp(a, b) <= 0)); 
                break;
            case OP_RETURN: {
                Value res = pop(vm);
                closeUpvalues(vm, frame->slots);
                vm->frameCount--;
                if (vm->frameCount == 0) {
                    if (vm->keepTop <= 0)
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
                writeNamespace(vm, vm->nspace, name, peek(vm, 0), true);
                pop(vm);
                break;
            }

            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();

                vm->safeMode++;
                if (setBound(vm, name, peek(vm, 0))) {
                    vm->safeMode--;
                    break;
                }
                vm->safeMode--;
                
                if (tableSet(vm, &vm->globals, name, peek(vm, 0))) {
                    tableDelete(&vm->globals, name);
                    runtimeError(vm, "Global variable '%s' is undefined.", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }
                writeNamespace(vm, vm->nspace, name, peek(vm, 0), true);
                break;
            }

            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value val;

                vm->safeMode++;
                if (getBound(vm, vm->frames[vm->frameCount - 1].bound, name)) {
                    vm->safeMode--;
                    break;
                }
                vm->safeMode--;
                
                if (!tableGet(&vm->globals, name, &val)) {
                    runtimeError(vm, "Global variable '%s' is undefined.", name->chars);
                    return INTERPRET_RUNTIME_ERR;
                }

                if (IS_CLASS(val)) {
                    if (IS_NAMESPACE(frame->bound))
                        AS_CLASS(val)->bound = frame->bound;
                    else if (IS_CLASS(frame->bound))
                        AS_CLASS(val)->bound = AS_CLASS(frame->bound)->bound;
                    else if (IS_INSTANCE(frame->bound))
                        AS_CLASS(val)->bound = AS_INSTANCE(frame->bound)->bound;
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
                Value accessed = peek(vm, 0);
                ObjString* name = READ_STRING();
                if (!IS_OBJ(accessed)) {
                    runtimeError(vm, "Given type does not support property access.");
                    return INTERPRET_RUNTIME_ERR;
                }

                switch (OBJ_TYPE(accessed)) {
                    case OBJ_INSTANCE: {
                        ObjInstance* inst = AS_INSTANCE(accessed);

                        Value val;
                        vm->safeMode++;
                        if (getInstanceField(vm, inst, name, &val, false)) {
                            vm->safeMode--;
                            pop(vm);
                            push(vm, val);
                            break;
                        }
                        vm->safeMode--;

                        if (!bindMethod(vm, inst->clazz, name, false)) {
                            return INTERPRET_RUNTIME_ERR;
                        }

                        break;
                    }

                    case OBJ_CLASS: {
                        ObjClass* clazz = AS_CLASS(accessed);

                        Value val;
                        vm->safeMode++;
                        if (getClassField(vm, clazz, name, &val, false)) {
                            vm->safeMode--;
                            pop(vm);
                            push(vm, val);
                            break;
                        }
                        vm->safeMode--;
                        
                        if (getClassMethod(vm, clazz, name, &val, false)) {
                            ObjBoundMethod* boundMethod = newBoundMethod(vm, peek(vm, 0), AS_CLOSURE(val));
                            pop(vm);
                            push(vm, OBJ_VAL(boundMethod));
                            break;
                        }

                        return INTERPRET_RUNTIME_ERR;
                    }

                    case OBJ_NAMESPACE: {
                        ObjNamespace* nspace = AS_NAMESPACE(accessed);

                        Value val;
                        if (!getNamespace(vm, nspace, name, &val, false)) {
                            runtimeError(vm, "Undefined property '%s'.", name->chars);
                            return INTERPRET_RUNTIME_ERR;
                        }

                        if (IS_CLOSURE(val))
                            val = OBJ_VAL(newBoundMethod(vm, accessed, AS_CLOSURE(val)));
                        
                        if (IS_CLASS(val))
                            AS_CLASS(val)->bound = accessed;
                        
                        pop(vm);
                        push(vm, val);
                        break;
                    }

                    default:
                        runtimeError(vm, "Given type does not support property access.");
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
                if (!setInstanceField(vm, inst, READ_STRING(), peek(vm, 0), false))
                    return INTERPRET_RUNTIME_ERR;
                
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
                    if (!defineMethod(vm, READ_STRING(), 
                            READ_BYTE() == 1, READ_BYTE() == 1))
                        return INTERPRET_RUNTIME_ERR;
                }
                break;
            }

            case OP_ATTRIBUTE:
                if (!defineAttribute(vm, READ_STRING(), READ_BYTE() == 1, 
                        READ_BYTE() == 1, READ_BYTE() == 1))
                    return INTERPRET_RUNTIME_ERR;
                break;

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
                tableAddAll(vm, &superclass->staticFields, &subclass->staticFields);
                for (int i = 0; i < DEFAULT_METHOD_COUNT; i++)
                    subclass->defaultMethods[i] = superclass->defaultMethods[i];
                tableAddAll(vm, &superclass->fields, &subclass->fields);

                pop(vm);
                break;
            }

            case OP_GET_SUPER: {
                ObjString* name = READ_STRING();
                ObjClass* superclass = AS_CLASS(pop(vm));
                
                if (!bindMethod(vm, superclass, name, false)) {
                    return INTERPRET_RUNTIME_ERR;
                }
                break;
            }

            case OP_SUPER_INVOKE: {
                ObjString* method = READ_STRING();
                int argc = READ_BYTE();
                ObjClass* superclass = AS_CLASS(pop(vm));

                if (!invokeFromClass(vm, superclass, method, argc, NULL_VAL)) {
                    return INTERPRET_RUNTIME_ERR;
                }

                frame = &vm->frames[vm->frameCount - 1];
                break;
            }

            case OP_MAKE_LIST: {
                int argc = READ_BYTE();
                ObjList* list = newList(vm);
                push(vm, OBJ_VAL(list));
                for (int i = 0; i < argc; i++)
                    writeValueArray(vm, &list->list, peek(vm, argc - i));
                popn(vm, argc + 1);
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
                } else if (IS_STRING(a) && IS_NUMBER(b)) {
                    ObjString* str = AS_STRING(a);
                    int idx = (int) AS_NUMBER(b);
                    if (idx < 0)
                        idx += str->length;
                    
                    if (idx >= str->length || idx < 0) {
                        runtimeError(vm, "Index out of bounds.");
                        return INTERPRET_RUNTIME_ERR;
                    }
                    push(vm, OBJ_VAL(copyString(vm, str->chars + idx, 1)));
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
        
            case OP_IMPORT: {
                ObjString* lib = READ_STRING();
                if (!importLibrary(vm, lib)) {
                    runtimeError(vm, "Undefined library '%s'.", lib->chars);
                    return INTERPRET_RUNTIME_ERR;
                }

                Value libVal;
                tableGet(&vm->libraries, lib, &libVal);
                
                push(vm, OBJ_VAL(AS_LIBRARY(libVal)->nspace));
                break;
            }

            case OP_IMPORT_FILE: {
                ObjString* filename = AS_STRING(peek(vm, 1));

                Value importVal;
                if (IS_STRING(peek(vm, 0)) && 
                        tableGet(&vm->importedFiles, filename, &importVal) && 
                        IS_NAMESPACE(importVal)) {
                    vm->stackTop[-2] = importVal;
                    pop(vm);
                    break;
                }

                ObjFunction* func = AS_FUNCTION(peek(vm, 0));

                VM* temp = malloc(sizeof(VM));
                initVM(temp, filename->chars);

                tableSet(vm, &vm->importedFiles, filename, OBJ_VAL(temp->nspace));
                tableAddAll(temp, &vm->importedFiles, &temp->importedFiles);

                runFunc(temp, func);

                ObjNamespace* nspace = temp->nspace;
                vm->stackTop[-2] = OBJ_VAL(nspace);
                tableSet(vm, &vm->importedFiles, filename, OBJ_VAL(nspace));
                tableAddAll(vm, &temp->importedFiles, &vm->importedFiles);
                
                decoupleVM(temp);
                takeOwnership(vm, temp->objects);
                
                pop(vm);

                break;
            }

            case OP_UNPACK: {
                Value op = peek(vm, 0);
                if (!IS_OBJ(op)) {
                    runtimeError(vm, "Given type does not support unpacking.");
                    return INTERPRET_RUNTIME_ERR;
                }

                switch (OBJ_TYPE(op)) {
                    case OBJ_NAMESPACE: {
                        tableAddAll(vm, AS_NAMESPACE(op)->publics, &vm->globals);
                        tableAddAll(vm, AS_NAMESPACE(op)->publics, vm->nspace->publics);
                        tableAddAll(vm, AS_NAMESPACE(op)->publics, vm->nspace->values);
                        break;
                    }

                    default:
                        runtimeError(vm, "Given type does not support unpacking.");
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

InterpretResult runFuncBound(VM* vm, ObjFunction* func, Value bound) {
    push(vm, OBJ_VAL(func));
    ObjClosure* clos = newClosure(vm, func);
    pop(vm);
    push(vm, OBJ_VAL(clos));
    call(vm, clos, 0, bound);

    return run(vm);
}

InterpretResult runFunc(VM* vm, ObjFunction* func) {
    return runFuncBound(vm, func, NULL_VAL);
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
