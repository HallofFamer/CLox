#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typechecker.h"
#include "../vm/vm.h"

struct ClassTypeChecker {
    ClassTypeChecker* enclosing;
    Token name;
    BehaviorTypeInfo* type;
    bool isAnonymous;
};

struct FunctionTypeChecker {
    FunctionTypeChecker* enclosing;
    Token name;
    SymbolTable* symtab;
    CallableTypeInfo* type;
    bool isAsync;
};

static void typeError(TypeChecker* typeChecker, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[line %d] Type Error: ", typeChecker->currentToken.line);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    typeChecker->hadError = true;
}

static void initClassTypeChecker(TypeChecker* typeChecker, ClassTypeChecker* klass, Token name, BehaviorTypeInfo* type, bool isAnonymous) {
    klass->enclosing = typeChecker->currentClass;
    klass->name = name;
    klass->type = type;
    klass->isAnonymous = isAnonymous;
    typeChecker->currentClass = klass;
}

static void endClassTypeChecker(TypeChecker* typeChecker) {
    typeChecker->currentClass = typeChecker->currentClass->enclosing;
}

static void initFunctionTypeChecker(TypeChecker* typeChecker, FunctionTypeChecker* function, Token name, CallableTypeInfo* type, bool isAsync) {
    function->enclosing = typeChecker->currentFunction;
    function->name = name;
    function->type = type;
    function->isAsync = isAsync;
    typeChecker->currentFunction = function;
}

static void endFunctionTypeChecker(TypeChecker* typeChecker) {
    typeChecker->currentFunction = typeChecker->currentFunction->enclosing;
}

void initTypeChecker(VM* vm, TypeChecker* typeChecker, bool debugTypetab) {
    typeChecker->vm = vm;
    typeChecker->currentNamespace = emptyString(vm);
    typeChecker->currentClass = NULL;
    typeChecker->currentFunction = NULL;
    typeChecker->scopeDepth = 0;
    typeChecker->debugTypetab = debugTypetab;
    typeChecker->hadError = false;
}

static void typeCheckExpression(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckStatement(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckClassDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckFunDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckMethodDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckNamespaceDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckTraitDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckVarDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
}

static void typeCheckDeclaration(TypeChecker* typeChecker, Ast* ast) {
    switch (ast->kind) {
        case AST_DECL_CLASS:
            typeCheckClassDeclaration(typeChecker, ast);
            break;
        case AST_DECL_FUN:
            typeCheckFunDeclaration(typeChecker, ast);
            break;
        case AST_DECL_METHOD:
            typeCheckMethodDeclaration(typeChecker, ast);
            break;
        case AST_DECL_NAMESPACE:
            typeCheckNamespaceDeclaration(typeChecker, ast);
            break;
        case AST_DECL_TRAIT:
            typeCheckTraitDeclaration(typeChecker, ast);
            break;
        case AST_DECL_VAR:
            typeCheckVarDeclaration(typeChecker, ast);
            break;
        default:
            typeError(typeChecker, "Invalid AST declaration type.");
        }
}

void typeCheckAst(TypeChecker* typeChecker, Ast* ast) {
    for (int i = 0; i < ast->children->count; i++) {
        typeCheckChild(typeChecker, ast, i);
    }
}

void typeCheckChild(TypeChecker* typeChecker, Ast* ast, int index) {
    Ast* child = astGetChild(ast, index);
    child->symtab = ast->symtab;
    typeChecker->currentToken = child->token;

    switch (child->category) {
        case AST_CATEGORY_SCRIPT:
        case AST_CATEGORY_OTHER:
            typeCheckAst(typeChecker, child);
            break;
        case AST_CATEGORY_EXPR:
            typeCheckExpression(typeChecker, child);
            break;
        case AST_CATEGORY_STMT:
            typeCheckStatement(typeChecker, child);
            break;
        case AST_CATEGORY_DECL:
            typeCheckDeclaration(typeChecker, child);
            break;
        default:
            typeError(typeChecker, "Invalid AST category.");
    }
}

void typeCheck(TypeChecker* typeChecker, Ast* ast) {
    FunctionTypeChecker functionTypeChecker;
    initFunctionTypeChecker(typeChecker, &functionTypeChecker, syntheticToken("script"), NULL, ast->modifier.isAsync);
    typeCheckAst(typeChecker, ast);

    endFunctionTypeChecker(typeChecker);
    if (typeChecker->debugTypetab) {
        typeTableOutput(typeChecker->vm->typetab);
    }
}