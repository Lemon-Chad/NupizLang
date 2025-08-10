
#ifndef jp_hashvalue_h
#define jp_hashvalue_h

#include <iosfwd>
#include <functional>

extern "C" {

#include "../vm/vm.h"
#include "../vm/value.h"
#include "../vm/object.h"

std::size_t hashValue(VM* vm, Value val);

}

#define HASHVALUE(val, vm) ((HashValue) { val, vm })

typedef struct HashValue {
    Value val;
    VM* vm;

    bool operator==(const HashValue& other) const {
        return valuesEqual(vm, val, other.val);
    }
} HashValue;

typedef struct ValueHash {
    std::size_t operator()(const HashValue& s) const {
        return hashValue(s.vm, s.val);
    }
} ValueHash;

#endif
