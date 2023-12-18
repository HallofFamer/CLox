#pragma once
#ifndef clox_index_h
#define clox_index_h

#include "common.h"
#include "value.h"

#define ENSURE_OBJECT_ID(object) \
    do { \
        if (object->objectID == 0) object->objectID = vm->objectIndex++ * 8; \
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

void initIDMap(IDMap* idMap);
void freeIDMap(VM* vm, IDMap* idMap);
bool idMapGet(IDMap* idMap, ObjString* key, int* index);
bool idMapSet(VM* vm, IDMap* idMap, ObjString* key, int index);
void idMapAddAll(VM* vm, IDMap* from, IDMap* to);
void markIDMap(VM* vm, IDMap* idMap);

#endif // !clox_index_h