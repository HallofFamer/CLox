#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "os.h"
#include "string.h"
#include "value.h"

void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void freeValueArray(VM* vm, ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

void valueArrayWrite(VM* vm, ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }
  
    array->values[array->count] = value;
    array->count++;
}

void valueArrayAddAll(VM* vm, ValueArray* from, ValueArray* to) {
    if (from->count == 0) return;
    for (int i = 0; i < from->count; i++) {
        valueArrayWrite(vm, to, from->values[i]);
    }
}

int valueArrayFirstIndex(VM* vm, ValueArray* array, Value value) {
    for (int i = 0; i < array->count; i++) {
        if (valuesEqual(array->values[i], value)) {
            return i;
        }
    }
    return -1;
}

int valueArrayLastIndex(VM* vm, ValueArray* array, Value value) {
    for (int i = array->count - 1; i >= 0; i--) {
        if (valuesEqual(array->values[i], value)) {
            return i;
        }
    }
    return -1;
}

void valueArrayInsert(VM* vm, ValueArray* array, int index, Value value) {
    if (IS_OBJ(value)) push(vm, value);
    valueArrayWrite(vm, array, NIL_VAL);
    if (IS_OBJ(value)) pop(vm);

    for (int i = array->count - 1; i > index; i--) {
        array->values[i] = array->values[i - 1];
    }
    array->values[index] = value;
}

Value valueArrayDelete(VM* vm, ValueArray* array, int index) {
    Value value = array->values[index];
    if (IS_OBJ(value)) push(vm, value);

    for (int i = index; i < array->count - 1; i++) {
        array->values[i] = array->values[i + 1];
    }
    array->count--;

    if (IS_OBJ(value)) pop(vm);
    return value;
}

bool valueArraysEqual(ValueArray* aArray, ValueArray* bArray) {
    if (aArray->count != bArray->count) return false;
    for (int i = 0; i < aArray->count; i++) {
        if (aArray->values[i] != bArray->values[i]) return false;
    }
    return true;
}

ObjString* valueArrayToString(VM* vm, ValueArray* array) {
    if (array->count == 0) return copyString(vm, "[]", 2);
    else {
        char string[UINT8_MAX] = "";
        string[0] = '[';
        size_t offset = 1;
        for (int i = 0; i < array->count; i++) {
            char* chars = valueToString(vm, array->values[i]);
            size_t length = strlen(chars);
            memcpy(string + offset, chars, length);
            if (i == array->count - 1) {
                offset += length;
            }
            else {
                memcpy(string + offset + length, ", ", 2);
                offset += length + 2;
            }
        }
        string[offset] = ']';
        string[offset + 1] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
}

void printValue(Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
    }
    else if (IS_NIL(value)) {
        printf("nil");
    }
    else if (IS_INT(value)) {
        printf("%d", AS_INT(value));
    }
    else if (IS_FLOAT(value)) {
        printf("%g", AS_NUMBER(value));
    }
    else if(IS_OBJ(value)) {
        printObject(value);
    }
    else {
        printf("undefined");
    }
#else
    switch (value.type) {
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: printf("nil"); break;
        case VAL_INT: printf("%d", AS_INT(value)); break;
        case VAL_FLOAT: printf("%g", AS_FLOAT(value)); break;
        case VAL_OBJ: printObject(value); break;
        default: printf("undefined");
    }
#endif
}

bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    return a == b;
#else
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL:   
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:    
            return true;
        case VAL_INT:
        case VAL_FLOAT:  
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ:    
            return AS_OBJ(a) == AS_OBJ(b);
        default:         
            return false;
    }
#endif
}

char* valueToString(VM* vm, Value value) {
    if (IS_BOOL(value)) {
        return AS_BOOL(value) ? "true" : "false";
    }
    else if (IS_NIL(value)) {
        return "nil";
    }
    else if (IS_INT(value)) {
        char* chars = ALLOCATE(char, (size_t)11);
        int length = sprintf_s(chars, 11, "%d", AS_INT(value));
        return chars;
    }
    else if (IS_FLOAT(value)) {
        char* chars = ALLOCATE(char, (size_t)24);
        int length = sprintf_s(chars, 24, "%.14g", AS_FLOAT(value));
        return chars;
    }
    else if (IS_OBJ(value)) {
        Obj* object = AS_OBJ(value);
        if (IS_STRING(value)) {
            return AS_CSTRING(value);
        }
        else {
            size_t size = (size_t)(9 + object->klass->name->length);
            char* chars = ALLOCATE(char, size);
            int length = sprintf_s(chars, strlen(chars), "<object %s>", object->klass->name->chars);
            return chars;
        }
    }
    else {
        return "undefined";
    }
}