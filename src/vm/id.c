#include <stdlib.h>
#include <string.h>

#include "id.h"
#include "memory.h"

void initIDMap(IDMap* idMap, GCGenerationType generation) {
    idMap->count = 0;
    idMap->capacity = 0;
    idMap->generation = generation;
    idMap->entries = NULL;
}

void freeIDMap(VM* vm, IDMap* idMap) {
    FREE_ARRAY(IDEntry, idMap->entries, idMap->capacity, idMap->generation);
    initIDMap(idMap, idMap->generation);
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
    IDEntry* entries = ALLOCATE(IDEntry, capacity, idMap->generation);
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

    FREE_ARRAY(IDEntry, idMap->entries, idMap->capacity, idMap->generation);
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

void markIDMap(VM* vm, IDMap* idMap, GCGenerationType generation) {
    for (int i = 0; i < idMap->capacity; i++) {
        IDEntry* entry = &idMap->entries[i];
        markObject(vm, (Obj*)entry->key, generation);
    }
}

void initGenericIDMap(VM* vm) {
    GenericIDMap genericIDMap = { .capacity = 0, .count = 0, .slots = NULL };
    vm->genericIDMap = genericIDMap;
}

void freeGenericIDMap(VM* vm, GenericIDMap* genericIDMap) {
    FREE_ARRAY(ValueArray, genericIDMap->slots, genericIDMap->capacity, GC_GENERATION_TYPE_PERMANENT);
    genericIDMap->capacity = 0;
    genericIDMap->count = 0;
    genericIDMap->slots = NULL;
}

IDMap* getIDMapFromGenericObject(VM* vm, Obj* object) {
    if (object->shapeID > vm->shapes.count) {
        runtimeError(vm, "Generic object has invalid shape ID assigned.");
        exit(70);
    }
    return &vm->shapes.list[object->shapeID].indexes;
}

ValueArray* getSlotsFromGenericObject(VM* vm, Obj* object) {
    if (object->objectID == 0) return NULL;
    return &vm->genericIDMap.slots[getIndexFromObjectID(object->objectID, true)];
}

void appendToGenericIDMap(VM* vm, Obj* object) {
    GenericIDMap* genericIDMap = &vm->genericIDMap;
    if (genericIDMap->capacity < genericIDMap->count + 1) {
        uint64_t oldCapacity = genericIDMap->capacity;
        genericIDMap->capacity = GROW_CAPACITY(oldCapacity);
        genericIDMap->slots = GROW_ARRAY(ValueArray, genericIDMap->slots, oldCapacity, genericIDMap->capacity, GC_GENERATION_TYPE_PERMANENT);
    }

    ValueArray slots;
    initValueArray(&slots, GC_GENERATION_TYPE_PERMANENT);
    genericIDMap->slots[genericIDMap->count++] = slots;
}