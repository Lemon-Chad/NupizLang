
#include "hashing.h"
#include "hashvalue.hpp"

size_t hashVal(VM* vm, Value val) {
    return hashValue(vm, val);
}
