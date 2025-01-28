#pragma once
#ifndef clox_typechecker_h
#define clox_typechecker_h

#include "ast.h"

typedef struct ClassTypeChecker ClassTypeChecker;
typedef struct FunctionTypeChecker FunctionTypeChecker;

typedef struct {
    VM* vm;
    Token currentToken;
    ObjString* currentNamespace;
    ClassTypeChecker* currentClass;
    FunctionTypeChecker* currentFunction;
    int scopeDepth;
    bool debugTypetab;
    bool hadError;
} TypeChecker;

void initTypeChecker(VM* vm, TypeChecker* typeChecker, bool debugTypetab);
void typeCheckAst(TypeChecker* typeChecker, Ast* ast);
void typeCheckChild(TypeChecker* typeChecker, Ast* ast, int index);
void typeCheck(TypeChecker* typeChecker, Ast* ast);

#endif // !clox_typechecker_h