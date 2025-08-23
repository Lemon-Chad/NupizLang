
#include <stdio.h>

#include "manager.h"

#include "../std/nplib.h"
#include "../fileio/filelib.h"
#include "../vec/veclib.h"
#include "../maps/maplib.h"
#include "../math/npmath.h"

void defineAllLibraries(VM* vm) {
    defineLibrary(vm, "std", importNPLib);
    defineLibrary(vm, "iofile", importFileLib);
    defineLibrary(vm, "npvec", importVecLib);
    defineLibrary(vm, "npmap", importMapLib);
    defineLibrary(vm, "math", importMathLib);
}
