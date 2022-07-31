#pragma once
#ifndef clox_compiler_h
#define clox_compiler_h

#include "common.h"
#include "object.h"
#include "vm.h"

ObjFunction* compile(VM* vm, const char* source);
void markCompilerRoots(VM* vm);

#endif // !clox_compiler_h