
#ifndef jp_vm_h
#define jp_vm_h

#include "../compiler/chunk.h"
#include "../compiler/compiler.h"
#include "object.h"
#include "../util/table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
    Value bound;
} CallFrame;

struct NativeResult {
    bool success;
    Value val;
};

#define NATIVE_VAL(val) ((NativeResult) { true, val })
#define NATIVE_OK (NATIVE_VAL(NULL_VAL))
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

    Table libraries;

    int safeMode;
    int pauseGC;
    int keepTop;

    const char** argv;
    int argc;
    bool isMain;
    ObjFunction* mainFunc;
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);
void decoupleVM(VM* vm);

InterpretResult runFuncBound(VM* vm, ObjFunction* func, Value binder);
InterpretResult runFunc(VM* vm, ObjFunction* func);
InterpretResult interpret(VM* vm, const char* src);
void push(VM* vm, Value value);
Value pop(VM* vm);
void popn(VM* vm, int n);
InterpretResult run(VM* vm);

void runtimeError(VM* vm, const char* format, ...);

void callFunc(VM* vm, ObjClosure* clos, int argc, Value binder);
NativeResult callDefaultMethod(VM* vm, ObjInstance* inst, int idx, Value* args, int argc);

#endif
