#ifndef clox_value_h
#define clox_value_h

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjClass ObjClass;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

#define SIGN_BIT  ((uint64_t)0x8000000000000000)
#define QNAN      ((uint64_t)0x7ffc000000000000)
#define TAG_NIL   1
#define TAG_FALSE 2
#define TAG_TRUE  3
#define TAG_INT   4

typedef uint64_t Value;

#define FALSE_VAL        ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL         ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NIL_VAL          ((Value)(uint64_t)(QNAN | TAG_NIL))
#define BOOL_VAL(b)      ((b) ? TRUE_VAL : FALSE_VAL)
#define INT_VAL(i)       ((Value)(QNAN | ((uint64_t)(uint32_t)(i) << 3) | TAG_INT))
#define DOUBLE_VAL(d)    numToValue(num)
#define NUMBER_VAL(num)  numToValue(num)
#define OBJ_VAL(obj)     ((Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj)))

#define IS_NIL(value)    ((value) == NIL_VAL)
#define IS_BOOL(value)   (((value) | 1) == TRUE_VAL)
#define IS_INT(value)    valueIsInt(value)
#define IS_DOUBLE(value) (((value) & QNAN) != QNAN)
#define IS_NUMBER(value) valueIsNum(value)
#define IS_OBJ(value)    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)   ((value) == TRUE_VAL)
#define AS_INT(value)    ((int)(value >> 3))
#define AS_DOUBLE(value) valueToDouble(value)
#define AS_NUMBER(value) valueToNum(value)
#define AS_OBJ(value)    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

static inline bool valueIsInt(Value value) {
    return (value & (QNAN | TAG_INT)) == (QNAN | TAG_INT) && ((value | SIGN_BIT) != value);
}

static inline bool valueIsNum(Value value) {
    return IS_DOUBLE(value) || valueIsInt(value);
}

static inline double valueToDouble(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static inline double valueToNum(Value value) {
    return IS_DOUBLE(value) ? AS_DOUBLE(value) : AS_INT(value);
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
    VAL_NUMBER,
    VAL_OBJ
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define AS_OBJ(value)     ((value).as.obj)
#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define BOOL_VAL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

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
    Value* values;
} ValueArray;

bool valuesEqual(Value a, Value b);
void initValueArray(ValueArray* array);
void writeValueArray(VM* vm, ValueArray* array, Value value);
void freeValueArray(VM* vm, ValueArray* array);
void printValue(Value value);

#endif // !clox_value_h