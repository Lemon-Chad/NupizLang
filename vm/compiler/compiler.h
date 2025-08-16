
#ifndef jp_compiler_h
#define jp_compiler_h

#include "../vm/object.h"
#include "../vm/value.h"
#include "../vm/vm.h"

ObjFunction* compile(VM* vm, const char* filepath, const char* src);
void markCompilerRoots(VM* vm, Compiler* compiler);

#endif
