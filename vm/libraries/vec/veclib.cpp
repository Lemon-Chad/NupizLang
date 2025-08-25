
#include <iostream>
#include <string>
#include <vector>

#include "veclib.h"
#include "../core/extension.h"
#include "npvec.hpp"

static NativeResult vecNative(VM* vm, int argc, Value* args) {
    std::vector<Value>* vec = new std::vector<Value>(args, args + argc);
    ObjPtr* ptr = newNPVector(vm, vec);
    return NATIVE_VAL(OBJ_VAL(ptr));
}

static NativeResult vecFromNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1)) {
        return NATIVE_FAIL;
    }

    std::vector<Value>* vec;
    if (IS_STRING(args[0])) {
        ObjString* str = AS_STRING(args[0]);
        vec = new std::vector<Value>(str->length);
        for (int i = 0; i < str->length; i++) {
            Value c = OBJ_VAL(formatString(vm, "%c", str->chars[i]));
            push(vm, c);
            (*vec)[i] = c;
        }
        popn(vm, str->length);
    } else if (IS_LIST(args[0])) {
        ValueArray* list = &AS_LIST(args[0])->list;
        vec = new std::vector<Value>(list->values, 
                list->values + list->count);
    } else {
        runtimeError(vm, "Expected list or string as argument.");
        return NATIVE_FAIL;
    }

    ObjPtr* ptr = newNPVector(vm, vec);
    return NATIVE_VAL(OBJ_VAL(ptr));
}

static NativeResult findNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2)) {
        return NATIVE_FAIL;
    }
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as argument.");
        return NATIVE_FAIL;
    }

    NPVector* vec = AS_NPVECTOR(args[0]);
    Value target = args[1];
    auto it = std::find_if(vec->vec->begin(), vec->vec->end(), 
        [vm, target](Value val) { return valuesEqual(vm, val, target); });
    if (it == vec->vec->end())
        return NATIVE_VAL(NUMBER_VAL(-1));
    return NATIVE_VAL(NUMBER_VAL((double) std::distance(vec->vec->begin(), it)));
}

static NativeResult appendNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2))
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector for first argument.");
        return NATIVE_FAIL;
    }

    NPVector* npvector = AS_NPVECTOR(args[0]);
    npvector->vec->emplace_back(args[1]);

    return NATIVE_OK;
}

static NativeResult popNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as argument.");
        return NATIVE_FAIL;
    }

    NPVector* npvector = AS_NPVECTOR(args[0]);
    Value val = npvector->vec->back();
    npvector->vec->pop_back();
    return NATIVE_VAL(val);
}

static NativeResult removeNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2)) 
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as first argument.");
        return NATIVE_FAIL;
    }
    if (!IS_NUMBER(args[1])) {
        runtimeError(vm, "Expected a number index as a second argument.");
        return NATIVE_FAIL;
    }

    NPVector* npvector = AS_NPVECTOR(args[0]);
    int idx = (int) AS_NUMBER(args[1]);
    size_t len = npvector->vec->size();
    if (idx < 0)
        idx += len;
    if (idx < 0 || idx >= len) {
        runtimeError(vm, "Index out of range.");
        return NATIVE_FAIL;
    }
    npvector->vec->erase(npvector->vec->begin() + idx);
    return NATIVE_OK;
}

static NativeResult sizeNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1)) 
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as argument.");
        return NATIVE_FAIL;
    }

    return NATIVE_VAL(NUMBER_VAL((double) AS_NPVECTOR(args[0])->vec->size()));
}

static NativeResult atNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2)) 
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as first argument.");
        return NATIVE_FAIL;
    }
    if (!IS_NUMBER(args[1])) {
        runtimeError(vm, "Expected a number index as a second argument.");
        return NATIVE_FAIL;
    }

    NPVector* npvector = AS_NPVECTOR(args[0]);
    int idx = AS_NUMBER(args[1]);
    size_t len = npvector->vec->size();
    if (idx < 0)
        idx += len;
    if (idx < 0 || idx >= len) {
        runtimeError(vm, "Index out of range.");
        return NATIVE_FAIL;
    }
    
    return NATIVE_VAL((*npvector->vec)[idx]);
}

static NativeResult insertNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 3)) 
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as first argument.");
        return NATIVE_FAIL;
    }
    if (!IS_NUMBER(args[2])) {
        runtimeError(vm, "Expected a number index as a second argument.");
        return NATIVE_FAIL;
    }

    NPVector* npvector = AS_NPVECTOR(args[0]);
    int idx = (int) AS_NUMBER(args[2]);
    size_t len = npvector->vec->size();
    if (idx < 0)
        idx += len;
    if (idx < 0 || idx >= len) {
        runtimeError(vm, "Index out of range.");
        return NATIVE_FAIL;
    }

    npvector->vec->insert(npvector->vec->begin() + idx, args[1]);
    
    return NATIVE_OK;
}

static NativeResult setNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 3)) 
        return NATIVE_FAIL;
    if (!IS_NPVECTOR(args[0])) {
        runtimeError(vm, "Expected vector as first argument.");
        return NATIVE_FAIL;
    }
    if (!IS_NUMBER(args[2])) {
        runtimeError(vm, "Expected a number index as a second argument.");
        return NATIVE_FAIL;
    }

    NPVector* npvector = AS_NPVECTOR(args[0]);
    int idx = (int) AS_NUMBER(args[2]);
    size_t len = npvector->vec->size();
    if (idx < 0)
        idx += len;
    if (idx < 0 || idx >= len) {
        runtimeError(vm, "Index out of range.");
        return NATIVE_FAIL;
    }

    (*npvector->vec)[idx] = args[1];
    
    return NATIVE_OK;
}

bool importVecLib(VM* vm, ObjString* lib) {
    LIBFUNC("vec", vecNative);
    LIBFUNC("vecFrom", vecFromNative);
    LIBFUNC("append", appendNative);
    LIBFUNC("insert", insertNative);
    LIBFUNC("set", setNative);
    LIBFUNC("remove", removeNative);
    LIBFUNC("pop", popNative);
    LIBFUNC("size", sizeNative);
    LIBFUNC("at", atNative);
    LIBFUNC("find", findNative);

    return true;
}
