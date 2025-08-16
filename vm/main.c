#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/common.h"
#include "compiler/chunk.h"
#include "util/debug.h"
#include "vm/vm.h"
#include "libraries/core/extension.h"

#include "compiler/dumper.h"
#include "vm/loader.h"

#include "util/memory.h"

#define FLAG_COMPILE        0b00001
#define FLAG_HELP           0b00010
#define FLAG_VERSION        0b00100
#define FLAG_OUT            0b01000
#define FLAG_RUN            0b10000
#define HAS_FLAG(flags, flag) (((flags) & (flag)) != 0)

#define NPZ_VERSION "1.0.0b"

/*
static void repl(VM* vm) {
    printf("Repl\n");
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(vm, line);
    }
}
*/

static uint8_t* readFileBytes(char* path, size_t* length) {
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    *length = len;
    rewind(fp);

    uint8_t* buf = (uint8_t*) malloc(len);
    if (buf == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buf, sizeof(uint8_t), len, fp);
    if (bytesRead < len) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    fclose(fp);
    fp = NULL;
    return buf;
}

static void dumpFile(VM* vm, ObjFunction* func, char* path) {
    FILE* fp = fopen(path, "wb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    DumpedBytes* bytes = dumpFunction(vm, func);

    if (!dumpBytes(fp, bytes)) {
        fprintf(stderr, "Failed to write to file \"%s\".\n", path);
        exit(74);
    }
    freeDumpedBytes(vm, bytes);

    fclose(fp);
    fp = NULL;
}

static ObjFunction* loadFile(VM* vm, char* path) {
    size_t length = 0;
    uint8_t* src = readFileBytes(path, &length);

    vm->pauseGC++;
    BytecodeLoader* loader = newLoader(vm, src, length);

    ObjFunction* func = readBytecode(loader);
    tableSet(vm, &vm->importedFiles, func->name, OBJ_VAL(vm->nspace));

    freeLoader(vm, loader);
    vm->pauseGC--;

    return func;
}

static void compileFile(VM* vm, char* srcPath, char* destPath) {
    char* src = readFile(srcPath);
    char* path = getFullPath(srcPath);

    ObjFunction* func = compile(vm, path, src);

    if (func == NULL)
        exit(65);

    vm->pauseGC++;
    dumpFile(vm, func, destPath);
    vm->pauseGC--;
    free(src);
}

static void runFile(VM* vm, char* path) {
    ObjFunction* func = loadFile(vm, path);

    vm->keepTop++;
    InterpretResult res = runFunc(vm, func);
    vm->keepTop--;

    if (res == INTERPRET_COMPILE_ERR) exit(65);
    if (res == INTERPRET_RUNTIME_ERR) exit(70);

    if (vm->mainFunc != NULL) {
        push(vm, OBJ_VAL(vm->mainFunc));
        ObjClosure* clos = newClosure(vm, vm->mainFunc);
        pop(vm);
        push(vm, OBJ_VAL(clos));

        ObjList* lst = newList(vm);
        push(vm, OBJ_VAL(lst));
        for (int i = 0; i < vm->argc; i++) {
            Value str = OBJ_VAL(copyString(vm, vm->argv[i], strlen(vm->argv[i])));
            push(vm, str);
            writeValueArray(vm, &lst->list, str);
            pop(vm);
        }

        callFunc(vm, clos, 1, NULL_VAL);

        res = run(vm);

        if (res == INTERPRET_COMPILE_ERR) exit(65);
        if (res == INTERPRET_RUNTIME_ERR) exit(70);
    }
    pop(vm);
}

int main(int argc, const char* argv[]) {
    VM vm;
    initVM(&vm, "main");
    vm.isMain = true;

    int flags = 0;
    char* compileTarget = "";
    char* outputTarget = "";
    char* runTarget = "";

    if (argc == 1) flags |= FLAG_HELP;

    if (argc > 1 && strlen(argv[1]) == 2 && argv[1][0] == '-' && argv[1][1] == 'R') {
        if (argc < 3) {
            fprintf(stderr, "Expected binary file name.\n");
            exit(2);
        }

        vm.argv = argv + 3;
        vm.argc = argc - 3;
        changeDirectoryToFile(argv[2]);
        runFile(&vm, argv[2]);
        return 0;
    }

    for (int i = 1; i < argc; i++) {
        int arglen = strlen(argv[i]);
        if (arglen != 2 || argv[i][0] != '-') {
            fprintf(stderr, "Invalid argument '%s'.\n", argv[i]);
            exit(2);
        }

        switch (argv[i][1]) {
            case 'R':
                fprintf(stderr, "-R must be the first flag.\n");
                exit(2);
                break;
            case 'c':
                flags |= FLAG_COMPILE;
                if (i + 1 >= argc) {
                    fprintf(stderr, "-c does not preceed a path.\n");
                    exit(2);
                }
                compileTarget = argv[++i];
                break;
            case 'o':
                flags |= FLAG_OUT;
                if (i + 1 >= argc) {
                    fprintf(stderr, "-o does not preceed a path.\n");
                    exit(2);
                }
                outputTarget = argv[++i];
                break;
            case 'r':
                flags |= FLAG_RUN;
                if (i + 1 >= argc) {
                    fprintf(stderr, "-r does not preceed a path.\n");
                    exit(2);
                }
                runTarget = argv[++i];
                break;
            case 'h':
                flags |= FLAG_HELP;
                break;
            case 'v':
                flags |= FLAG_VERSION;
                break;
        }
    }

    if (HAS_FLAG(flags, FLAG_HELP)) {
        printf("Usage: npz [options]\n");
        printf("Options:\n");
        printf("  -c [target]\t\tCompile target\n");
        printf("  -o [target]\t\tOutput target to file\n");
        printf("  -r [target]\t\tRuns the target compiled file\n");
        printf("  -R [target]\t\tRuns the target compiled file,\n");
        printf("             \t\tpassing all remaining args to the VM\n");
        printf("  -v\t\tPrint version\n");
        printf("  -h\t\tPrint this help message\n");
    }

    if (HAS_FLAG(flags, FLAG_VERSION)) {
        printf(" -- jackson smith --\n");
        printf("nupiz version %s\n", NPZ_VERSION);
    }

    if (HAS_FLAG(flags, FLAG_COMPILE)) {
        if (!HAS_FLAG(flags, FLAG_OUT)) {
            fprintf(stderr, "No output file specified.\n");
            exit(2);
        }

        changeDirectoryToFile(compileTarget);
        compileFile(&vm, compileTarget, outputTarget);
    }

    if (HAS_FLAG(flags, FLAG_RUN)) {
        changeDirectoryToFile(runTarget);
        runFile(&vm, runTarget);
    }
    
    freeVM(&vm);
    return 0;
}
