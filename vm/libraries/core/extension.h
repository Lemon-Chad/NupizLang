
#ifndef jp_extension_h
#define jp_extension_h

#include <stdbool.h>
#include "../../vm/value.h"
#include "../../vm/vm.h"
#include "../../util/memory.h"

#define LIBFUNC(name, func) defineFunction(vm, lib, name, func)
#define LIBCONST(name, val) defineConstant(vm, lib, name, val)

ObjString* defineLibrary(VM* vm, const char* name, ImportLibrary init);
ObjString* defineFunction(VM* vm, ObjString* lib, const char* name, NativeFn func);
ObjString* defineConstant(VM* vm, ObjString* lib, const char* name, Value val);

bool expectArgs(VM* vm, int argc, int expected);

bool importLibrary(VM* vm, ObjString* lib);

#endif
