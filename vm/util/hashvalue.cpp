#include <string>

#include "hashvalue.hpp"
#include "../vm/object.h"

static size_t hashObject(VM* vm, Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*) obj;
            std::string_view s(string->chars);
            std::hash<std::string_view> hash_f;
            return hash_f(s);
        }

        case OBJ_ATTRIBUTE:
            return hashValue(vm, ((ObjAttribute*) obj)->val);
        
        case OBJ_BOUND_METHOD:
            return hashObject(vm, (Obj*) ((ObjBoundMethod*) obj)->method->function->name);

        case OBJ_CLASS:
            return hashObject(vm, (Obj*) ((ObjClass*) obj)->name);

        case OBJ_CLOSURE:
            return hashObject(vm, (Obj*) ((ObjClosure*) obj)->function->name);
        
        case OBJ_FUNCTION:
            return hashObject(vm, (Obj*) ((ObjFunction*) obj)->name);
        
        case OBJ_INSTANCE: {
            ObjInstance* inst = (ObjInstance*) obj;
            NativeResult res = callDefaultMethod(vm, inst, DEFMTH_HASH, NULL, 0);
            if (res.success) {
                return (std::size_t) AS_NUMBER(res.val);
            }
            return hashObject(vm, (Obj*) formatString(vm, "<%p %s>", (void*) inst, inst->clazz->name->chars));
        }

        case OBJ_LIBRARY:
            return hashObject(vm, (Obj*) ((ObjLibrary*) obj)->name);
        
        case OBJ_LIST: {
            std::size_t hash = 0;
            ObjList* lst = (ObjList*) obj;
            for (int i = 0; i < lst->list.count; i++)
                hash = (hash << 3) ^ hashValue(vm, lst->list.values[i]) * 655;
            return hash + 7 * lst->list.count + 13 * lst->list.capacity;
        }

        case OBJ_NAMESPACE:
            return hashObject(vm, (Obj*) ((ObjNamespace*) obj)->name);
        
        case OBJ_NATIVE:
            // :)
            return 0xabcd;
        
        case OBJ_PTR: {
            ObjPtr* ptr = (ObjPtr*) obj;
            if (ptr->hashFn == NULL) {
                std::hash<void*> hash_f;
                return hash_f(ptr->ptr);
            }
            return ptr->hashFn(vm, ptr);
        }
        
        case OBJ_UPVALUE:
            return hashValue(vm, ((ObjUpvalue*) obj)->closed);
    }
}

std::size_t hashValue(VM* vm, Value val) {
    switch (val.type) {
        case VAL_BOOL:
            return val.as.boolean ? 1 : 0;
        case VAL_NULL:
            return 0;
        case VAL_NUMBER: {
            std::hash<int> hash_f;
            return hash_f((int) val.as.number);
        }
        case VAL_OBJ:
            return hashObject(vm, val.as.obj);
    }
}
