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

struct Compiler {
    VM* vm;
    struct Compiler* enclosing;
    Ast* ast;
    ObjFunction* function;

    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    IDMap indexes;

    int scopeDepth;
    int innermostLoopStart;
    int innermostLoopScopeDepth;
    bool isAsync;
};

static void initCompiler(VM* vm, Compiler* compiler, Compiler* enclosing, Ast* ast, bool isAsync) {
    compiler->vm = vm;
    compiler->enclosing = enclosing;
    compiler->ast = ast;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->innermostLoopStart = -1;
    compiler->innermostLoopScopeDepth = 0;
}

static ObjFunction* endCompiler(Compiler* compiler) {
    // To be implemented
    return NULL;
}

static void emitOpCode(Compiler* compiler) {
    // To be implemented
    return NULL;
}

ObjFunction* compile(VM* vm, const char* source) {
    Lexer lexer;
    initLexer(&lexer, source);

    Parser parser;
    initParser(&parser, &lexer);
    Ast* ast = parse(&parser);
    if (parser.hadError) return NULL;
    
    Compiler compiler;
    initCompiler(vm, &compiler, NULL, ast, false);
    emitOpCode(&compiler);
    ObjFunction* function = endCompiler(&compiler);
    return function;
}