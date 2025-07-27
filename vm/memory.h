
#ifndef jp_memory_h
#define jp_memory_h

#include "common.h"
#include "compiler.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*  2)

#define ALLOCATE(vm, type, count) (type*) reallocate(vm, NULL, 0, sizeof(type) * (count))

#define GROW_ARRAY(vm, type, ptr, oldSize, newSize) \
    (type*) reallocate(vm, ptr, sizeof(type)* (oldSize), sizeof(type)* (newSize))

#define FREE_ARRAY(vm, type, ptr, oldSize) reallocate(vm, ptr, sizeof(type)* (oldSize), 0)
#define FREE(vm, type, ptr) reallocate(vm, ptr, sizeof(type), 0)

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize);
void freeObjects(VM* vm);
void markObject(VM* vm, Obj* obj);
void markValue(VM* vm, Value val);
void markTable(VM* vm, Table* tb);
void markCompilerRoots(VM* vm, Compiler* compiler);
void collectGarbage(VM* vm);

#endif
