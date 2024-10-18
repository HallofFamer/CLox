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
    resolver->symtab = NULL;
    resolver->numSlots = 1;
    resolver->debugSymtab = debugSymtab;
    resolver->hadError = false;
}

static SymbolItem* insertSymbol(Resolver* resolver, Token token, SymbolCategory category, SymbolState state) {
    ObjString* symbol = copyString(resolver->vm, token.start, token.length);
    SymbolItem* item = newSymbolItem(token, category, state, resolver->symtab->count++);
    bool inserted = symbolTableSet(resolver->symtab, symbol, item);

    if (inserted) return item;
    else {
        free(item);
        return NULL;
    }
}

static void beginScope(Resolver* resolver, Ast* ast, SymbolScope scope) {
    ast->symtab = newSymbolTable(ast->symtab, scope, ast->symtab->scope + 1);
}

static void endScope(Resolver* resolver, Ast* ast) {
    if (resolver->debugSymtab) symbolTableOutput(ast->symtab);
    resolver->symtab = resolver->symtab->parent;
}

static void declareVariable(Resolver* resolver, Token token) {
    SymbolCategory category = (resolver->symtab->scope == SYMBOL_SCOPE_GLOBAL) ? SYMBOL_CATEGORY_GLOBAL : SYMBOL_CATEGORY_LOCAL;
    SymbolItem* item = insertSymbol(resolver, token, category, SYMBOL_STATE_DECLARED);
    if (item == NULL) semanticError(resolver, "Already a variable with this name in this scope.");
}

static void defineVariable(Resolver* resolver, Token token) {
    ObjString* symbol = copyString(resolver->vm, token.start, token.length);
    SymbolItem* item = symbolTableLookup(resolver->symtab, symbol);
    if (item == NULL) semanticError(resolver, "Variable %s does not exist in this scope.");
    else item->state = SYMBOL_STATE_DEFINED;
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
    declareVariable(resolver, ast->token);
    bool hasValue = astHasChild(ast);
    if (hasValue) {
        resolveChild(resolver, ast, 0);
        defineVariable(resolver, ast->token);
    }
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
    ast->symtab = resolver->symtab;
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
    resolver->symtab = newSymbolTable(resolver->vm->symtab, SYMBOL_SCOPE_MODULE, 0);
    resolveAst(resolver, ast);
    if (resolver->debugSymtab) symbolTableOutput(resolver->symtab);
}