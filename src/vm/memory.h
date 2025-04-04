#pragma once
#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"
#include "value.h"
#include "vm.h"

#define GC_GENERATION_COUNT 4

enum GCGenerationType {
    GC_GENERATION_TYPE_EDEN,
    GC_GENERATION_TYPE_YOUNG,
    GC_GENERATION_TYPE_OLD,
    GC_GENERATION_TYPE_PERMANENT
};

typedef struct {
    Obj* object;
} GCRememberedEntry;

typedef struct {
    int count;
    int capacity;
    GCRememberedEntry* entries;
} GCRememberedSet;

typedef struct {
    GCGenerationType type;
    Obj* objects;
    GCRememberedSet remSet;
    size_t bytesAllocated;
    size_t heapSize;
} GCGeneration;

struct GC {
    GCGeneration* generations[4];
    int grayCount;
    int grayCapacity;
    Obj** grayStack;
};

#define ALLOCATE(type, count) \
    (type*)reallocate(vm, NULL, 0, sizeof(type) * (count))

#define ALLOCATE_STRUCT(type) (type*)malloc(sizeof(type))

#define FREE(type, pointer) reallocate(vm, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(vm, pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(vm, pointer, sizeof(type) * (oldCount), 0)

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize);
GC* newGC(VM* vm);
void freeGC(VM* vm);
bool rememberedSetGetObject(GCRememberedSet* rememberedSet, Obj* object);
bool rememberedSetPutObject(VM* vm, GCRememberedSet* rememberedSet, Obj* object);
void markRememberedSet(VM* vm, GCGenerationType generation);
void markObject(VM* vm, Obj* object);
void markValue(VM* vm, Value value);
void collectGarbage(VM* vm);
void freeObjects(VM* vm);

#endif // !clox_memory_h