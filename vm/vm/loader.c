
#include "loader.h"

#include <stdbool.h>
#include <string.h>

#include "../util/memory.h"
#include "../compiler/dumper.h"

BytecodeLoader* newLoader(VM* vm, uint8_t* bytes, int length) {
    BytecodeLoader* loader = ALLOCATE(vm, BytecodeLoader, 1);

    loader->bytes = bytes;
    loader->length = length;
    loader->byte = loader->bytes[0];
    loader->idx = 0;

    loader->vm = vm;

    return loader;
}

static uint8_t advance(BytecodeLoader* loader) {
    #ifdef DEBUG_PRINT_LOADER
        printf("---- reading byte %d/%d (%04u)\n", loader->idx, loader->length, loader->bytes[loader->idx]);
    #endif
    if (loader->idx + 1 > loader->length) {
        fprintf(stderr, "Malformed bytecode, ran out of bytes.\n");
        exit(65);
    }

    uint8_t byte = loader->bytes[loader->idx++];
    if (loader->idx < loader->length)
        loader->byte = loader->bytes[loader->idx];
    return byte;
}

static int readInt(BytecodeLoader* loader) {
    return advance(loader) + 
        (advance(loader) << 8) +
        (advance(loader) << 16) +
        (advance(loader) << 24);
}

/*
static bool match(BytecodeLoader* loader, uint8_t byte) {
    if (loader->byte != byte)
        return false;
    advance(loader);
    return true;
}
*/

static void consume(BytecodeLoader* loader, uint8_t byte) {
    if (loader->byte != byte) {
        fprintf(stderr, "Malformed bytecode. Expected %04u, found %04u.\n", byte, loader->byte);
        exit(65);
    }

    advance(loader);
}

void freeLoader(VM* vm, BytecodeLoader* loader) {
    FREE_ARRAY(vm, uint8_t, loader->bytes, loader->length);
    FREE(vm, BytecodeLoader, loader);
}

static ObjFunction* readFunction(BytecodeLoader* loader);
static ObjString* readString(BytecodeLoader* loader);
static ObjNamespace* readNamespace(BytecodeLoader* loader);

static bool readBool(BytecodeLoader* loader) {
    consume(loader, DUMP_BOOL);
    return advance(loader) == 1;
}

static double readNumber(BytecodeLoader* loader) {
    consume(loader, DUMP_NUMBER);
    uint8_t byte_array[sizeof(double)];
    double num = 0;
    for (int i = 0; i < sizeof(double); i++)
        byte_array[i] = advance(loader);
    memcpy(&num, byte_array, sizeof(double));
    return num;
}

static Value readValue(BytecodeLoader* loader) {
    switch (loader->byte) {
        case DUMP_BOOL:
            return BOOL_VAL(readBool(loader));
        case DUMP_NUMBER:
            return NUMBER_VAL(readNumber(loader));
        case DUMP_STRING:
            return OBJ_VAL((Obj*) readString(loader));
        case DUMP_FUNC:
            return OBJ_VAL((Obj*) readFunction(loader));
        case DUMP_NAMESPACE:
            return OBJ_VAL((Obj*) readNamespace(loader));
        default:
            fprintf(stderr, "Malformed bytecode, expected type byte, got '%04u'.\n", 
                loader->byte);
            exit(65);
            break;
    }
}

static ValueArray readValueArray(BytecodeLoader* loader) {
    int count = readInt(loader);

    ValueArray array;
    initValueArray(&array);
    for (int i = 0; i < count; i++)
        writeValueArray(loader->vm, &array, readValue(loader));
    
    return array;
}

static Chunk readChunk(BytecodeLoader* loader) {
    #ifdef DEBUG_PRINT_LOADER
        printf("-- reading chunk\n");
    #endif
    consume(loader, DUMP_CHUNK);
    
    int lines_count = readInt(loader);
    int* lines = ALLOCATE(loader->vm, int, lines_count);
    int* lines_run = ALLOCATE(loader->vm, int, lines_count);
    for (int i = 0; i < lines_count; i++) {
        lines[i] = readInt(loader);
        lines_run[i] = readInt(loader);   
    }

    ValueArray array = readValueArray(loader);

    int count = readInt(loader);
    uint8_t* code = ALLOCATE(loader->vm, uint8_t, count);
    if (loader->idx + count > loader->length) {
        fprintf(stderr, "Malformed bytecode, expected %d bytes in chunk, got %d.\n", 
            count, loader->length - loader->idx);
        exit(65);
    }

    memcpy(code, loader->bytes + loader->idx, count);
    loader->idx += count - 1;
    advance(loader);

    Chunk chunk;
    initChunk(&chunk);
 
    chunk.count = count;
    chunk.capacity = count;
    chunk.code = code;

    chunk.constants = array;

    chunk.lines_count = lines_count;
    chunk.lines_capacity = lines_count;
    chunk.lines = lines;
    chunk.lines_run = lines_run;

    return chunk;
}

static ObjString* readString(BytecodeLoader* loader) {
    #ifdef DEBUG_PRINT_LOADER
        printf("-- reading string\n");
    #endif
    consume(loader, DUMP_STRING);

    int length = readInt(loader);
    char* src = ALLOCATE(loader->vm, char, length);
    if (loader->idx + length > loader->length) {
        fprintf(stderr, "Malformed bytecode, expected %d bytes in string, got %d.\n", 
            length, loader->length - loader->idx);
        exit(65);
    }

    // 1 2 3 4 5 6 
    //       ^
    // idx : 3
    // len : 6
    // 

    memcpy(src, loader->bytes + loader->idx, length);
    loader->idx += length - 1;
    advance(loader);

    #ifdef DEBUG_PRINT_LOADER
        printf("-- read string '%s'\n", src);
    #endif

    return takeString(loader->vm, src, length);
}

static ObjFunction* readFunction(BytecodeLoader* loader) {
    #ifdef DEBUG_PRINT_LOADER
        printf("-- reading function\n");
    #endif
    consume(loader, DUMP_FUNC);
    
    #ifdef DEBUG_PRINT_LOADER
        printf("--- reading arity\n");
    #endif
    uint8_t arity = advance(loader);

    ObjString* name = NULL;
    if (loader->byte == DUMP_NULL) 
        advance(loader);
    else
        name = readString(loader);
    
    int upvalues = advance(loader);
    Chunk chunk = readChunk(loader);

    ObjFunction* func = newFunction(loader->vm);

    func->name = name;
    func->arity = arity;
    func->chunk = chunk;
    func->upvalueCount = upvalues;

    return func;
}

static ObjNamespace* readNamespace(BytecodeLoader* loader) {
    consume(loader, DUMP_NAMESPACE);

    ObjString* name = readString(loader);
    ObjNamespace* nspace = newNamespace(loader->vm, name);

    int length = readInt(loader);
    printf("len: %d\n", length);
    for (int i = 0; i < length; i++) {
        ObjString* key = readString(loader);
        Value val = readValue(loader);
        bool public = advance(loader) == 1;
        
        writeNamespace(loader->vm, nspace, key, val, public);
    }

    return nspace;
}

ObjFunction* readBytecode(BytecodeLoader* loader) {
    #ifdef DEBUG_PRINT_LOADER
        printf("-- reading bytecode\n");
    #endif
    return readFunction(loader);
}
