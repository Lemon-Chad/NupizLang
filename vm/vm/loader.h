
#ifndef jp_loader
#define jp_loader

#include <stdio.h>
#include <stdlib.h>

#include "../vm/value.h"

struct BytecodeLoader {
    uint8_t* bytes;
    uint8_t byte;
    int idx;
    int length;
    
    VM* vm;
};

BytecodeLoader* newLoader(VM* vm, uint8_t* bytes, int length);
void freeLoader(VM* vm, BytecodeLoader* loader);

ObjFunction* readBytecode(BytecodeLoader* loader);

#endif
