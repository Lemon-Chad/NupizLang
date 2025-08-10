
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "maplib.h"
#include "npmap.hpp"
#include "../core/extension.h"
#include "../../util/hashvalue.hpp"
#include "../vec/npvec.hpp"

static NativeResult mapNative(VM* vm, int argc, Value* args) {
    if (argc % 2 == 1) {
        runtimeError(vm, "Not every key has a value pair.");
        return NATIVE_FAIL;
    }

    std::unordered_map<HashValue, Value, ValueHash>* map = 
        new std::unordered_map<HashValue, Value, ValueHash>;
    for (int i = 0; i < argc; i += 2) {
        map->emplace(HASHVALUE(args[i], vm), args[i + 1]);
    }
    
    ObjPtr* npmap = newNPMap(vm, map);
    return NATIVE_VAL(OBJ_VAL(npmap));
}

static NativeResult putNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 3)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPMAP(args[0])) {
        runtimeError(vm, "Expected map as first argument.");
        return NATIVE_FAIL;
    }

    NPMap* npmap = AS_NPMAP(args[0]);
    HashValue key = HASHVALUE(args[1], vm);
    (*npmap->map)[key] = args[2];
    return NATIVE_OK;
}

static NativeResult emplaceNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 3)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPMAP(args[0])) {
        runtimeError(vm, "Expected map as first argument.");
        return NATIVE_FAIL;
    }

    NPMap* npmap = AS_NPMAP(args[0]);
    HashValue key = HASHVALUE(args[1], vm);
    bool success = npmap->map->emplace(key, args[2]).second;
    return NATIVE_VAL(BOOL_VAL(success));
}

static NativeResult getNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPMAP(args[0])) {
        runtimeError(vm, "Expected map as first argument.");
        return NATIVE_FAIL;
    }

    NPMap* npmap = AS_NPMAP(args[0]);
    HashValue key = HASHVALUE(args[1], vm);
    auto it = npmap->map->find(key);
    if (it == npmap->map->end()) {
        runtimeError(vm, "Key is not found in map.");
        return NATIVE_FAIL;
    }

    return NATIVE_VAL(it->second);
}

static NativeResult removeNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPMAP(args[0])) {
        runtimeError(vm, "Expected map as first argument.");
        return NATIVE_FAIL;
    }

    NPMap* npmap = AS_NPMAP(args[0]);
    HashValue key = HASHVALUE(args[1], vm);
    return NATIVE_VAL(BOOL_VAL(npmap->map->erase(key) > 0));
}

static NativeResult hasNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPMAP(args[0])) {
        runtimeError(vm, "Expected map as first argument.");
        return NATIVE_FAIL;
    }

    NPMap* npmap = AS_NPMAP(args[0]);
    HashValue key = HASHVALUE(args[1], vm);
    auto it = npmap->map->find(key);
    return NATIVE_VAL(BOOL_VAL(it != npmap->map->end()));
}

static NativeResult keysNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPMAP(args[0])) {
        runtimeError(vm, "Expected map as argument.");
        return NATIVE_FAIL;
    }

    NPMap* npmap = AS_NPMAP(args[0]);
    std::vector<Value>* vec = new std::vector<Value>;
    for (const auto &it : *npmap->map)
        vec->push_back(it.first.val);
    ObjPtr* ptr = newNPVector(vm, vec);
    return NATIVE_VAL(OBJ_VAL(ptr));
}

bool importMapLib(VM* vm, ObjString* lib) {
    LIBFUNC("map", mapNative);
    LIBFUNC("put", putNative);
    LIBFUNC("emplace", emplaceNative);
    LIBFUNC("get", getNative);
    LIBFUNC("remove", removeNative);
    LIBFUNC("has", hasNative);
    LIBFUNC("keys", keysNative);

    return true;
}

