#include <stdlib.h>
#include <string.h>

#include "index.h"
#include "memory.h"

void initIndexMap(IndexMap* indexMap) {
    indexMap->count = 0;
    indexMap->capacity = 0;
    indexMap->entries = NULL;
}

void freeIndexMap(VM* vm, IndexMap* indexMap) {
    FREE_ARRAY(IndexEntry, indexMap->entries, indexMap->capacity);
    initIndexMap(indexMap);
}

static IndexEntry* findIndexEntry(IndexEntry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    for (;;) {
        IndexEntry* entry = &entries[index];
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

bool indexMapGet(IndexMap* indexMap, ObjString* key, int* index) {
    if (indexMap->count == 0) return false;

    IndexEntry* entry = findIndexEntry(indexMap->entries, indexMap->capacity, key);
    if (entry->key == NULL) return false;

    *index = entry->value;
    return true;
}

static void indexMapAdjustCapacity(VM* vm, IndexMap* indexMap, int capacity) {
    IndexEntry* entries = ALLOCATE(IndexEntry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = -1;
    }

    indexMap->count = 0;
    for (int i = 0; i < indexMap->capacity; i++) {
        IndexEntry* entry = &indexMap->entries[i];
        if (entry->key == NULL) continue;

        IndexEntry* dest = findIndexEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        indexMap->count++;
    }

    FREE_ARRAY(IndexEntry, indexMap->entries, indexMap->capacity);
    indexMap->entries = entries;
    indexMap->capacity = capacity;
}

bool indexMapSet(VM* vm, IndexMap* indexMap, ObjString* key, int index) {
    if (indexMap->count + 1 > indexMap->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(indexMap->capacity);
        indexMapAdjustCapacity(vm, indexMap, capacity);
    }

    IndexEntry* entry = findIndexEntry(indexMap->entries, indexMap->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && entry->value == -1) indexMap->count++;

    entry->key = key;
    entry->value = index;
    return isNewKey;
}

void indexMapAddAll(VM* vm, IndexMap* from, IndexMap* to) {
    for (int i = 0; i < from->capacity; i++) {
        IndexEntry* entry = &from->entries[i];
        if (entry->key != NULL) {
            indexMapSet(vm, to, entry->key, entry->value);
        }
    }
}

void markIndexMap(VM* vm, IndexMap* indexMap) {
    for (int i = 0; i < indexMap->capacity; i++) {
        IndexEntry* entry = &indexMap->entries[i];
        markObject(vm, (Obj*)entry->key);
    }
}