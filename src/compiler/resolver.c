#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resolver.h"
#include "../vm/vm.h"

struct ClassResolver {
    ClassResolver* enclosing;
    Token name;
    Token superClass;
    int currentScope;
    BehaviorType type;
};

struct FunctionResolver {
    FunctionResolver* enclosing;
    Token name;
    int currentScope;
    int numSlots;
    bool isMethod;
    bool isLambda;
    bool isGenerator;
    bool isAsync;
};

static int currentLine(Resolver* resolver) {
    return resolver->currentToken.line;
}

static void semanticError(Resolver* resolver, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[line %d] Semantic Error: ", currentLine(resolver));
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
    resolver->hadError = true;
}

void initResolver(VM* vm, Resolver* resolver, bool debugSymtab) {
    resolver->vm = vm;
    resolver->currentClass = NULL;
    resolver->currentFunction = NULL;
    resolver->currentScope = 0;
    resolver->numSlots = 1;
    resolver->debugSymtab = debugSymtab;
    resolver->hadError = false;
}

static int nextSymbolIndex(Resolver* resolver) {
    return (resolver->currentFunction == NULL) ? resolver->numSlots++ : resolver->currentFunction->numSlots++;
}

static bool insertSymbol(Resolver* resolver, Ast* ast, SymbolCategory category) {
    uint8_t index = (uint8_t)nextSymbolIndex(resolver);
    ObjString* symbol = copyString(resolver->vm, ast->token.start, ast->token.length);
    SymbolItem* item = newSymbolItem(ast->token, category, index);
    return symbolTableSet(ast->symtab, symbol, item);
}

static void beginScope(Resolver* resolver, Ast* ast, SymbolScope scope) {
    ast->symtab = newSymbolTable(ast->symtab, scope);
    resolver->currentScope++;
}

static void endScope(Resolver* resolver) {
    resolver->currentScope--;
}

static void declareVariable(Resolver* resolver, Ast* ast) {
    if (!insertSymbol(resolver, ast, resolver->currentScope == 0 ? SYMBOL_CATEGORY_GLOBAL : SYMBOL_CATEGORY_LOCAL)) {
        semanticError(resolver, "Already a variable with this name in this scope.");
    }
    // To be implemented
}

static void resolveExpression(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveClassDeclaration(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveFunDeclaration(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveMethodDeclaration(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveNamespaceDeclaration(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveTraitDeclaration(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveVarDeclaration(Resolver* resolver, Ast* ast) {
    //declareVariable(resolver, ast);
    bool hasValue = astHasChild(ast);
    if (hasValue) resolveChild(resolver, ast, 0);
    else if (!ast->modifier.isMutable) {
        semanticError(resolver, "Immutable variable must be initialized upon declaration.");
    }
}

static void resolveDeclaration(Resolver* resolver, Ast* ast) {
    switch (ast->type) {
        case AST_DECL_CLASS:
            resolveClassDeclaration(resolver, ast);
            break;
        case AST_DECL_FUN:
            resolveFunDeclaration(resolver, ast);
            break;
        case AST_DECL_METHOD:
            resolveMethodDeclaration(resolver, ast);
            break;
        case AST_DECL_NAMESPACE:
            resolveNamespaceDeclaration(resolver, ast);
            break;
        case AST_DECL_TRAIT:
            resolveTraitDeclaration(resolver, ast);
            break;
        case AST_DECL_VAR:
            resolveVarDeclaration(resolver, ast);
            break;
        default:
            semanticError(resolver, "Invalid AST declaration type.");
    }
}

void resolveAst(Resolver* resolver, Ast* ast) {
    for (int i = 0; i < ast->children->count; i++) {
        resolveChild(resolver, ast, i);
    }
}

static void resolveChild(Resolver* resolver, Ast* ast, int index) {
    Ast* child = astGetChild(ast, index);
    resolver->currentToken = child->token;
    switch (child->category) {
        case AST_CATEGORY_SCRIPT:
        case AST_CATEGORY_OTHER:
            resolveAst(resolver, child);
            break;
        case AST_CATEGORY_EXPR:
            resolveExpression(resolver, child);
            break;
        case AST_CATEGORY_STMT:
            resolveStatement(resolver, child);
            break;
        case AST_CATEGORY_DECL:
            resolveDeclaration(resolver, child);
            break;
        default:
            semanticError(resolver, "Invalid AST category.");
    }
}

void resolve(Resolver* resolver, Ast* ast) {
    ast->symtab = newSymbolTable(NULL, SYMBOL_SCOPE_MODULE);
    resolveAst(resolver, ast);
}