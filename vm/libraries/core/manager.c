
#include "manager.h"

#include "../std/nplib.h"
#include <stdio.h>

void defineAllLibraries(VM* vm) {
    defineLibrary(vm, "std", importNPLib);
}
