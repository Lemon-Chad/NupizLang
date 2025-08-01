#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "../vm/object.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* tb) {
    tb->capacity = 0;
    tb->count = 0;
    tb->entries = NULL;
}

void freeTable(VM* vm, Table* tb) {
    FREE_ARRAY(vm, Entry, tb->entries, tb->capacity);
    initTable(tb);
}

static void printEntries(Entry* entries, int capacity) {
    printf("{ ");
    for (int i = 0; i < capacity; i++) {
        Entry* entry = &entries[i];
        if (entry->key == NULL)
            continue;
        
        printf("(\"%s\"[%d] -> ", entry->key->chars, i);
        printValue(entry->value);
        printf(") ");
    }
    printf("}");
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t idx = key->hash & (capacity - 1);
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[idx];
        if (entry->key == NULL) {
            if (IS_NULL(entry->value))
                return tombstone != NULL ? tombstone : entry;
            if (tombstone == NULL)
                tombstone = entry;
        } else if (key->length == entry->key->length && 
                memcmp(entry->key->chars, key->chars, key->length) == 0) {
            return entry;
        }
        
        idx = (idx + 1) & (capacity - 1);
    }
}

bool tableGet(Table* tb, ObjString* key, Value* ptr) {
    if (tb->count == 0)
        return false;
    
    Entry* entry = findEntry(tb->entries, tb->capacity, key);
    if (entry->key == NULL)
        return false;
    
    *ptr = entry->value;
    return true;
}

static void adjustCapacity(VM* vm, Table* tb, int cap) {
    Entry* entries = ALLOCATE(vm, Entry, cap);
    for (int i = 0; i < cap; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    tb->count = 0;
    for (int i = 0; i < tb->capacity; i++) {
        Entry* entry = &tb->entries[i];
        if (entry->key == NULL)
            continue;
        tb->count++;
        
        Entry* dest = findEntry(entries, cap, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
    }

    FREE_ARRAY(vm, Entry, tb->entries, tb->capacity);
    tb->entries = entries;
    tb->capacity = cap;
}

void tableAddAll(VM* vm, Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key == NULL)
            continue;
        
        tableSet(vm, to, entry->key, entry->value);
    }
}

void tableRemoveWhite(Table* tb) {
    for (int i = 0; i < tb->capacity; i++) {
        Entry* entry = &tb->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(tb, entry->key);
        }
    }
}

ObjString* tableFindString(Table* tb, const char* src, int len, uint32_t hash) {
    if (tb->count == 0)
        return NULL;
    
    uint32_t idx = hash & (tb->capacity - 1);
    for (;;) {
        Entry* entry = &tb->entries[idx];
        if (entry->key == NULL) {
            if (IS_NULL(entry->value))
                return NULL;
        } else if (entry->key->length == len && entry->key->hash == hash &&
                    memcmp(entry->key->chars, src, len) == 0) {
            return entry->key;
        }

        idx = (idx + 1) & (tb->capacity - 1);
    }
}

bool tableSet(VM* vm, Table* tb, ObjString* key, Value val) {
    if (tb->count + 1 > tb->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(tb->capacity);
        adjustCapacity(vm, tb, capacity);
    }

    Entry* entry = findEntry(tb->entries, tb->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NULL(entry->value))
        tb->count++;

    entry->key = key;
    entry->value = val;
    return isNewKey;
}

bool tableDelete(Table* tb, ObjString* key) {
    if (tb->count == 0) return false;

    Entry* entry = findEntry(tb->entries, tb->capacity, key);
    if (entry->key == NULL)
        return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void printTable(Table* tb) {
    printf("{ ");
    for (int i = 0; i < tb->capacity; i++) {
        Entry* entry = &tb->entries[i];
        if (entry->key == NULL)
            continue;
        
        printf("(\"%s\"[%d] -> ", entry->key->chars, i);
        printValue(entry->value);
        printf(") ");
    }
    printf("}");
}
