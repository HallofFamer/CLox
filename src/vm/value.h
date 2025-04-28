#pragma once
#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "../common/common.h"

typedef struct Obj Obj;
typedef struct ObjArray ObjArray;
typedef struct ObjClass ObjClass;
typedef struct ObjClosure ObjClosure;
typedef struct ObjDictionary ObjDictionary;
typedef struct ObjException ObjException;
typedef struct ObjFile ObjFile;
typedef struct ObjGenerator ObjGenerator;
typedef struct ObjInstance ObjInstance;
typedef struct ObjModule ObjModule;
typedef struct ObjNamespace ObjNamespace;
typedef struct ObjPromise ObjPromise;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT      ((uint64_t)0x8000000000000000)
#define QNAN          ((uint64_t)0x7ffc000000000000)
#define TAG_NIL       1
#define TAG_FALSE     2
#define TAG_TRUE      3
#define TAG_INT       4
#define TAG_CHAR      5
#define TAG_GENERIC   6
#define TAG_UNDEFINED 7

typedef uint64_t Value;

#define FALSE_VAL           ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL            ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL             ((Value)(uint64_t)(QNAN | TAG_NIL))
#define UNDEFINED_VAL       ((Value)(uint64_t)(QNAN | TAG_UNDEFINED))
#define BOOL_VAL(b)         ((b) ? TRUE_VAL : FALSE_VAL)
#define INT_VAL(i)          ((Value)(QNAN | ((uint64_t)(uint32_t)(i) << 3) | TAG_INT))
#define FLOAT_VAL(f)        numToValue(f)
#define NUMBER_VAL(num)     numToValue(num)
#define OBJ_VAL(obj)        ((Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)))

#define IS_EMPTY(value)     valueIsEmpty(value)
#define IS_NIL(value)       ((value) == NIL_VAL)
#define IS_UNDEFINED(value) ((value) == UNDEFINED_VAL)
#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_INT(value)       valueIsInt(value)
#define IS_FLOAT(value)     (((value) & QNAN) != QNAN)
#define IS_NUMBER(value)    valueIsNum(value)
#define IS_OBJ(value)       (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_INT(value)       ((int)((value) >> 3))
#define AS_FLOAT(value)     valueToFloat(value)
#define AS_NUMBER(value)    valueToNum(value)
#define AS_OBJ(value)       ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

static inline bool valueIsEmpty(Value value) { 
    return IS_NIL(value) || IS_UNDEFINED(value);
}

static inline bool valueIsInt(Value value) {
    return (value & (QNAN | TAG_INT)) == (QNAN | TAG_INT) && ((value | SIGN_BIT) != value);
}

static inline bool valueIsNum(Value value) {
    return IS_FLOAT(value) || valueIsInt(value);
}

static inline double valueToFloat(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline double valueToNum(Value value) {
    return IS_FLOAT(value) ? AS_FLOAT(value) : AS_INT(value);
}

static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool bval;
        int ival;
        double fval;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_INT(value)     ((value).type == VAL_INT)
#define IS_FLOAT(value)   ((value).type == VAL_FLOAT)
#define IS_NUMBER(value)  valueIsNum(value)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define AS_OBJ(value)     ((value).as.obj)
#define AS_BOOL(value)    ((value).as.bval)
#define AS_INT(value)     ((value).as.ival)
#define AS_FLOAT(value)   ((value).as.fval)
#define AS_NUMBER(value)  ((value).as.fval)

#define BOOL_VAL(b)       ((Value){VAL_BOOL, {.bval = b}})
#define NIL_VAL           ((Value){VAL_NIL, {.ival = 0}})
#define INT_VAL(i)        ((Value){VAL_INT, {.ival = i}})
#define FLOAT_VAL(f)      ((Value){VAL_FLOAT, {.fval = f}})
#define NUMBER_VAL(num)   ((Value){VAL_FLOAT, {.fval = num}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

static inline bool valueIsInt(Value value) {
    return value.type == VAL_INT;
}

static inline bool valueIsNum(Value value) {
    return IS_FLOAT(value) || valueIsInt(value);
}

static inline double valueToFloat(Value value) {
    return AS_FLOAT(value);
}

static inline double valueToNum(Value value) {
    return AS_NUMBER(value);
}

static inline Value numToValue(double num) {
    return NUMBER_VAL(num);
}

#endif

typedef struct {
    int capacity;
    int count;
    GCGenerationType generation;
    Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
char* valueToString(VM* vm, Value value);
void initValueArray(ValueArray* array, GCGenerationType generation);
void freeValueArray(VM* vm, ValueArray* array);
void valueArrayWrite(VM* vm, ValueArray* array, Value value);
void valueArrayAddAll(VM* vm, ValueArray* from, ValueArray* to);
void valueArrayPut(VM* vm, ValueArray* array, int index, Value value);
void valueArrayInsert(VM* vm, ValueArray* array, int index, Value value);
int valueArrayFirstIndex(VM* vm, ValueArray* array, Value value);
int valueArrayLastIndex(VM* vm, ValueArray* array, Value value);
Value valueArrayDelete(VM* vm, ValueArray* array, int index);
bool valueArraysEqual(ValueArray* aArray, ValueArray* bArray);
ObjString* valueArrayToString(VM* vm, ValueArray* array);
void printValue(Value value);

#endif // !clox_value_h