
#ifndef jp_npmap_h
#define jp_npmap_h

#include <unordered_map>
#include "../../util/hashvalue.hpp"

extern "C" {

#include "../core/extension.h"

typedef std::unordered_map<HashValue, Value, ValueHash> unordered_valmap;

typedef struct {
    unordered_valmap* map;
} NPMap;

static const char* npmapPtrOrigin = "nupiz.map";

#define IS_NPMAP(val) (IS_PTR(val) && AS_PTR(val)->origin == npmapPtrOrigin && \
    AS_PTR(val)->typeEncoding == 0)
#define AS_NPMAP(val) ((NPMap*) AS_PTR(val)->ptr)

ObjPtr* newNPMap(VM* vm, unordered_valmap* map);

}

#endif
