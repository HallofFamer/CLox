#pragma once
#ifndef clox_compiler_v1_h
#define clox_compiler_v1_h

#include "object.h"
#include "vm.h"

ObjFunction* compileV1(VM* vm, const char* source);
void markCompilerRoots(VM* vm);

#endif // !clox_compiler_v1_h