
#include "manager.h"

#include "../std/nplib.h"
#include "../fileio/filelib.h"
#include <stdio.h>

void defineAllLibraries(VM* vm) {
    defineLibrary(vm, "std", importNPLib);
    defineLibrary(vm, "iofile", importFileLib);
}
