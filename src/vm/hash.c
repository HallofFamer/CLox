#include <string.h>

#include "hash.h"

uint32_t hashString(const char* chars, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)chars[i];
        hash *= 16777619;
    }
    return hash;
}

uint32_t hashObject(Obj* object) {
    switch (object->type) {
        case OBJ_STRING:
            return ((ObjString*)object)->hash;
        case OBJ_CLASS:
            return ((ObjClass*)object)->name->hash;    
        case OBJ_FUNCTION: { 
            ObjFunction* function = (ObjFunction*)object;
            return hashNumber(function->arity) ^ hashNumber(function->chunk.count);
        }
        default: {
            uint64_t hash = (uint64_t)(&object);
            return hash64To32Bits(hash);
        }
    }
}

uint32_t hashValue(Value value) {
    if (IS_OBJ(value)) return hashObject(AS_OBJ(value));
    return hash64To32Bits(value);
}
