#pragma once
#ifndef clox_buffer_h
#define clox_buffer_h

#include "common.h"

#define DECLARE_BUFFER(name, type) \
    typedef struct { \
        int capacity; \
        int count; \
        type* elements; \
    } name; \
    void name##Init(name* buffer); \
    void name##Free(name* buffer); \
    void name##Write(name* buffer, type element); \
    void name##AddAll(name* from, name* to); \
    int name##FirstIndex(name* buffer, type element); \
    int name##LastIndex(name* buffer, type element); \
    void name##Delete(name* buffer, int index); 

#define DEFINE_BUFFER(name, type) \
    void name##Init(name* buffer) { \
        buffer->capacity = 0; \
        buffer->count = 0; \
        buffer->elements = NULL; \
    }\
    \
    void name##Free(name* buffer) { \
        free(buffer->elements); \
        name##Init(buffer); \
    } \
    \
    void name##Write(name* buffer, type element) { \
        if (buffer->capacity < buffer->count + 1) { \
            int oldCapacity = buffer->capacity; \
            buffer->capacity = bufferGrowCapacity(oldCapacity); \
            buffer->elements = (type*)realloc(buffer->elements, sizeof(type) * buffer->capacity); \
            if (buffer->elements == NULL) exit(1); \
        }\
        buffer->elements[buffer->count] = element; \
        buffer->count++; \
    } \
    \
    void name##AddAll(name* from, name* to) { \
        if (from->count == 0) return; \
        for (int i = 0; i < from->count; i++) { \
            name##Write(to, from->elements[i]); \
        } \
    } \
    \
    int name##FirstIndex(name* buffer, type element) { \
        for (int i = 0; i < buffer->count; i++) { \
            if (buffer->elements[i] == element) return i; \
        } \
        return -1; \
    }\
    int name##LastIndex(name* buffer, type element) { \
        for (int i = buffer->count - 1; i >= 0; i--) { \
            if (buffer->elements[i] == element) return i; \
        } \
        return -1;\
    }\
    void name##Delete(name* buffer, int index) { \
        type element = buffer->elements[index]; \
        for (int i = index; i < buffer->count - 1; i++) { \
            buffer->elements[i] = buffer->elements[i + 1]; \
        } \
        buffer->count--; \
        return element; \
    }

static inline int bufferGrowCapacity(int capacity) {
    return capacity < 8 ? 8 : capacity * 2;
}

static inline void* bufferReallocate(void* elements, int newSize) {
    return realloc(elements, newSize);
}

DECLARE_BUFFER(ByteArray, uint8_t)
DECLARE_BUFFER(IntArray, int)
DECLARE_BUFFER(DoubleArray, double)

#endif // !clox_buffer_h