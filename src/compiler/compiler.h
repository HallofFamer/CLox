#pragma once
#ifndef clox_compiler_h
#define clox_compiler_h

#include "ast.h"
#include "../vm/object.h"
#include "../vm/vm.h"

void compileAst(Compiler* compiler, Ast* ast);
ObjFunction* compile(VM* vm, const char* source);
void markCompilerRoots(VM* vm);

#endif // !clox_compiler_h