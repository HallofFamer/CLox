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
    void name##Add(name* buffer, type element); \
    void name##AddAll(name* from, name* to); \
    int name##FirstIndex(name* buffer, type element); \
    int name##LastIndex(name* buffer, type element); \
    type name##Delete(name* buffer, int index); 

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
    void name##Add(name* buffer, type element) { \
        if (buffer->capacity < buffer->count + 1) { \
            int oldCapacity = buffer->capacity; \
            buffer->capacity = bufferGrowCapacity(oldCapacity); \
            type* elements = (type*)realloc(buffer->elements, sizeof(type) * buffer->capacity); \
            if(elements != NULL) buffer->elements = elements; \
            else exit(1); \
        }\
        buffer->elements[buffer->count] = element; \
        buffer->count++; \
    } \
    \
    void name##AddAll(name* from, name* to) { \
        if (from->count == 0) return; \
        for (int i = 0; i < from->count; i++) { \
            name##Add(to, from->elements[i]); \
        } \
    } \
    \
    int name##FirstIndex(name* buffer, type element) { \
        for (int i = 0; i < buffer->count; i++) { \
            if (buffer->elements[i] == element) return i; \
        } \
        return -1; \
    }\
    \
    int name##LastIndex(name* buffer, type element) { \
        for (int i = buffer->count - 1; i >= 0; i--) { \
            if (buffer->elements[i] == element) return i; \
        } \
        return -1;\
    }\
    \
    type name##Delete(name* buffer, int index) { \
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

DECLARE_BUFFER(ByteArray, uint8_t)
DECLARE_BUFFER(IntArray, int)
DECLARE_BUFFER(DoubleArray, double)
DECLARE_BUFFER(StringArray, char*)

#endif // !clox_buffer_h