#pragma once
#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NAN_BOXING
//#define DEBUG_PRINT_CODE
//#define DEBUG_TRACE_EXECUTION

#define DEBUG_PRINT_SHAPE
#define DEBUG_TRACE_CACHE

//#define DEBUG_STRESS_GC
//#define DEBUG_LOG_GC

#define UINT8_COUNT (UINT8_MAX + 1)
#define UINT4_MAX 15
#define UINT4_COUNT (UINT4_MAX + 1)
#define MAX_CASES 256

typedef struct VM VM;
typedef struct Compiler Compiler;
typedef struct ClassCompiler ClassCompiler;

#endif // !clox_common_h
