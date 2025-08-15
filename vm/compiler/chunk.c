#include <stdio.h>

#include "chunk.h"
#include "../util/memory.h"

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;

    chunk->lines = NULL;
    chunk->lines_run = NULL;
    chunk->lines_count = 0;
    chunk->lines_capacity = 0;

    initValueArray(&chunk->constants);
}

void writeConstant(VM* vm, Chunk* chunk, Value value, int line) {
    int constant = addConstant(vm, chunk, value);

    if (constant >= 256) {
        writeChunk(vm, chunk, OP_CONSTANT_LONG, line);
        writeChunk(vm, chunk, constant, line);
        writeChunk(vm, chunk, constant >> 8, line);
        writeChunk(vm, chunk, constant >> 16, line);
    } else {
        writeChunk(vm, chunk, OP_CONSTANT, line);
        writeChunk(vm, chunk, constant, line);
    }
}

void writeChunk(VM* vm, Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        
        chunk->code = GROW_ARRAY(vm, uint8_t, chunk->code, oldCapacity, chunk->capacity);
    }

    if (chunk->lines != NULL && chunk->lines[chunk->lines_count - 1] == line) {
        chunk->lines_run[chunk->lines_count - 1]++;
    } else {
        if (chunk->lines_capacity < chunk->lines_count + 1) {
            int oldCapacity = chunk->lines_capacity;
            chunk->lines_capacity = GROW_CAPACITY(oldCapacity);
            chunk->lines = GROW_ARRAY(vm, int, chunk->lines, oldCapacity, chunk->capacity);
            chunk->lines_run = GROW_ARRAY(vm, int, chunk->lines_run, oldCapacity, chunk->capacity);
        }

        chunk->lines[chunk->lines_count] = line;
        chunk->lines_run[chunk->lines_count] = 1;
        chunk->lines_count++;
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;
}

void freeChunk(VM* vm, Chunk* chunk) {
    FREE_ARRAY(vm, uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(vm, int, chunk->lines, chunk->capacity);
    FREE_ARRAY(vm, int, chunk->lines_run, chunk->capacity);
    initChunk(chunk);
    freeValueArray(vm, &chunk->constants);
}

int addConstant(VM* vm, Chunk* chunk, Value value) {
    for (int i = 0; i < chunk->constants.count; i++) {
        if (valuesEqual(vm, chunk->constants.values[i], value))
            return i;
    }

    push(vm, value);
    writeValueArray(vm, &chunk->constants, value);
    pop(vm);
    
    return chunk->constants.count - 1;
}

int getLine(Chunk* chunk, int offset) {
    int idx = 0;
    while (idx < chunk->lines_count && offset >= chunk->lines_run[idx]) {
        offset -= chunk->lines_run[idx];
        idx++;
    }

    if (idx >= chunk->lines_count) {
        return -1;
    }
    return chunk->lines[idx];
}
