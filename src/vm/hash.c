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
            int hash = 7;
            hash = hash * 31 + hashNumber(function->arity);
            hash = hash * 31 + hashNumber(function->chunk.count);
            return hash;
        }
        case OBJ_ARRAY: { 
            ObjArray* array = (ObjArray*)object;
            int hash = 7;
            for (int i = 0; i < array->elements.count; i++) {
                hash = hash * 31 + hashValue(array->elements.values[i]);
            }
            return hash;
        }
        case OBJ_DICTIONARY: { 
            ObjDictionary* dict = (ObjDictionary*)object;
            int hash = 7;
            for (int i = 0; i < dict->capacity; i++) {
                ObjEntry* entry = &dict->entries[i];
                if (IS_UNDEFINED(entry->key)) continue;
                hash = hash * 31 + hashValue(entry->key);
                hash = hash * 31 + hashValue(entry->value);
            }
            return hash;
        }
        case OBJ_ENTRY: { 
            ObjEntry* entry = (ObjEntry*)object;
            int hash = 7;
            hash = hash * 31 + hashValue(entry->key);
            hash = hash * 31 + hashValue(entry->value);
            return hash;
        }
        case OBJ_INSTANCE: { 
            ObjInstance* instance = (ObjInstance*)object;
            int hash = 7;
            hash = hash * 31 + hashValue(INT_VAL(instance->shapeID));
            for (int i = 0; i < instance->fields.count; i++) {
                hash = hash * 31 + hashValue(instance->fields.values[i]);
            }
            return hash;
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
