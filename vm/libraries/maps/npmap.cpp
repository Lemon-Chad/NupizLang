
#include "npmap.hpp"

static void freeNPMap(VM* vm, ObjPtr* ptr) {
    NPMap* npmap = (NPMap*) ptr->ptr;
    delete npmap->map;

    FREE(vm, NPMap, npmap);
    ptr->ptr = NULL;
}

static void blackenNPMap(VM* vm, ObjPtr* ptr) {
    NPMap* npmap = (NPMap*) ptr->ptr;
    for (const auto&[key, val] : *npmap->map) {
        markValue(vm, key.val);
        markValue(vm, val);
    }
}

ObjPtr* newNPMap(VM* vm, unordered_valmap* map) {
    NPMap* npmap = ALLOCATE(vm, NPMap, 1);
    npmap->map = map;

    ObjPtr* ptr = newPtr(vm, npmapPtrOrigin, 0);
    ptr->ptr = (void*) npmap;
    ptr->freeFn = freeNPMap;
    ptr->blackenFn = blackenNPMap;

    return ptr;
}
