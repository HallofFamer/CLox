#include <stdlib.h>
#include <string.h>

#include "id.h"
#include "memory.h"

void initIDMap(IDMap* idMap) {
    idMap->count = 0;
    idMap->capacity = 0;
    idMap->entries = NULL;
}

void freeIDMap(VM* vm, IDMap* idMap) {
    FREE_ARRAY(IDEntry, idMap->entries, idMap->capacity);
    initIDMap(idMap);
}

static IDEntry* findIDEntry(IDEntry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    for (;;) {
        IDEntry* entry = &entries[index];
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

bool idMapGet(IDMap* idMap, ObjString* key, int* index) {
    if (idMap->count == 0) return false;

    IDEntry* entry = findIDEntry(idMap->entries, idMap->capacity, key);
    if (entry->key == NULL) return false;

    *index = entry->value;
    return true;
}

static void idMapAdjustCapacity(VM* vm, IDMap* idMap, int capacity) {
    IDEntry* entries = ALLOCATE(IDEntry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = -1;
    }

    idMap->count = 0;
    for (int i = 0; i < idMap->capacity; i++) {
        IDEntry* entry = &idMap->entries[i];
        if (entry->key == NULL) continue;

        IDEntry* dest = findIDEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        idMap->count++;
    }

    FREE_ARRAY(IDEntry, idMap->entries, idMap->capacity);
    idMap->entries = entries;
    idMap->capacity = capacity;
}

bool idMapSet(VM* vm, IDMap* idMap, ObjString* key, int index) {
    if (idMap->count + 1 > idMap->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(idMap->capacity);
        idMapAdjustCapacity(vm, idMap, capacity);
    }

    IDEntry* entry = findIDEntry(idMap->entries, idMap->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && entry->value == -1) idMap->count++;

    entry->key = key;
    entry->value = index;
    return isNewKey;
}

void idMapAddAll(VM* vm, IDMap* from, IDMap* to) {
    for (int i = 0; i < from->capacity; i++) {
        IDEntry* entry = &from->entries[i];
        if (entry->key != NULL) {
            idMapSet(vm, to, entry->key, entry->value);
        }
    }
}

void markIDMap(VM* vm, IDMap* idMap) {
    for (int i = 0; i < idMap->capacity; i++) {
        IDEntry* entry = &idMap->entries[i];
        markObject(vm, (Obj*)entry->key);
    }
}