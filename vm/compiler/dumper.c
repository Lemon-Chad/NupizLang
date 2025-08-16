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
            #ifdef DEBUG_PRINT_DUMPER
                printf("-- writing string '%s'\n", str->chars);
            #endif
            writeByte(vm, bytes, DUMP_STRING);
            writeInt(vm, bytes, str->length);
            for (int i = 0; i < str->length; i++)
                writeByte(vm, bytes, str->chars[i]);
            break;
        }

        case OBJ_FUNCTION:
            takeBytes(vm, bytes, dumpFunction(vm, (ObjFunction*) obj));
            break;
        
        case OBJ_UPVALUE:
            takeBytes(vm, bytes, dumpValue(vm, ((ObjUpvalue*) obj)->closed));
            break;
        
        case OBJ_NAMESPACE: {
            ObjNamespace* nspace = (ObjNamespace*) obj;
            writeByte(vm, bytes, DUMP_NAMESPACE);
            writeObject(vm, bytes, (Obj*) nspace->name);
            writeInt(vm, bytes, nspace->values->count);
            for (int i = 0; i < nspace->values->capacity; i++) {
                Entry* entry = &nspace->values->entries[i];
                if (entry->key == NULL)
                    continue;
                
                writeObject(vm, bytes, (Obj*) entry->key);
                takeBytes(vm, bytes, dumpValue(vm, entry->value));
                writeByte(vm, bytes, 
                    tableGet(nspace->publics, entry->key, NULL) ? 1 : 0);
            }
            break;
        }
        
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

    #ifdef DEBUG_PRINT_DUMPER
        printf("    -- writing function '%s'\n", func->name->chars);
    #endif

    writeByte(vm, bytes, DUMP_FUNC);
    writeByte(vm, bytes, func->arity);
    if (func->name == NULL)
        writeByte(vm, bytes, DUMP_NULL);
    else
        writeObject(vm, bytes, (Obj*) func->name);
    
    writeByte(vm, bytes, func->upvalueCount);
    
    takeBytes(vm, bytes, dumpChunk(vm, &func->chunk));

    #ifdef DEBUG_PRINT_DUMPER
        printf("    -- wrote function '%s'\n", func->name->chars);
    #endif

    return bytes;
}

DumpedBytes* dumpChunk(VM* vm, Chunk* chunk) {
    DumpedBytes* bytes = newDumpedBytes(vm);

    #ifdef DEBUG_PRINT_DUMPER
        printf("-- writing chunk\n");
    #endif

    writeByte(vm, bytes, DUMP_CHUNK);

    writeInt(vm, bytes, chunk->lines_count);
    for (int i = 0; i < chunk->lines_count; i++) {
        writeInt(vm, bytes, chunk->lines[i]);
        writeInt(vm, bytes, chunk->lines_run[i]);
    }

    takeBytes(vm, bytes, dumpValueArray(vm, &chunk->constants));

    writeInt(vm, bytes, chunk->count);
    for (int i = 0; i < chunk->count; i++) {
        writeByte(vm, bytes, chunk->code[i]);
    }
    
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
            #ifdef DEBUG_PRINT_DUMPER
                printf("-- writing bool '%s'\n", AS_BOOL(val) ? "true" : "false");
            #endif
            writeByte(vm, bytes, DUMP_BOOL);
            writeByte(vm, bytes, AS_BOOL(val) ? 1 : 0);
            break;
        
        case VAL_NUMBER: {
            #ifdef DEBUG_PRINT_DUMPER
                printf("-- writing number '%f'\n", AS_NUMBER(val));
            #endif
            writeByte(vm, bytes, DUMP_NUMBER);

            uint8_t byte_array[sizeof(double)];
            double num = AS_NUMBER(val);
            memcpy(byte_array, &num, sizeof(double));
            for (int i = 0; i < sizeof(double); i++)
                writeByte(vm, bytes, byte_array[i]);
            
            break;
        }

        case VAL_NULL:
            #ifdef DEBUG_PRINT_DUMPER
                printf("-- writing null\n");
            #endif
            writeByte(vm, bytes, DUMP_NULL);
            break;
        
        case VAL_OBJ:
            writeObject(vm, bytes, AS_OBJ(val));
            break;
    }

    return bytes;
}

DumpedBytes* dumpTable(VM* vm, Table* tb) {
    DumpedBytes* bytes = newDumpedBytes(vm);

    writeInt(vm, bytes, tb->count);
    for (int i = 0; i < tb->capacity; i++) {
        if (tb->entries[i].key == NULL) continue;
        writeObject(vm, bytes, (Obj*) tb->entries[i].key);
        takeBytes(vm, bytes, dumpValue(vm, tb->entries[i].value));
    }

    return bytes;
}
