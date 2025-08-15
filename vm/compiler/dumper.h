
#ifndef jp_dumper_h
#define jp_dumper_h

#include <stdio.h>
#include <stdlib.h>

#include "../util/table.h"
#include "../vm/value.h"

typedef enum {
    DUMP_NULL,
    DUMP_NUMBER,
    DUMP_BOOL,
    DUMP_STRING,
    DUMP_FUNC,
    DUMP_CHUNK,
    DUMP_NAMESPACE,
} DumpCode;

struct DumpedBytes {
    uint8_t* bytes;
    int count;
    int capacity;
};

DumpedBytes* newDumpedBytes(VM* vm);
void freeDumpedBytes(VM* vm, DumpedBytes* bytes);

void writeByte(VM* vm, DumpedBytes* bytes, uint8_t byte);
void writeBytes(VM* vm, DumpedBytes* dest, DumpedBytes* src);
void takeBytes(VM* vm, DumpedBytes* dest, DumpedBytes* src);
void printBytes(DumpedBytes* bytes);

bool dumpBytes(FILE* fp, DumpedBytes* bytes);

DumpedBytes* dumpFunction(VM* vm, ObjFunction* func);
DumpedBytes* dumpChunk(VM* vm, Chunk* chunk);
DumpedBytes* dumpValueArray(VM* vm, ValueArray* array);
DumpedBytes* dumpValue(VM* vm, Value val);
DumpedBytes* dumpTable(VM* vm, Table* tb);

#endif
