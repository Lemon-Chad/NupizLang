
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "nplib.h"

static NativeResult printNative(VM* vm, int argc, Value* args) {
    for (int i = 0; i < argc; i++) {
        printValue(args[i]);
        if (i + 1 < argc)
            printf(" ");
    }
    return NATIVE_OK;
}

static NativeResult printlnNative(VM* vm, int argc, Value* args) {
    printNative(vm, argc, args);
    printf("\n");
    return NATIVE_OK;
}

static NativeResult asStringNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;

    Value val = OBJ_VAL(strValue(vm, args[0]));
    return NATIVE_VAL(val);
}

static NativeResult lengthNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;

    Value arg = args[0];
    if (IS_STRING(arg)) {
        return NATIVE_VAL(NUMBER_VAL(strlen(AS_CSTRING(arg))));
    } else if (IS_LIST(arg)) {
        return NATIVE_VAL(NUMBER_VAL(AS_LIST(arg)->list.count));
    }

    runtimeError(vm, "Cannot measure length of given type.", argc);
    return NATIVE_FAIL;
}

static NativeResult appendNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2))
        return NATIVE_FAIL;

    Value list = args[0];
    Value ele = args[1];
    if (!IS_LIST(list)) {
        runtimeError(vm, "Expected a list as a first arg.");
        return NATIVE_FAIL;
    }

    push(vm, list);
    push(vm, ele);
    writeValueArray(vm, &AS_LIST(list)->list, ele);
    popn(vm, 2);

    return NATIVE_VAL(NUMBER_VAL(AS_LIST(list)->list.count));
}

static NativeResult removeNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2))
        return NATIVE_FAIL;

    Value list = args[0];
    Value ele = args[1];
    if (!IS_LIST(list)) {
        runtimeError(vm, "Expected a list as a first arg.");
        return NATIVE_FAIL;
    }
    if (!IS_NUMBER(ele)) {
        runtimeError(vm, "Expected a number index as a second arg.");
        return NATIVE_FAIL;
    }

    int idx = (int) AS_NUMBER(ele);
    ValueArray* array = &AS_LIST(list)->list;
    if (idx < 0)
        idx += array->count;
    
    if (idx < 0 || idx >= array->count) {
        runtimeError(vm, "Index out of bounds.");
        return NATIVE_FAIL;
    }

    memcpy(array->values + idx, array->values + idx + 1, 
        sizeof(Value) * (array->count-- - idx));

    return NATIVE_VAL(NUMBER_VAL(array->count));
}

static NativeResult popNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;

    Value list = args[0];
    if (!IS_LIST(list)) {
        runtimeError(vm, "Expected a list as a first arg.");
        return NATIVE_FAIL;
    }

    ValueArray* array = &AS_LIST(list)->list;
    if (array->count == 0) {
        runtimeError(vm, "Given list is empty.");
        return NATIVE_FAIL;
    }

    array->count--;
    return NATIVE_VAL(array->values[array->count]);
}

static NativeResult clockNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 0))
        return NATIVE_FAIL;
    
    return NATIVE_VAL(NUMBER_VAL(((double) clock()) / CLOCKS_PER_SEC));
}

static NativeResult cmdargsNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 0))
        return NATIVE_FAIL;
    
    ObjList* lst = newList(vm);
    push(vm, OBJ_VAL(lst));
    for (int i = 0; i < vm->argc; i++)
        writeValueArray(vm, &lst->list, 
            OBJ_VAL(copyString(vm, vm->argv[i], strlen(vm->argv[i]))));
    pop(vm);
    return NATIVE_VAL(OBJ_VAL(lst));
}

static NativeResult mainNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;
    
    if (!IS_CLOSURE(args[0]) || AS_CLOSURE(args[0])->upvalueCount > 0) {
        runtimeError(vm, "Expected function.");
        return NATIVE_FAIL;
    }

    if (vm->mainFunc != NULL) {
        runtimeError(vm, "Main function already defined.");
        return NATIVE_FAIL;
    }

    vm->mainFunc = AS_CLOSURE(args[0])->function;
    return NATIVE_OK;
}

static NativeResult asByteNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 1))
        return NATIVE_FAIL;
    if (!IS_STRING(args[0]) || AS_STRING(args[0])->length != 1) {
        runtimeError(vm, "Expected character as argument.");
        return NATIVE_FAIL;
    }
    return NATIVE_VAL(NUMBER_VAL((uint8_t) AS_CSTRING(args[0])[0]));
}

static NativeResult sliceNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 3))
        return NATIVE_FAIL;
    if (!IS_STRING(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
        runtimeError(vm, "Expected (string, int, int) as arguments.");
        return NATIVE_FAIL;
    }
    ObjString* str = AS_STRING(args[0]);

    int start = (int) AS_NUMBER(args[1]);
    if (start < 0)
        start += str->length + 1;
    
    int end = (int) AS_NUMBER(args[2]);
    if (end < 0)
        end += str->length + 1;

    if (end > str->length)
        end = str->length;
    if (start > end)
        start = end;
    
    if (start < 0 || end < 0) {
        runtimeError(vm, "Indices out of bounds.");
        return NATIVE_FAIL;
    }

    return NATIVE_VAL(OBJ_VAL(copyString(vm, str->chars + start, end - start)));
}

static NativeResult findNative(VM* vm, int argc, Value* args) {
    if (!expectArgs(vm, argc, 2))
        return NATIVE_FAIL;
    if (!IS_LIST(args[0])) {
        runtimeError(vm, "Expected list as first argument.");
        return NATIVE_FAIL;
    }

    ObjList* list = AS_LIST(args[0]);
    for (int i = 0; i < list->list.count; i++) {
        if (valuesEqual(vm, list->list.values[i], args[1])) {
            return NATIVE_VAL(NUMBER_VAL(i));
        }
    }
    return NATIVE_VAL(NUMBER_VAL(-1));
}

bool importNPLib(VM* vm, ObjString* lib) {
    LIBFUNC("print", printNative);
    LIBFUNC("println", printlnNative);
    LIBFUNC("asString", asStringNative);
    LIBFUNC("length", lengthNative);
    LIBFUNC("append", appendNative);
    LIBFUNC("remove", removeNative);
    LIBFUNC("pop", popNative);
    LIBFUNC("clock", clockNative);
    LIBFUNC("asByte", asByteNative);
    LIBFUNC("cmdargs", cmdargsNative);
    LIBFUNC("main", mainNative);
    LIBFUNC("slice", sliceNative);
    LIBFUNC("find", findNative);

    return true;
}
