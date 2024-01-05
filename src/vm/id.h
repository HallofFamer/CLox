#pragma once
#ifndef clox_index_h
#define clox_index_h

#include "common.h"
#include "value.h"

#define ENSURE_OBJECT_ID(object) \
    do { \
        if (object->objectID == 0){ \
            if(object->type == OBJ_INSTANCE) object->objectID = ++vm->objectIndex * 8; \
            else object->objectID = ++vm->genericIDMap.count * 8 - 2; \
            appendToGenericIDMap(vm, object); \
        } \
    } while (false);

typedef struct {
    ObjString* key;
    int value;
} IDEntry;

typedef struct {
    int count;
    int capacity;
    IDEntry* entries;
} IDMap;

typedef struct {
    uint64_t count;
    uint64_t capacity;
    IDMap* idMaps;
    ValueArray* idSlots;
} GenericIDMap;

void initIDMap(IDMap* idMap);
void freeIDMap(VM* vm, IDMap* idMap);
bool idMapGet(IDMap* idMap, ObjString* key, int* index);
bool idMapSet(VM* vm, IDMap* idMap, ObjString* key, int index);
void idMapAddAll(VM* vm, IDMap* from, IDMap* to);
void markIDMap(VM* vm, IDMap* idMap);

void initGenericIDMap(VM* vm);
void freeGenericIDMap(VM* vm, GenericIDMap* genericIDMap);
IDMap* getIDMapFromGenericObject(VM* vm, Obj* object);
ValueArray* getIDSlotsFromGenericObject(VM* vm, Obj* object);
void appendToGenericIDMap(VM* vm, Obj* object);

static inline uint64_t getObjectIDFromIndex(uint64_t index, bool isGeneric) {
    return isGeneric ? (index << 3) + 6 : index << 3;
}

static inline uint64_t getIndexFromObjectID(uint64_t id, bool isGeneric) {
    return isGeneric ? (id - 6) >> 3 : id >> 3;
}

#endif // !clox_id_h