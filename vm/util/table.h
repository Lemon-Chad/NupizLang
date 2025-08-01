
#ifndef jp_table_h
#define jp_table_h

#include "common.h"
#include "../vm/value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* tb);
void freeTable(VM* vm, Table* tb);
bool tableGet(Table* tb, ObjString* key, Value* ptr);
bool tableSet(VM* vm, Table* tb, ObjString* key, Value val);
bool tableDelete(Table* tb, ObjString* key);
void tableAddAll(VM* vm, Table* from, Table* to);
void tableRemoveWhite(Table* tb);
ObjString* tableFindString(Table* tb, const char* src, int len, uint32_t hash);
void printTable(Table* tb);

#endif
