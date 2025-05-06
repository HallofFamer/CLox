#pragma once
#ifndef clox_memory_h
#define clox_memory_h

#include "object.h"
#include "value.h"
#include "vm.h"

#define GC_GENERATION_TYPE_COUNT 4

typedef struct {
    Obj* object;
} GCRememberedEntry;

typedef struct {
    int count;
    int capacity;
    GCGenerationType generation;
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

#define ALLOCATE(type, count, generation) \
    (type*)reallocate(vm, NULL, 0, sizeof(type) * (count), generation)

#define ALLOCATE_STRUCT(type) (type*)malloc(sizeof(type))

#define FREE(type, pointer, generation) reallocate(vm, pointer, sizeof(type), 0, generation)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount, generation) \
    (type*)reallocate(vm, pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount), generation)

#define FREE_ARRAY(type, pointer, oldCount, generation) \
    reallocate(vm, pointer, sizeof(type) * (oldCount), 0, generation)

#define GET_GC_GENERATION(generation) vm->gc->generations[generation]

#define PROCESS_WRITE_BARRIER(source, target) \
    do { \
       if (sourceOlderThanTarget(source, target)) { \
           GCGenerationType generation = OBJ_GEN(target); \
           addToRememberedSet(vm, source, generation); \
       } \
    } while (false)

void* reallocate(VM* vm, void* pointer, size_t oldSize, size_t newSize, GCGenerationType generation);
GC* newGC(VM* vm);
void freeGC(VM* vm);
void addToRememberedSet(VM* vm, Obj* object, GCGenerationType generation);
void markObject(VM* vm, Obj* object, GCGenerationType generation);
void markValue(VM* vm, Value value, GCGenerationType generation);
void markRememberedSet(VM* vm, GCGenerationType generation);
void collectGarbage(VM* vm, GCGenerationType generation);
void freeObjects(VM* vm);

static inline bool sourceOlderThanTarget(Obj* source, Value target) {
    return IS_OBJ(target) && (source->generation > OBJ_GEN(target));
}

#endif // !clox_memory_h