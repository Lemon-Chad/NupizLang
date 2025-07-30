
#ifndef jp_vm_h
#define jp_vm_h

#include "../compiler/chunk.h"
#include "../compiler/compiler.h"
#include "object.h"
#include "../util/table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct ObjFunction ObjFunction;

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
    Value bound;
} CallFrame;

typedef struct {
    bool success;
    Value val;
} NativeResult;

#define NATIVE_OK(val) ((NativeResult) { true, val })
#define NATIVE_FAIL ((NativeResult) { false, NULL_VAL })

struct VM {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    Obj* objects;

    ObjUpvalue* openUpvalues;

    int grayCount;
    int grayCapacity;
    Obj** grayStack;

    size_t bytesAllocated;
    size_t nextGC;

    Compiler* compiler;

    bool safeMode;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);

InterpretResult runFuncBound(VM* vm, ObjFunction* func, Value binder);
InterpretResult runFunc(VM* vm, ObjFunction* func);
InterpretResult interpret(VM* vm, const char* src);
void push(VM* vm, Value value);
Value pop(VM* vm);
void popn(VM* vm, int n);
InterpretResult run(VM* vm);

NativeResult callDefaultMethod(VM* vm, ObjInstance* inst, int idx, Value* args, int argc);

#endif
