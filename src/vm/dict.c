#include <stdlib.h>

#include "dict.h"
#include "hash.h"
#include "memory.h"

ObjEntry* dictFindEntry(ObjEntry* entries, int capacity, Value key) {
    uint32_t hash = hashValue(key);
    uint32_t index = hash & (capacity - 1);
    ObjEntry* tombstone = NULL;

    for (;;) {
        ObjEntry* entry = &entries[index];
        if (IS_UNDEFINED(entry->key)) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            }
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

void dictAdjustCapacity(VM* vm, ObjDictionary* dict, int capacity) {
    ObjEntry* entries = ALLOCATE(ObjEntry, capacity, dict->obj.generation);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = UNDEFINED_VAL;
        entries[i].value = NIL_VAL;
    }

    dict->count = 0;
    for (int i = 0; i < dict->capacity; i++) {
        ObjEntry* entry = &dict->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;

        ObjEntry* dest = dictFindEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        dict->count++;
    }

    FREE_ARRAY(ObjEntry, dict->entries, dict->capacity, dict->obj.generation);
    dict->entries = entries;
    dict->capacity = capacity;
}

bool dictGet(ObjDictionary* dict, Value key, Value* value) {
    if (dict->count == 0) return false;
    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    if (IS_UNDEFINED(entry->key)) return false;
    *value = entry->value;
    return true;
}

bool dictSet(VM* vm, ObjDictionary* dict, Value key, Value value) {
    if (dict->count + 1 > dict->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(dict->capacity);
        dictAdjustCapacity(vm, dict, capacity);
    }

    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    bool isNewKey = IS_UNDEFINED(entry->key);
    if (isNewKey && IS_NIL(entry->value)) dict->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

void dictAddAll(VM* vm, ObjDictionary* from, ObjDictionary* to) {
    for (int i = 0; i < from->capacity; i++) {
        ObjEntry* entry = &from->entries[i];
        if (!IS_UNDEFINED(entry->key)) {
            dictSet(vm, to, entry->key, entry->value);
        }
    }
}

bool dictDelete(ObjDictionary* dict, Value key) {
    if (dict->count == 0) return false;

    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    if (IS_UNDEFINED(entry->key)) return false;

    entry->key = UNDEFINED_VAL;
    entry->value = BOOL_VAL(true);
    dict->count--;
    return true;
}