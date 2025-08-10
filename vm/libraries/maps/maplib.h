
#ifndef jp_maplib_h
#define jp_maplib_h

#ifdef __cplusplus
extern "C" {
#endif

#include "../core/extension.h"

bool importMapLib(VM* vm, ObjString* lib);

#ifdef __cplusplus
}
#endif

#endif
