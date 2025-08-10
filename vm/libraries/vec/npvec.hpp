
#ifndef jp_npvec_h
#define jp_npvec_h

#include <vector>

#include "../core/extension.h"

const char* npvecPtrOrigin = "nupiz.vec";

#define IS_NPVECTOR(val) (IS_PTR(val) && AS_PTR(val)->origin == npvecPtrOrigin && \
    AS_PTR(val)->typeEncoding == 0)
#define AS_NPVECTOR(val) ((NPVector*) AS_PTR(val)->ptr)

typedef struct {
    std::vector<Value>* vec;
} NPVector;

ObjPtr* newNPVector(VM* vm, std::vector<Value>* vec);

#endif
