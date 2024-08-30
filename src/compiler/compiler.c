#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "parser.h"

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
    bool isMutable;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
    bool isMutable;
} Upvalue;

typedef enum {
    COMPILE_TYPE_FUNCTION,
    COMPILE_TYPE_INITIALIZER,
    COMPILE_TYPE_LAMBDA,
    COMPILE_TYPE_METHOD,
    COMPILE_TYPE_SCRIPT
} CompileType;

struct Compiler {
    VM* vm;
    struct Compiler* enclosing;
    CompileType type;
    ObjFunction* function;

    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    IDMap indexes;

    int scopeDepth;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
    bool isAsync;
    bool hadError;
};

static void initCompiler(VM* vm, Compiler* compiler, Compiler* enclosing, CompileType type, bool isAsync) {
    compiler->vm = vm;
    compiler->enclosing = enclosing;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->innermostLoopStart = -1;
    compiler->innermostLoopScopeDepth = 0;
    compiler->hadError = false;
}

static ObjFunction* endCompiler(Compiler* compiler) {
    // To be implemented
    return NULL;
}

static void compileStatement(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileClassDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileFunDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileMethodDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileNamespaceDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileTraitDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileVarDeclaration(Compiler* compiler, Ast* ast) {
    // To be implemented
    return NULL;
}

static void compileDeclaration(Compiler* compiler, Ast* ast) {
    switch (ast->type) {
        case AST_DECL_CLASS:
            compileClassDeclaration(compiler, ast);
            break;
        case AST_DECL_FUN:
            compileFunDeclaration(compiler, ast);
            break;
        case AST_DECL_METHOD:
            compileMethodDeclaration(compiler, ast);
            break;
        case AST_DECL_NAMESPACE:
            compileNamespaceDeclaration(compiler, ast);
            break;
        case AST_DECL_TRAIT:
            compileTraitDeclaration(compiler, ast);
            break;
        case AST_DECL_VAR:
            compileVarDeclaration(compiler, ast);
            break;
        default: 
            compileStatement(compiler, ast);
    }
}

static void compileScript(Compiler* compiler, Ast* ast) {
    if (compiler->hadError || !AST_IS_ROOT(ast)) return;
    for (int i = 0; i < ast->children->count; i++) {
        Ast* decl = ast->children->elements[i];
        compileDeclaration(compiler, decl);
    }
}

ObjFunction* compile(VM* vm, const char* source) {
    Lexer lexer;
    initLexer(&lexer, source);

    Parser parser;
    initParser(&parser, &lexer);
    Ast* ast = parse(&parser);
    if (parser.hadError) return NULL;
    
    Compiler compiler;
    initCompiler(vm, &compiler, NULL, COMPILE_TYPE_SCRIPT, false);
    compileScript(&compiler, ast);
    ObjFunction* function = endCompiler(&compiler);
    return function;
}