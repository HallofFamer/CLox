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
typedef enum InterpretResult InterpretResult;
typedef struct CallFrame CallFrame;
typedef struct CompilerV1 CompilerV1;
typedef struct ClassCompilerV1 ClassCompilerV1;
typedef struct Compiler Compiler;

#endif // !clox_common_h
