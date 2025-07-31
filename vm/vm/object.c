#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/memory.h"
#include "object.h"
#include "../util/table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) (type*) allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(VM* vm, size_t size, ObjType type) {
    Obj* object = (Obj*) reallocate(vm, NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm->objects;
    vm->objects = object;

    #ifdef DEBUG_LOG_GC
        printf("%p allocate %zu for %d\n", (void*) object, size, type);
    #endif

    return object;
}

static ObjString* allocateString(VM* vm, const char* src, int len, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = len;
    string->chars = src;
    string->hash = hash;

    push(vm, OBJ_VAL(string));
    tableSet(vm, &vm->strings, string, NULL_VAL);
    pop(vm);

    return string;
}

// FNV-1a
static uint32_t hashString(const char* src, int len) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < len; i++) {
        hash ^= (uint8_t) src[i];
        hash *= 16777619;
    }
    return hash;
}

ObjClosure* newClosure(VM* vm, ObjFunction* func) {
    ObjUpvalue** upvalues = ALLOCATE(vm, ObjUpvalue*, func->upvalueCount);
    for (int i = 0; i < func->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* clos = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    clos->function = func;
    clos->upvalues = upvalues;
    clos->upvalueCount = func->upvalueCount;
    return clos;
}

ObjFunction* newFunction(VM* vm) {
    ObjFunction* func = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);
    func->arity = 0;
    func->name = NULL;
    func->upvalueCount = 0;
    initChunk(&func->chunk);
    return func;
}

ObjNative* newNative(VM* vm, NativeFn func) {
    ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = func;
    return native;
}

ObjClass* newClass(VM* vm, ObjString* name) {
    ObjClass* clazz = ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
    clazz->name = name;
    clazz->constructor = NULL;
    initTable(&clazz->methods);
    return clazz;
}

ObjInstance* newInstance(VM* vm, ObjClass* clazz) {
    ObjInstance* inst = ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
    inst->clazz = clazz;
    initTable(&inst->fields);
    return inst;
}

ObjBoundMethod* newBoundMethod(VM* vm, Value reciever, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->reciever = reciever;
    bound->method = method;
    return bound;
}

ObjList* newList(VM* vm) {
    ObjList* list = ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    initValueArray(&list->list);
    return list;
}

ObjNamespace* newNamespace(VM* vm, ObjString* name) {
    ObjNamespace* namespace = ALLOCATE_OBJ(vm, ObjNamespace, OBJ_NAMESPACE);
    
    namespace->name = name;
    initTable(&namespace->publics);
    initTable(&namespace->values);

    return namespace;
}

bool writeNamespace(VM* vm, ObjNamespace* namespace, ObjString* name, Value val, bool public) {
    bool newKey = tableSet(vm, &namespace->values, name, val);
    if (public)
        tableSet(vm, &namespace->publics, name, val);
    return newKey;
}

bool getNamespace(VM* vm, ObjNamespace* namespace, ObjString* name, Value* ptr, bool internal) {
    if (!internal && !tableGet(&namespace->publics, name, ptr))
        return false;
    return tableGet(&namespace->values, name, ptr);
}

ObjString* takeString(VM* vm, const char* src, int len) {
    uint32_t hash = hashString(src, len);
    ObjString* interned = tableFindString(&vm->strings, src, len, hash);
    if (interned != NULL) {
        FREE_ARRAY(vm, char, src, len + 1);
        return interned;
    }
    return allocateString(vm, src, len, hash);
}

ObjString* copyString(VM* vm, const char* src, int len) {
    uint32_t hash = hashString(src, len);

    ObjString* interned = tableFindString(&vm->strings, src, len, hash);
    if (interned != NULL) 
        return interned;

    char* newString = ALLOCATE(vm, char, len + 1);
    memcpy(newString, src, len);
    newString[len] = '\0';
    return allocateString(vm, newString, len, hash);
}

ObjString* formatString(VM* vm, const char* format, ...) {
    va_list args;
    va_start(args, format);

    int len = vsnprintf(NULL, 0, format, args);
    char* buf = ALLOCATE(vm, char, len + 1);
    if (buf == NULL) exit(1);
    vsnprintf(buf, len + 1, format, args);

    va_end(args);

    return takeString(vm, buf, len);
}

ObjUpvalue* newUpvalue(VM* vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NULL_VAL;
    return upvalue;
}

ObjLibrary* newLibrary(VM* vm, ObjString* name, ImportLibrary init) {
    ObjLibrary* library = ALLOCATE_OBJ(vm, ObjLibrary, OBJ_LIBRARY);

    library->imported = false;
    library->initializer = init;
    library->namespace = NULL;
    library->name = name;

    return library;
}

static ObjString* strFunction(VM* vm, ObjFunction* func) {
    if (func->name == NULL) {
        return formatString(vm, "<script>");
    }

    return formatString(vm, "<func %s>", func->name->chars);
}

static void printFunction(ObjFunction* func) {
    if (func->name == NULL) {
        printf("<script>");
        return;
    }

    printf("<func %s>", func->name->chars);
}

static ObjString* strInstance(VM* vm, ObjInstance* inst) {
    NativeResult res = callDefaultMethod(vm, inst, DEFMTH_STRING, NULL, 0);
    if (res.success) {
        return AS_STRING(res.val);
    }
    return formatString(vm, "<%p %s>", (void*) inst, inst->clazz->name->chars);
}

static void printInstance(ObjInstance* inst) {
    printf("<%p %s>", (void*) inst, inst->clazz->name->chars);
}

ObjString* strObject(VM* vm, Value val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING:
            return AS_STRING(val);
        case OBJ_FUNCTION:
            return strFunction(vm, AS_FUNCTION(val));
        case OBJ_NATIVE:
            return formatString(vm, "<native fn>");
        case OBJ_CLOSURE:
            return strFunction(vm, AS_CLOSURE(val)->function);
        case OBJ_BOUND_METHOD:
            return strFunction(vm, AS_BOUND_METHOD(val)->method->function);
        case OBJ_UPVALUE:
            return formatString(vm, "upvalue");
        case OBJ_CLASS:
            return formatString(vm, "<class %s>", AS_CLASS(val)->name->chars);
        case OBJ_INSTANCE:
            return strInstance(vm, AS_INSTANCE(val));
        case OBJ_LIST: {
            ObjList* list = AS_LIST(val);
            return formatString(vm, "[ %p (%d|%d) ]", list, 
                list->list.count, list->list.capacity);
        }
        case OBJ_NAMESPACE:
            return formatString(vm, "<namespace '%s'>", AS_NAMESPACE(val)->name->chars);
        case OBJ_LIBRARY:
            return formatString(vm, "<library '%s'>", AS_LIBRARY(val)->name->chars);
    }
    return formatString(vm, "undefined");
}

void printObject(Value val) {
    switch (OBJ_TYPE(val)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(val));
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(val));
            return;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(val)->function);
            break;
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(val)->method->function);
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
        case OBJ_CLASS:
            printf("<class %s>", AS_CLASS(val)->name->chars);
            break;
        case OBJ_INSTANCE:
            printInstance(AS_INSTANCE(val));
            break;
        case OBJ_LIST: {
            ObjList* list = AS_LIST(val);
            printf("[ %p (%d|%d) ]", list, list->list.count, list->list.capacity);
            break;
        }
        case OBJ_NAMESPACE:
            printf("<namespace %s>", AS_NAMESPACE(val)->name->chars);
            break;
        case OBJ_LIBRARY:
            printf("<library %s>", AS_LIBRARY(val)->name->chars);
            break;
        default:
            printf("undefined");
            break;
    }
}
