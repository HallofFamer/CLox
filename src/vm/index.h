#pragma once
#ifndef clox_index_h
#define clox_index_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    int value;
} IndexEntry;

typedef struct {
    int count;
    int capacity;
    IndexEntry* entries;
} IndexMap;

void initIndexMap(IndexMap* indexMap);
void freeIndexMap(VM* vm, IndexMap* indexMap);
bool indexMapGet(IndexMap* indexMap, ObjString* key, int* index);
bool indexMapSet(VM* vm, IndexMap* indexMap, ObjString* key, int index);
void indexMapAddAll(VM* vm, IndexMap* from, IndexMap* to);
void markIndexMap(VM* vm, IndexMap* indexMap);

#endif // !clox_index_h