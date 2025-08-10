
#ifndef jp_hashing_h
#define jp_hashing_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../vm/vm.h"

size_t hashVal(VM* vm, Value val);

#ifdef __cplusplus
}
#endif

#endif
