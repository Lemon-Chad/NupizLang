
#ifndef jp_npmap_h
#define jp_npmap_h

#include <unordered_map>

extern "C" {

#include "../core/extension.h"
#include "../../util/hashvalue.hpp"

typedef struct {
    std::unordered_map<HashValue, Value, ValueHash>* map;
} NPMap;

static const char* npmapPtrOrigin = "nupiz.map";

#define IS_NPMAP(val) (IS_PTR(val) && AS_PTR(val)->origin == npmapPtrOrigin && \
    AS_PTR(val)->typeEncoding == 0)
#define AS_NPMAP(val) ((NPMap*) AS_PTR(val)->ptr)

ObjPtr* newNPMap(VM* vm, std::unordered_map<HashValue, Value, ValueHash>* map);

}

#endif
