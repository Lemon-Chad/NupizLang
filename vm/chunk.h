
#ifndef jp_chunk_h
#define jp_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_NULL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_SET_UPVALUE,
    OP_GET_UPVALUE,
    OP_LOOP,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_TRUE,
    OP_FALSE,
    OP_NOT,
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_NEGATE,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_RETURN,
    OP_POP,
    OP_POP_N,
    OP_CLOSE_UPVALUE,
    OP_CALL,
    OP_CLOSURE,
    OP_CLASS,
    OP_METHOD,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_INVOKE,
    OP_INHERIT,
    OP_GET_SUPER,
    OP_SUPER_INVOKE,
} OpCode;

struct Chunk {
    int count;
    int capacity;
    uint8_t* code;
    int* lines;
    int* lines_run;
    int lines_count;
    int lines_capacity;
    ValueArray constants;
};

void initChunk(Chunk* chunk);
void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line);
void writeConstant(VM* vm, Chunk* chunk, Value value, int line);
void freeChunk(VM* vm, Chunk* chunk);
int addConstant(VM* vm, Chunk* chunk, Value value);
int getLine(Chunk* chunk, int offset);

#endif
