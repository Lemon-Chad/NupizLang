
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
#define IS_NAMESPACE(val) isObjType(val, OBJ_NAMESPACE)
#define IS_LIBRARY(val) isObjType(val, OBJ_LIBRARY)
#define IS_ATTRIBUTE(val) isObjType(val, OBJ_ATTRIBUTE)
#define IS_PTR(val) isObjType(val, OBJ_PTR)

#define AS_STRING(val) ((ObjString*) AS_OBJ(val))
#define AS_CSTRING(val) (((ObjString*) AS_OBJ(val))->chars)
#define AS_NATIVE(val) (((ObjNative*) AS_OBJ(val))->function)
#define AS_FUNCTION(val) ((ObjFunction*) AS_OBJ(val))
#define AS_CLOSURE(val) ((ObjClosure*) AS_OBJ(val))
#define AS_CLASS(val) ((ObjClass*) AS_OBJ(val))
#define AS_INSTANCE(val) ((ObjInstance*) AS_OBJ(val))
#define AS_BOUND_METHOD(val) ((ObjBoundMethod*) AS_OBJ(val))
#define AS_LIST(val) ((ObjList*) AS_OBJ(val))
#define AS_NAMESPACE(val) ((ObjNamespace*) AS_OBJ(val))
#define AS_LIBRARY(val) ((ObjLibrary*) AS_OBJ(val))
#define AS_ATTRIBUTE(val) ((ObjAttribute*) AS_OBJ(val))
#define AS_PTR(val) ((ObjPtr*) AS_OBJ(val))

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
    OBJ_NAMESPACE,
    OBJ_LIBRARY,
    OBJ_ATTRIBUTE,
    OBJ_PTR,
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

struct ObjPtr {
    Obj obj;
    char* origin;
    int typeEncoding;
    void* ptr;
    PtrFreeFunc freeFn;
    PtrBlackenFunc blackenFn;
    PtrPrintFunc printFn;
    PtrStringFunc stringFn;
    PtrHashFunc hashFn;
};

#define DEFAULT_METHOD_COUNT 3

typedef enum {
    DEFMTH_STRING,
    DEFMTH_EQ,
    DEFMTH_HASH,
} DefaultMethods;

struct ObjClass {
    Obj obj;
    ObjString* name;
    ObjClosure* constructor;
    Table methods;
    Table fields;
    Table staticFields;
    ObjClosure* defaultMethods[DEFAULT_METHOD_COUNT];
    Value bound;
};

struct ObjInstance {
    Obj obj;
    ObjClass* clazz;
    Table fields;
    Value bound;
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

struct ObjNamespace {
    Obj obj;
    ObjString* name;
    Table publics;
    Table values;
};

struct ObjLibrary {
    Obj obj;
    ObjString* name;
    ObjNamespace* nspace;
    ImportLibrary initializer;
    bool imported;
};

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

struct ObjAttribute {
    Obj obj;
    Value val;
    bool isPublic;
    bool isStatic;
    bool isConstant;
};

ObjClosure* newClosure(VM* vm, ObjFunction* func);
ObjFunction* newFunction(VM* vm);
ObjNative* newNative(VM* vm, NativeFn func);
ObjClass* newClass(VM* vm, ObjString* name);
ObjInstance* newInstance(VM* vm, ObjClass* clazz);
ObjBoundMethod* newBoundMethod(VM* vm, Value reciever, ObjClosure* method);
ObjList* newList(VM* vm);
ObjNamespace* newNamespace(VM* vm, ObjString* name);
ObjLibrary* newLibrary(VM* vm, ObjString* name, ImportLibrary init);
ObjPtr* newPtr(VM* vm, const char* origin, int typeEncoding);

bool writeNamespace(VM* vm, ObjNamespace* nspace, ObjString* name, Value val, bool isPublic);
bool getNamespace(VM* vm, ObjNamespace* nspace, ObjString* name, Value* ptr, bool internal);

ObjAttribute* newAttribute(VM* vm, Value val, bool isPublic, bool isStatic, bool isConstant);
ObjAttribute* copyAttribute(VM* vm, Value attr);

bool declareClassField(VM* vm, ObjClass* clazz, ObjString* name, Value val,
        bool isPublic, bool isStatic, bool isConstant);
bool declareClassMethod(VM* vm, ObjClass* clazz, ObjString* name, Value val, bool isPublic, bool isStatic);

bool setClassField(VM* vm, ObjClass* clazz, ObjString* name, Value val, bool internal);
bool getClassField(VM* vm, ObjClass* clazz, ObjString* name, Value* ptr, bool internal);
bool getClassMethod(VM* vm, ObjClass* clazz, ObjString* name, Value* ptr, bool internal);
bool getInstanceClassMethod(VM* vm, ObjClass* clazz, ObjString* name, Value* ptr, bool internal);

bool setInstanceField(VM* vm, ObjInstance* inst, ObjString* name, Value val, bool internal);
bool getInstanceField(VM* vm, ObjInstance* inst, ObjString* name, Value* ptr, bool internal);
bool getInstanceMethod(VM* vm, ObjInstance* inst, ObjString* name, Value* ptr, bool internal);

bool hasClassField(VM* vm, ObjClass* clazz, ObjString* name, bool internal);
bool hasClassMethod(VM* vm, ObjClass* clazz, ObjString* name, bool internal);
bool hasInstanceClassMethod(VM* vm, ObjClass* clazz, ObjString* name, bool internal);
bool hasInstanceField(VM* vm, ObjInstance* inst, ObjString* name, bool internal);
bool hasInstanceMethod(VM* vm, ObjInstance* inst, ObjString* name, bool internal);

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
