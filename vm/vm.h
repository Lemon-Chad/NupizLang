
#ifndef jp_vm_h
#define jp_vm_h

#include "chunk.h"
#include "compiler.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct ObjFunction ObjFunction;

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

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
};

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERR,
    INTERPRET_RUNTIME_ERR,
} InterpretResult;

void initVM(VM* vm);
void freeVM(VM* vm);

InterpretResult interpret(VM* vm, const char* src);
void push(VM* vm, Value value);
Value pop(VM* vm);
void popn(VM* vm, int n);

#endif
