
#include "filelib.h"

typedef struct {
    FILE* fp;
} NPFile;

const char* npfilePtrOrigin = "nupiz.iofile";

#define IS_NPFILE(val) (IS_PTR(val) && AS_PTR(val)->origin == npfilePtrOrigin && \
    AS_PTR(val)->typeEncoding == 0)
#define AS_NPFILE(val) ((NPFile*) AS_PTR(val)->ptr)

static void freeNPFile(VM* vm, ObjPtr* ptr) {
    NPFile* npfile = (NPFile*) ptr->ptr;
    if (npfile->fp != NULL) {
        fclose(npfile->fp);
        npfile->fp = NULL;
    }

    FREE(vm, NPFile, npfile);
    ptr->ptr = NULL;
}

static ObjPtr* newNPFile(VM* vm, FILE* fp) {
    NPFile* npfile = ALLOCATE(vm, NPFile, 1);
    npfile->fp = fp;

    ObjPtr* ptr = newPtr(vm, npfilePtrOrigin, 0);
    ptr->ptr = (void*) npfile;
    ptr->freeFn = freeNPFile;

    return ptr;
}

static NativeResult openFileNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2))
        return NATIVE_FAIL;
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) {
        runtimeError(vm, "Expected strings for arguments.");
        return NATIVE_FAIL;
    }

    char* filename = AS_CSTRING(args[0]);
    char* mode = AS_CSTRING(args[1]);

    FILE* fp = fopen(filename, mode);
    if (fp == NULL) {
        runtimeError(vm, "Failed to open file.");
        return NATIVE_FAIL;
    }

    return NATIVE_VAL(OBJ_VAL(newNPFile(vm, fp)));
}

static NPFile* expectOpenFile(VM* vm, int argc, Value* args, int expected) {
    if (!expectArgs(vm, argc, expected))
        return NULL;
    if (!IS_NPFILE(args[0])) {
        runtimeError(vm, "Expected file pointer.");
        return NULL;
    }

    NPFile* npfile = AS_NPFILE(args[0]);
    if (npfile->fp == NULL) {
        runtimeError(vm, "File is closed. Expected open file.");
        return NULL;
    }
    return npfile;
}

static NativeResult closeFileNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;
    if (!IS_NPFILE(args[0])) {
        runtimeError(vm, "Expected file pointer.");
        return NATIVE_FAIL;
    }

    NPFile* npfile = AS_NPFILE(args[0]);
    if (npfile->fp != NULL) {
        fclose(npfile->fp);
        npfile->fp = NULL;
        return NATIVE_VAL(BOOL_VAL(true));
    }

    return NATIVE_VAL(BOOL_VAL(false));
}

static NativeResult readFileNative(VM* vm, int argc, Value* args) {
    NPFile* npfile = expectOpenFile(vm, argc, args, 1);
    if (npfile == NULL)
        return NATIVE_FAIL;
    
    FILE* fp = npfile->fp;

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    rewind(fp);

    char buf[len];
    fread(buf, sizeof(char), len, fp);
    rewind(fp);

    ObjString* str = copyString(vm, buf, len);
    return NATIVE_VAL(OBJ_VAL(str));
}

static NativeResult fileLengthNative(VM* vm, int argc, Value* args) {
    NPFile* npfile = expectOpenFile(vm, argc, args, 1);
    if (npfile == NULL)
        return NATIVE_FAIL;
    
    FILE* fp = npfile->fp;

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    rewind(fp);

    return NATIVE_VAL(NUMBER_VAL(len));
}

static NativeResult writeFileNative(VM* vm, int argc, Value* args) {
    NPFile* npfile = expectOpenFile(vm, argc, args, 2);
    if (npfile == NULL)
        return NATIVE_FAIL;
    
    FILE* fp = npfile->fp;
    ObjString* toWrite = strValue(vm, args[1]);

    fseek(fp, 0, SEEK_END);
    size_t written = fwrite(toWrite->chars, sizeof(char), toWrite->length, fp);
    rewind(fp);

    return NATIVE_VAL(NUMBER_VAL(written));
}

static NativeResult writeFileAtNative(VM* vm, int argc, Value* args) {
    NPFile* npfile = expectOpenFile(vm, argc, args, 3);
    if (npfile == NULL)
        return NATIVE_FAIL;
    if (!IS_NUMBER(args[2])) {
        runtimeError(vm, "Expected index as third argument.");
        return NATIVE_FAIL;
    }
    
    FILE* fp = npfile->fp;
    ObjString* toWrite = strValue(vm, args[1]);
    size_t idx = (size_t) AS_NUMBER(args[2]);

    fseek(fp, idx, SEEK_CUR);
    size_t written = fwrite(toWrite->chars, sizeof(char), toWrite->length, fp);
    rewind(fp);

    return NATIVE_VAL(NUMBER_VAL(written));
}

static NativeResult writeFileByteNative(VM* vm, int argc, Value* args) {
    NPFile* npfile = expectOpenFile(vm, argc, args, 2);
    if (npfile == NULL)
        return NATIVE_FAIL;
    
    FILE* fp = npfile->fp;
    if (!IS_NUMBER(args[1])) {
        runtimeError(vm, "Expected byte as second argument.");
        return NATIVE_FAIL;
    }

    uint8_t byte = (uint8_t) AS_NUMBER(args[1]);

    fseek(fp, 0, SEEK_END);
    size_t written = fwrite(&byte, sizeof(uint8_t), 1, fp);
    rewind(fp);

    return NATIVE_VAL(NUMBER_VAL(written));
}

bool importFileLib(VM* vm, ObjString* lib) {
    LIBFUNC("openFile", openFileNative);
    LIBFUNC("closeFile", closeFileNative);

    LIBFUNC("readFile", readFileNative);

    LIBFUNC("fileLength", fileLengthNative);

    LIBFUNC("writeFile", writeFileNative);
    LIBFUNC("writeFileAt", writeFileAtNative);
    LIBFUNC("writeFileByte", writeFileByteNative);

    return true;
}
