
#ifndef jp_compiler_h
#define jp_compiler_h

#include "object.h"
#include "value.h"
#include "vm.h"

ObjFunction* compile(VM* vm, const char* src);
void markCompilerRoots(VM* vm, Compiler* compiler);

#endif
