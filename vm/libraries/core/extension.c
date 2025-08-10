#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extension.h"
#include "../../vm/object.h"
#include "../../util/table.h"
#include "../../vm/vm.h"

ObjString* defineLibrary(VM* vm, const char* name, ImportLibrary init) {
    ObjString* nameString = copyString(vm, name, strlen(name));
    push(vm, OBJ_VAL(nameString));

    ObjLibrary* library = newLibrary(vm, nameString, init);
    push(vm, OBJ_VAL(library));

    if (!tableSet(vm, &vm->libraries, nameString, OBJ_VAL(library))) {
        fprintf(stderr, "Library '%s' is already defined.", name);
        exit(1);
    }

    popn(vm, 2);

    return nameString;
}

ObjString* defineFunction(VM* vm, ObjString* lib, const char* name, NativeFn func) {
    ObjNative* native = newNative(vm, func);
    Value val = OBJ_VAL(native);
    
    push(vm, val);
    ObjString* nameString = defineConstant(vm, lib, name, val);
    pop(vm);
    
    return nameString;
}

ObjString* defineConstant(VM* vm, ObjString* lib, const char* name, Value val) {
    ObjString* nameString = copyString(vm, name, strlen(name));

    push(vm, OBJ_VAL(nameString));
    push(vm, val);

    Value libVal;
    ObjLibrary* library;
    if (!tableGet(&vm->libraries, lib, &libVal) || !IS_LIBRARY(libVal) 
        || !(library = AS_LIBRARY(libVal))->imported) {
        fprintf(stderr, "Undefined library '%s'.", lib->chars);
        exit(1);
    }

    ObjNamespace* nspace = library->nspace;
    if (!writeNamespace(vm, nspace, nameString, val, true)) {
        fprintf(stderr, "Redefinition of '%s.%s'.", lib->chars, name);
        exit(1);
    }
    popn(vm, 2);

    return nameString;
}

bool expectArgs(VM* vm, int argc, int expected) {
    if (argc != expected) {
        runtimeError(vm, "Expected %d args, got %d.", expected, argc);
        return false;
    }
    return true;
}

bool importLibrary(VM* vm, ObjString* lib) {
    Value libVal;
    if (!tableGet(&vm->libraries, lib, &libVal))
        return false;
    
    ObjLibrary* library = AS_LIBRARY(libVal);
    if (library->imported)
        return true;
    
    library->imported = true;
    library->nspace = newNamespace(vm, library->name);
    if (!library->initializer(vm, lib))
        return false;
    
    tableSet(vm, &vm->globals, library->name, OBJ_VAL(library->nspace));
    
    return true;
}
