#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

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

static char* readFile(char* path) {
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    rewind(fp);

    char* buf = (char*) malloc(len + 1);
    if (buf == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buf, sizeof(char), len, fp);
    if (bytesRead < len) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buf[bytesRead] = '\0';

    fclose(fp);
    fp = NULL;
    return buf;
}

static void runFile(VM* vm, char* path) {
    char* src = readFile(path);
    InterpretResult res = interpret(vm, src);
    free(src);

    if (res == INTERPRET_COMPILE_ERR) exit(65);
    if (res == INTERPRET_RUNTIME_ERR) exit(70);
}

int main(int argc, const char* argv[]) {
    VM vm;
    initVM(&vm);

    if (argc == 1) {
        repl(&vm);
    } else if (argc == 2) {
        runFile(&vm, argv[1]);
    } else {
        fprintf(stderr, "Usage: jpvm [path]\n");
        exit(64);
    }
    
    freeVM(&vm);
    return 0;
}
