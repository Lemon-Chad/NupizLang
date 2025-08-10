
#include "npvec.hpp"

static void freeNPVector(VM* vm, ObjPtr* ptr) {
    NPVector* npvec = (NPVector*) ptr->ptr;
    delete npvec->vec;

    FREE(vm, NPVector, npvec);
    ptr->ptr = NULL;
}

static void blackenNPVector(VM* vm, ObjPtr* ptr) {
    NPVector* npvec = (NPVector*) ptr->ptr;
    for (int i = 0; i < npvec->vec->size(); i++)
        markValue(vm, (*npvec->vec)[i]);
}

ObjPtr* newNPVector(VM* vm, std::vector<Value>* vec) {
    NPVector* npvector = ALLOCATE(vm, NPVector, 1);
    npvector->vec = vec;

    ObjPtr* ptr = newPtr(vm, npvecPtrOrigin, 0);
    ptr->ptr = (void*) npvector;
    ptr->freeFn = freeNPVector;
    ptr->blackenFn = blackenNPVector;

    return ptr;
}
