
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "npmath.h"

static bool expectNumberArgs(VM* vm, Value* args, int argc, int expected) {
    if (!expectArgs(vm, argc, expected))
        return false;
    for (int i = 0; i < argc; i++) {
        if (!IS_NUMBER(args[i])) {
            runtimeError(vm, "Expected number for argument %d.", i);
            return false;
        }
    }
    return true;
}

static NativeResult powNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 2))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1]))));
}

static NativeResult modNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 2))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(fmod(AS_NUMBER(args[0]), AS_NUMBER(args[1]))));
}

static NativeResult floorNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 1))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(floor(AS_NUMBER(args[0]))));
}

static NativeResult roundNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 1))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(round(AS_NUMBER(args[0]))));
}

static NativeResult ceilNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 1))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(ceil(AS_NUMBER(args[0]))));
}

static NativeResult sinNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 1))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(sin(AS_NUMBER(args[0]))));
}

static NativeResult cosNative(VM* vm, int argc, Value* args) {
    if (!expectNumberArgs(vm, args, argc, 1))
        return NATIVE_FAIL;
    return NATIVE_VAL(NUMBER_VAL(cos(AS_NUMBER(args[0]))));
}

bool importMathLib(VM* vm, ObjString* lib) {
    LIBFUNC("pow", powNative);
    LIBFUNC("mod", modNative);
    LIBFUNC("round", roundNative);
    LIBFUNC("floor", floorNative);
    LIBFUNC("ceil", ceilNative);
    LIBFUNC("sin", sinNative);
    LIBFUNC("cos", cosNative);

    return true;
}
