#include <string.h>

#include <stdio.h>
#include "dumper.h"
#include "../util/memory.h"

DumpedBytes* newDumpedBytes(VM* vm) {
    DumpedBytes* bytes = ALLOCATE(vm, DumpedBytes, 1);

    bytes->bytes = NULL;
    bytes->count = 0;
    bytes->capacity = 0;

    return bytes;
}

void freeDumpedBytes(VM* vm, DumpedBytes* bytes) {
    FREE_ARRAY(vm, uint8_t, bytes->bytes, bytes->capacity);
    FREE(vm, DumpedBytes, bytes);
}

void writeByte(VM* vm, DumpedBytes* bytes, uint8_t byte) {
    if (bytes->capacity < bytes->count + 1) {
        int oldCapacity = bytes->capacity;
        bytes->capacity = GROW_CAPACITY(oldCapacity);
        bytes->bytes = GROW_ARRAY(vm, uint8_t, bytes->bytes, oldCapacity, bytes->capacity);
    }

    bytes->bytes[bytes->count++] = byte;
}

static void writeInt(VM* vm, DumpedBytes* bytes, int i) {
    writeByte(vm, bytes, i & 0xFF);
    writeByte(vm, bytes, (i >> 8) & 0xFF);
    writeByte(vm, bytes, (i >> 16) & 0xFF);
    writeByte(vm, bytes, (i >> 24) & 0xFF);
}

static void writeObject(VM* vm, DumpedBytes* bytes, Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* str = (ObjString*) obj;
            writeByte(vm, bytes, DUMP_STRING);
            writeInt(vm, bytes, str->length);
            for (int i = 0; i < str->length; i++)
                writeByte(vm, bytes, str->chars[i]);
            break;
        }

        case OBJ_FUNCTION:
            takeBytes(vm, bytes, dumpFunction(vm, (ObjFunction*) obj));
            break;
        
        default:
            fprintf(stderr, "Unhandled type '%d'.\n", obj->type);
            exit(2);
            break;
    }
}

void writeBytes(VM* vm, DumpedBytes* dest, DumpedBytes* src) {
    while (dest->capacity < dest->count + src->count) {
        int oldCapacity = dest->capacity;
        dest->capacity = GROW_CAPACITY(oldCapacity);
        dest->bytes = GROW_ARRAY(vm, uint8_t, dest->bytes, oldCapacity, dest->capacity);
    }

    memcpy(dest->bytes + dest->count, src->bytes, src->count);
    dest->count += src->count;
}

void takeBytes(VM* vm, DumpedBytes* dest, DumpedBytes* src) {
    writeBytes(vm, dest, src);
    freeDumpedBytes(vm, src);
}

void printBytes(DumpedBytes* bytes) {
    for (int i = 0; i < bytes->count; i++) {
        printf("%04u", bytes->bytes[i]);
        if (i < bytes->count - 1) printf(" ");
    }
}

bool dumpBytes(FILE* fp, DumpedBytes* bytes) {
    return bytes->count == fwrite(bytes->bytes, sizeof(uint8_t), bytes->count, fp);
}

DumpedBytes* dumpFunction(VM* vm, ObjFunction* func) {
    DumpedBytes* bytes = newDumpedBytes(vm);

    writeByte(vm, bytes, DUMP_FUNC);
    writeByte(vm, bytes, func->arity);
    if (func->name == NULL)
        writeByte(vm, bytes, DUMP_NULL);
    else
        writeObject(vm, bytes, (Obj*) func->name);
    
    writeByte(vm, bytes, func->upvalueCount);
    
    takeBytes(vm, bytes, dumpChunk(vm, &func->chunk));

    return bytes;
}

DumpedBytes* dumpChunk(VM* vm, Chunk* chunk) {
    DumpedBytes* bytes = newDumpedBytes(vm);

    writeByte(vm, bytes, DUMP_CHUNK);

    writeInt(vm, bytes, chunk->lines_count);
    for (int i = 0; i < chunk->lines_count; i++) {
        writeInt(vm, bytes, chunk->lines[i]);
        writeInt(vm, bytes, chunk->lines_run[i]);
    }

    takeBytes(vm, bytes, dumpValueArray(vm, &chunk->constants));

    writeInt(vm, bytes, chunk->count);
    for (int i = 0; i < chunk->count; i++)
        writeByte(vm, bytes, chunk->code[i]);
    
    return bytes;
}

DumpedBytes* dumpValueArray(VM* vm, ValueArray* array) {
    DumpedBytes* bytes = newDumpedBytes(vm);

    writeInt(vm, bytes, array->count);
    for (int i = 0; i < array->count; i++)
        takeBytes(vm, bytes, dumpValue(vm, array->values[i]));
    
    return bytes;
}

DumpedBytes* dumpValue(VM* vm, Value val) {
    DumpedBytes* bytes = newDumpedBytes(vm);

    switch (val.type) {
        case VAL_BOOL:
            writeByte(vm, bytes, DUMP_BOOL);
            writeByte(vm, bytes, AS_BOOL(val) ? 1 : 0);
            break;
        
        case VAL_NUMBER: {
            writeByte(vm, bytes, DUMP_NUMBER);

            uint8_t byte_array[sizeof(double)];
            double num = AS_NUMBER(val);
            memcpy(byte_array, &num, sizeof(double));
            for (int i = 0; i < sizeof(double); i++)
                writeByte(vm, bytes, byte_array[i]);
            
            break;
        }

        case VAL_NULL:
            writeByte(vm, bytes, DUMP_NULL);
            break;
        
        case VAL_OBJ:
            writeObject(vm, bytes, AS_OBJ(val));
            break;
    }

    return bytes;
}
