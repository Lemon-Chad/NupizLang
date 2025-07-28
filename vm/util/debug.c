#include <stdio.h>

#include "debug.h"
#include "table.h"
#include "../vm/object.h"
#include "../vm/value.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int offset = 0; offset < chunk->count;) {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int simpleInstruction(const char* name, int offset) {
    printf("%s\n", name);
    return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int longConstantInstruction(const char* name, Chunk* chunk, int offset) {
    int constant = chunk->code[offset + 1] + (chunk->code[offset + 2] << 8) + (chunk->code[offset + 3] << 16);
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(const char* name, int sign, Chunk* chunk, int offset) {
    uint16_t jump = (uint16_t) (chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];

    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int invokeInstruction(const char* name, Chunk* chunk, int offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

int disassembleInstruction(Chunk* chunk, int offset) {
    printf("%04d ", offset);
    if (offset > 0 && getLine(chunk, offset) == getLine(chunk, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", getLine(chunk, offset));
    }

    #define INST(inst, type) case type: return inst(#type, chunk, offset)
    #define CONST_INST(type) INST(constantInstruction, type)
    #define BYTE_INST(type) INST(byteInstruction, type)
    #define INVOKE_INST(type) INST(invokeInstruction, type)
    #define JUMP_INST(type, sign) case type: return jumpInstruction(#type, sign, chunk, offset)
    #define SIMPLE_INST(type) case type: return simpleInstruction(#type, offset)

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        CONST_INST(OP_CONSTANT);

        case OP_CONSTANT_LONG:
            return longConstantInstruction("OP_LONG_CONSTANT", chunk, offset);
        
        CONST_INST(OP_DEFINE_GLOBAL);
        CONST_INST(OP_GET_GLOBAL);
        CONST_INST(OP_SET_GLOBAL);

        CONST_INST(OP_CLASS);

        BYTE_INST(OP_GET_LOCAL);
        BYTE_INST(OP_SET_LOCAL);
        
        JUMP_INST(OP_LOOP, -1);
        JUMP_INST(OP_JUMP, 1);
        JUMP_INST(OP_JUMP_IF_FALSE, 1);
        JUMP_INST(OP_JUMP_IF_TRUE, 1);
        
        BYTE_INST(OP_CALL);
        
        SIMPLE_INST(OP_CLOSE_UPVALUE);
        SIMPLE_INST(OP_NOT);
        SIMPLE_INST(OP_NEGATE);
        SIMPLE_INST(OP_ADD);
        SIMPLE_INST(OP_SUBTRACT);
        SIMPLE_INST(OP_MULTIPLY);
        SIMPLE_INST(OP_DIVIDE);
        SIMPLE_INST(OP_EQUAL);
        SIMPLE_INST(OP_NOT_EQUAL);
        SIMPLE_INST(OP_GREATER);
        SIMPLE_INST(OP_GREATER_EQUAL);
        SIMPLE_INST(OP_LESS);
        SIMPLE_INST(OP_LESS_EQUAL);
        SIMPLE_INST(OP_RETURN);
        SIMPLE_INST(OP_TRUE);
        SIMPLE_INST(OP_FALSE);
        SIMPLE_INST(OP_NULL);
        SIMPLE_INST(OP_INHERIT);
        SIMPLE_INST(OP_POP);
        SIMPLE_INST(OP_GET_INDEX);
        SIMPLE_INST(OP_SET_INDEX);
        BYTE_INST(OP_POP_N);
        BYTE_INST(OP_MAKE_LIST);

        CONST_INST(OP_GET_PROPERTY);
        CONST_INST(OP_SET_PROPERTY);
        CONST_INST(OP_GET_SUPER);

        case OP_METHOD: {
            int methodType = chunk->code[offset++];
            if (methodType == 1) {
                printf("%-16s INITIALIZER\n", "OP_METHOD");
            } else if (methodType == 2) {
                uint8_t defMethodIdx = chunk->code[offset++];
                printf("%-16s %4d DEFAULT\n", "OP_METHOD", defMethodIdx);
            } else {
                uint8_t constant = chunk->code[offset++];
                printf("%-16s %4d '", "OP_METHOD", constant);
                printValue(chunk->constants.values[constant]);
                printf("'\n");
            }
            return offset;
        }

        INVOKE_INST(OP_INVOKE);
        INVOKE_INST(OP_SUPER_INVOKE);

        case OP_CLOSURE: {
            offset++;

            bool isLong = chunk->code[offset++] == OP_CONSTANT_LONG;
            uint8_t constant = chunk->code[offset++];
            if (isLong) {
                constant += (chunk->code[offset + 1] << 8) + (chunk->code[offset + 2] << 16);
                offset += 2;
            }

            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");

            ObjFunction* func = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < func->upvalueCount; j++) {
                int isLocal = chunk->code[offset++];
                int idx = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", 
                    offset - 2, isLocal ? "local" : "upvalue", idx);
            }

            return offset;
        }

        BYTE_INST(OP_GET_UPVALUE);
        BYTE_INST(OP_SET_UPVALUE);
        
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }

    #undef INST
    #undef CONST_INST
    #undef BYTE_INST
    #undef JUMP_INST
    #undef SIMPLE_INST
}
