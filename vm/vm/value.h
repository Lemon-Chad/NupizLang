
#ifndef jp_value_h
#define jp_value_h

#include <stdbool.h>

typedef int int32_t;

typedef struct Obj Obj;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;
typedef struct ObjClosure ObjClosure;
typedef struct ObjUpvalue ObjUpvalue;
typedef struct ObjClass ObjClass;
typedef struct ObjInstance ObjInstance;
typedef struct ObjBoundMethod ObjBoundMethod;
typedef struct ObjList ObjList;

typedef struct VM VM;
typedef struct Chunk Chunk;
typedef struct Compiler Compiler;
typedef struct ClassCompiler ClassCompiler;

typedef struct DumpedBytes DumpedBytes;
typedef struct BytecodeLoader BytecodeLoader;

typedef enum {
    VAL_BOOL,
    VAL_NULL, 
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define BOOL_VAL(val) ((Value) { VAL_BOOL, { .boolean = val }})
#define NULL_VAL ((Value) { VAL_NULL, { .number = 0 }})
#define NUMBER_VAL(val) ((Value) { VAL_NUMBER, { .number = val }})
#define OBJ_VAL(val) ((Value) { VAL_OBJ, { .obj = (Obj*) val }})

#define AS_BOOL(val) ((val).as.boolean)
#define AS_NUMBER(val) ((val).as.number)
#define AS_OBJ(val) ((val).as.obj)

#define IS_BOOL(val) ((val).type == VAL_BOOL)
#define IS_NULL(val) ((val).type == VAL_NULL)
#define IS_NUMBER(val) ((val).type == VAL_NUMBER)
#define IS_OBJ(val) ((val).type == VAL_OBJ)

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(VM* vm, ValueArray* array, Value value);
void freeValueArray(VM* vm, ValueArray* array);
void printValue(Value value);
ObjString* strValue(VM* vm, Value value);
bool valuesEqual(Value a, Value b);

#endif
