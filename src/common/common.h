#pragma once
#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING
//#define DEBUG_TRACE_EXECUTION
//#define DEBUG_PRINT_SHAPE
//#define DEBUG_TRACE_CACHE

//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT4_MAX 15
#define UINT4_COUNT (UINT4_MAX + 1)
#define MAX_CASES 256

typedef struct VM VM;
typedef struct CallFrame CallFrame;
typedef struct CompilerV1 CompilerV1;
typedef struct ClassCompilerV1 ClassCompilerV1;
typedef struct Compiler Compiler;
typedef struct GC GC;

typedef enum {
    GC_GENERATION_TYPE_EDEN,
    GC_GENERATION_TYPE_YOUNG,
    GC_GENERATION_TYPE_OLD,
    GC_GENERATION_TYPE_PERMANENT
} GCGenerationType;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

#endif // !clox_common_h