#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../vm/object.h"
#include "memory.h"

#define GC_HEAP_GROWTH_FACTOR 2

void* reallocate(VM* vm, void* ptr, size_t oldSize, size_t newSize) {
    vm->bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
        #ifdef DEBUG_STRESS_GC
            collectGarbage(vm);
        #endif

        if (vm->bytesAllocated > vm->nextGC) {
            collectGarbage(vm);
        }
    }

    if (newSize == 0) {
        free(ptr);
        return NULL;
    }

    void* res = realloc(ptr, newSize);
    if (res == NULL) exit(1);
    return res;
}



static void freeObject(VM* vm, Obj* obj) {
    #ifdef DEBUG_SLOG_GC
        printObject(OBJ_VAL(obj));
        printf(": %p free type %d\n", (void*) obj, obj->type);
    #endif

    switch (obj->type) {
        case OBJ_BOUND_METHOD:
            FREE(vm, ObjBoundMethod, obj);
            break;
        
        case OBJ_LIST: {
            ObjList* list = (ObjList*) obj;
            freeValueArray(vm, &list->list);
            FREE(vm, ObjList, obj);
            break;
        }
        
        case OBJ_CLASS: {
            ObjClass* clazz = (ObjClass*) obj;
            freeTable(vm, &clazz->methods);
            FREE(vm, ObjClass, obj);
            break;
        }

        case OBJ_STRING: {
            ObjString* string = (ObjString*) obj;
            FREE_ARRAY(vm, char, string->chars, string->length + 1);
            FREE(vm, ObjString, obj);
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*) obj;
            freeChunk(vm, &func->chunk);
            FREE(vm, ObjFunction, func);
            break;
        }

        case OBJ_NATIVE:
            FREE(vm, ObjNative, obj);
            break;

        case OBJ_CLOSURE: {
            ObjClosure* clos = (ObjClosure*) obj;
            FREE_ARRAY(vm, ObjUpvalue*, clos->upvalues, clos->upvalueCount);
            FREE(vm, ObjClosure, obj);
            break;
        }

        case OBJ_UPVALUE:
            FREE(vm, ObjUpvalue, obj);
            break;
        
        case OBJ_INSTANCE: {
            ObjInstance* inst = (ObjInstance*) obj;
            freeTable(vm, &inst->fields);
            FREE(vm, ObjInstance, obj);
            break;
        }

        case OBJ_NAMESPACE: {
            ObjNamespace* nspace = (ObjNamespace*) obj;
            freeTable(vm, &nspace->values);
            freeTable(vm, &nspace->publics);
            FREE(vm, ObjNamespace, obj);
            break;
        }

        case OBJ_LIBRARY:
            FREE(vm, ObjLibrary, obj);
            break;
        
        case OBJ_ATTRIBUTE:
            FREE(vm, ObjAttribute, obj);
            break;
        
        case OBJ_PTR: {
            ObjPtr* ptr = (ObjPtr*) obj;
            if (ptr->freeFn != NULL)
                ptr->freeFn(vm, ptr);
            FREE(vm, ObjPtr, obj);
            break;
        }
    }
}

void freeObjects(VM* vm) {
    Obj* obj = vm->objects;
    while (obj != NULL) {
        Obj* next = obj->next;
        freeObject(vm, obj);
        obj = next;
    }
    vm->objects = NULL;

    free(vm->grayStack);
}

void markObject(VM* vm, Obj* obj) {
    if (obj == NULL)
        return;
    if (obj->isMarked)
        return;
    
    #ifdef DEBUG_SLOG_GC
        printf("%p mark ", (void*) obj);
        printValue(OBJ_VAL(obj));
        printf("\n");
    #endif

    obj->isMarked = true;

    if (vm->grayCapacity < vm->grayCount + 1) {
        vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
        vm->grayStack = (Obj**) realloc(vm->grayStack, sizeof(Obj*) * vm->grayCapacity);
        if (vm->grayStack == NULL) exit(1);
    }

    vm->grayStack[vm->grayCount++] = obj;
}

void markValue(VM* vm, Value val) {
    if (IS_OBJ(val)) {
        markObject(vm, AS_OBJ(val));
    }
}

void markTable(VM* vm, Table* tb) {
    for (int i = 0; i < tb->capacity; i++) {
        Entry* entry = &tb->entries[i];
        markObject(vm, (Obj*) entry->key);
        markValue(vm, entry->value);
    }
}

static void markArray(VM* vm, ValueArray* arr) {
    for (int i = 0; i < arr->count; i++) {
        markValue(vm, arr->values[i]);
    }
}

static void markRoots(VM* vm) {
    for (Value* slot = vm->stack; slot < vm->stackTop; slot++) {
        markValue(vm, *slot);
    }

    for (int i = 0; i < vm->frameCount; i++) {
        markObject(vm, (Obj*) vm->frames[i].closure);
        markValue(vm, vm->frames[i].bound);
    }

    for (ObjUpvalue* upv = vm->openUpvalues; upv != NULL; upv = upv->next) {
        markObject(vm, (Obj*) upv);
    }

    markTable(vm, &vm->globals);
    markTable(vm, &vm->libraries);
    markCompilerRoots(vm, vm->compiler);
}

static void blackenObject(VM* vm, Obj* obj) {
    #ifdef DEBUG_SLOG_GC
        printf("%p blacken ", (void*) obj);
        printValue(OBJ_VAL(obj));
        printf("\n");
    #endif

    switch (obj->type) {
        case OBJ_BOUND_METHOD: {
            ObjBoundMethod* bound = (ObjBoundMethod*) obj;
            markValue(vm, bound->reciever);
            markObject(vm, (Obj*) bound->method);
            break;
        }

        case OBJ_LIST: {
            ObjList* list = (ObjList*) obj;
            markArray(vm, &list->list);
            break;
        }

        case OBJ_CLASS: {
            ObjClass* clazz = (ObjClass*) obj;
            markObject(vm, (Obj*) clazz->name);
            markObject(vm, (Obj*) clazz->constructor);
            markTable(vm, &clazz->methods);
            break;
        }

        case OBJ_INSTANCE: {
            ObjInstance* inst = (ObjInstance*) obj;
            markObject(vm, (Obj*) inst->clazz);
            markTable(vm, &inst->fields);
            break;
        }

        case OBJ_CLOSURE: {
            ObjClosure* clos = (ObjClosure*) obj;
            markObject(vm, (Obj*) clos->function);
            for (int i = 0; i < clos->upvalueCount; i++) {
                markObject(vm, (Obj*) clos->upvalues[i]);
            }
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* func = (ObjFunction*) obj;
            markObject(vm, (Obj*) func->name);
            markArray(vm, &func->chunk.constants);
            break;
        }

        case OBJ_UPVALUE:
            markValue(vm, ((ObjUpvalue*) obj)->closed);
            break;
        
        case OBJ_NAMESPACE: {
            ObjNamespace* nspace = (ObjNamespace*) obj;
            markObject(vm, (Obj*) nspace->name);
            markTable(vm, &nspace->values);
            markTable(vm, &nspace->publics);
            break;
        }

        case OBJ_LIBRARY: {
            ObjLibrary* library = (ObjLibrary*) obj;
            markObject(vm, (Obj*) library->name);
            if (library->imported)
                markObject(vm, (Obj*) library->nspace);
            break;
        }

        case OBJ_ATTRIBUTE: {
            ObjAttribute* attr = (ObjAttribute*) obj;
            markValue(vm, attr->val);
            break;
        }

        case OBJ_PTR: {
            ObjPtr* ptr = (ObjPtr*) obj;
            if (ptr->blackenFn != NULL)
                ptr->blackenFn(vm, ptr);
            break;
        }

        case OBJ_STRING:
        case OBJ_NATIVE:
            break;
    }
}

static void traceReferences(VM* vm) {
    while (vm->grayCount > 0) {
        Obj* obj = vm->grayStack[--vm->grayCount];
        blackenObject(vm, obj);
    }
}

static void sweep(VM* vm) {
    Obj* prev = NULL;
    Obj* curr = vm->objects;
    while (curr != NULL) {
        if (curr->isMarked) {
            curr->isMarked = false;
            prev = curr;
            curr = curr->next;
        } else {
            Obj* del = curr;
            curr = curr->next;
            if (prev != NULL) {
                prev->next = curr;
            } else {
                vm->objects = curr;
            }

            freeObject(vm, del);
        }
    }
}

void collectGarbage(VM* vm) {
    if (vm->pauseGC > 0)
        return;
    
    #ifdef DEBUG_LOG_GC
        printf("-- gc begin\n");

        size_t before = vm->bytesAllocated;
    #endif

    markRoots(vm);
    traceReferences(vm);
    tableRemoveWhite(&vm->strings);
    sweep(vm);

    vm->nextGC = vm->bytesAllocated * GC_HEAP_GROWTH_FACTOR;

    #ifdef DEBUG_LOG_GC
        printf("-- gc end\n");
        printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm->bytesAllocated, before, vm->bytesAllocated, vm->nextGC);
    #endif
    
}

char* readFile(char* path) {
    FILE* fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    rewind(fp);

    char* buf = (char*) malloc(len + 1);
    if (buf == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }
    size_t bytesRead = fread(buf, sizeof(char), len, fp);
    if (bytesRead < len) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buf[bytesRead] = '\0';

    fclose(fp);
    fp = NULL;
    return buf;
}

char* getDirectory(char* path) {
    int length = 0;
    for (int i = 0; i < strlen(path); i++)
        if (path[i] == '/' || path[i] == '\\')
            length = i;
    
    char* newPath = malloc(length + 1);
    memcpy(newPath, path, length);
    newPath[length] = '\0';
    return newPath;
}

void changeDirectory(char* path) {
    path = getDirectory(path);
    chdir(path);
    free(path);
}
