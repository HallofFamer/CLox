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

void initGenericIDMap(VM* vm) {
    GenericIDMap genericIDMap = { .capacity = 0, .count = 0, .idMaps = NULL, .idSlots = NULL };
    vm->genericIDMap = genericIDMap;
}

void freeGenericIDMap(VM* vm, GenericIDMap* genericIDMap) {
    FREE_ARRAY(IDMap, genericIDMap->idMaps, genericIDMap->capacity);
    FREE_ARRAY(ValueArray, genericIDMap->idSlots, genericIDMap->capacity);
    genericIDMap->capacity = 0;
    genericIDMap->count = 0;
    genericIDMap->idMaps = NULL;
    genericIDMap->idSlots = NULL;
}

IDMap* getIDMapFromGenericObject(VM* vm, Obj* object) {
    if (object->objectID == 0) {
        runtimeError(vm, "Generic object has no ID assigned.");
        return NULL;
    }
    return &vm->genericIDMap.idMaps[getIndexFromObjectID(object->objectID, true)];
}

ValueArray* getIDSlotsFromGenericObject(VM* vm, Obj* object) {
    if (object->objectID == 0) {
        runtimeError(vm, "Generic object has no ID assigned.");
        return NULL;
    }
    return &vm->genericIDMap.idSlots[getIndexFromObjectID(object->objectID, true)];
}

void appendToGenericIDMap(VM* vm, Obj* object) {
    GenericIDMap* genericIDMap = &vm->genericIDMap;
    if (genericIDMap->capacity < genericIDMap->count + 1) {
        uint64_t oldCapacity = genericIDMap->capacity;
        genericIDMap->capacity = GROW_CAPACITY(oldCapacity);
        genericIDMap->idMaps = GROW_ARRAY(IDMap, genericIDMap->idMaps, oldCapacity, genericIDMap->capacity);
        genericIDMap->idSlots = GROW_ARRAY(ValueArray, genericIDMap->idSlots, oldCapacity, genericIDMap->capacity);
    }
    ENSURE_OBJECT_ID(object);

    IDMap idMap;
    initIDMap(&idMap);
    genericIDMap->idMaps[genericIDMap->count] = idMap;

    ValueArray idSlot;
    initValueArray(&idSlot);
    genericIDMap->idSlots[genericIDMap->count] = idSlot;
}