
#ifndef jp_object_h
#define jp_object_h

#include "../util/common.h"
#include "../compiler/chunk.h"
#include "../util/table.h"
#include "value.h"
#include "vm.h"

#define OBJ_TYPE(val) (AS_OBJ(val)->type)

#define IS_STRING(val) isObjType(val, OBJ_STRING)
#define IS_FUNCTION(val) isObjType(val, OBJ_FUNCTION)
#define IS_NATIVE(val) isObjType(val, OBJ_NATIVE)
#define IS_CLOSURE(val) isObjType(val, OBJ_CLOSURE)
#define IS_CLASS(val) isObjType(val, OBJ_CLASS)
#define IS_INSTANCE(val) isObjType(val, OBJ_INSTANCE)
#define IS_BOUND_METHOD(val) isObjType(val, OBJ_BOUND_METHOD)
#define IS_LIST(val) isObjType(val, OBJ_LIST)

#define AS_STRING(val) ((ObjString*) AS_OBJ(val))
#define AS_CSTRING(val) (((ObjString*) AS_OBJ(val))->chars)
#define AS_NATIVE(val) (((ObjNative*) AS_OBJ(val))->function)
#define AS_FUNCTION(val) ((ObjFunction*) AS_OBJ(val))
#define AS_CLOSURE(val) ((ObjClosure*) AS_OBJ(val))
#define AS_CLASS(val) ((ObjClass*) AS_OBJ(val))
#define AS_INSTANCE(val) ((ObjInstance*) AS_OBJ(val))
#define AS_BOUND_METHOD(val) ((ObjBoundMethod*) AS_OBJ(val))
#define AS_LIST(val) ((ObjList*) AS_OBJ(val))

#define DEFAULT_METHOD_COUNT 1

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_LIST,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
    bool isMarked;
};

struct ObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

struct ObjFunction {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
};

struct ObjUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
};

struct ObjClosure {
    Obj obj;
    ObjFunction* function;

    ObjUpvalue** upvalues;
    int upvalueCount;
};

struct ObjClass {
    Obj obj;
    ObjString* name;
    ObjClosure* constructor;
    Table methods;
    ObjClosure* defaultMethods[DEFAULT_METHOD_COUNT];
};

typedef enum {
    DEFMTH_STRING,
} DefaultMethods;

struct ObjInstance {
    Obj obj;
    ObjClass* clazz;
    Table fields;
};

struct ObjBoundMethod {
    Obj obj;
    Value reciever;
    ObjClosure* method;
};

struct ObjList {
    Obj obj;
    ValueArray list;
};

typedef Value (*NativeFn)(VM* vm, int argc, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

ObjClosure* newClosure(VM* vm, ObjFunction* func);
ObjFunction* newFunction(VM* vm);
ObjNative* newNative(VM* vm, NativeFn func);
ObjClass* newClass(VM* vm, ObjString* name);
ObjInstance* newInstance(VM* vm, ObjClass* clazz);
ObjBoundMethod* newBoundMethod(VM* vm, Value reciever, ObjClosure* method);
ObjList* newList(VM* vm);

static inline bool isObjType(Value val, ObjType type) {
    return IS_OBJ(val) && AS_OBJ(val)->type == type;
}

ObjString* takeString(VM* vm, const char* src, int len);
ObjString* copyString(VM* vm, const char* src, int len);
ObjString* formatString(VM* vm, const char* format, ...);
ObjUpvalue* newUpvalue(VM* vm, Value* slot);
ObjString* strObject(VM* vm, Value val);
void printObject(Value val);

#endif
