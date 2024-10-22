#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resolver.h"
#include "../vm/vm.h"

typedef struct {
    bool isAsync;
    bool isClass;
    bool isGenerator;
    bool isInitializer;
    bool isLambda;
    bool isMethod;
    bool isScript;
} ResolverModifier;

struct ClassResolver {
    ClassResolver* enclosing;
    Token name;
    Token superClass;
    int scopeDepth;
    BehaviorType type;
};

struct FunctionResolver {
    FunctionResolver* enclosing;
    Token name;
    int scopeDepth;
    int numSlots;
    ResolverModifier modifier;
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

static ResolverModifier resolverInitModifier() {
    ResolverModifier modifier = {
        .isAsync = false,
        .isClass = false,
        .isGenerator = false,
        .isInitializer = false,
        .isLambda = false,
        .isMethod = false,
        .isScript = false
    };
    return modifier;
}

static void initFunctionResolver(Resolver* resolver, FunctionResolver* function, FunctionResolver* enclosing, Token name, int scopeDepth) {
    function->enclosing = enclosing;
    function->name = name;
    function->numSlots = 0;
    function->scopeDepth = scopeDepth;
    function->modifier = resolverInitModifier();
    resolver->currentFunction = function;
}

static void endFunctionResolver(Resolver* resolver) {
    resolver->currentFunction = resolver->currentFunction->enclosing;
}

void initResolver(VM* vm, Resolver* resolver, bool debugSymtab) {
    resolver->vm = vm;
    resolver->currentClass = NULL;
    resolver->currentFunction = NULL;
    resolver->symtab = NULL;
    resolver->loopDepth = 0;
    resolver->switchDepth = 0;
    resolver->debugSymtab = debugSymtab;
    resolver->hadError = false;
}

static SymbolItem* insertSymbol(Resolver* resolver, Token token, SymbolCategory category, SymbolState state) {
    ObjString* symbol = copyString(resolver->vm, token.start, token.length);
    uint8_t index = (category == SYMBOL_CATEGORY_LOCAL) ? ++resolver->currentFunction->numSlots : 0;
    SymbolItem* item = newSymbolItem(token, category, state, index);
    bool inserted = symbolTableSet(resolver->symtab, symbol, item);

    if (inserted) return item;
    else {
        free(item);
        return NULL;
    }
}

static void beginScope(Resolver* resolver, Ast* ast, SymbolScope scope) {
    resolver->symtab = newSymbolTable(resolver->symtab, scope, resolver->symtab->depth + 1);
    ast->symtab = resolver->symtab;
}

static void endScope(Resolver* resolver, Ast* ast) {
    if (resolver->debugSymtab) symbolTableOutput(resolver->symtab);
    resolver->symtab = resolver->symtab->parent;
}

static void declareVariable(Resolver* resolver, Token token) {
    SymbolCategory category = symbolScopeToCategory(resolver->symtab->scope);
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

static void resolverAwaitStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveBlockStatement(Resolver* resolver, Ast* ast) {
    beginScope(resolver, ast, SYMBOL_SCOPE_BLOCK);
    Ast* stmts = astGetChild(ast, 0);
    for (int i = 0; i < stmts->children->count; i++) {
        resolveChild(resolver, stmts, i);
    }
    endScope(resolver, ast);
}

static void resolveBreakStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveCaseStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveCatchStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveContinueStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveDefaultStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveExpressionStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveFinallyStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveForStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveIfStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveRequireStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveReturnStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveSwitchStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveThrowStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveTryStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveUsingStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveWhileStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveYieldStatement(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveStatement(Resolver* resolver, Ast* ast) {
    switch (ast->type) {
        case AST_STMT_AWAIT:
            resolverAwaitStatement(resolver, ast);
            break;
        case AST_STMT_BLOCK:
            resolveBlockStatement(resolver, ast);
            break;
        case AST_STMT_BREAK:
            resolveBreakStatement(resolver, ast);
            break;
        case AST_STMT_CASE:
            resolveCaseStatement(resolver, ast);
            break;
        case AST_STMT_CATCH:
            resolveCatchStatement(resolver, ast);
            break;
        case AST_STMT_CONTINUE:
            resolveContinueStatement(resolver, ast);
            break;
        case AST_STMT_DEFAULT:
            resolveDefaultStatement(resolver, ast);
            break;
        case AST_STMT_EXPRESSION:
            resolveExpressionStatement(resolver, ast);
            break;
        case AST_STMT_FINALLY:
            resolveFinallyStatement(resolver, ast);
            break;
        case AST_STMT_FOR:
            resolveForStatement(resolver, ast);
            break;
        case AST_STMT_IF:
            resolveIfStatement(resolver, ast);
            break;
        case AST_STMT_REQUIRE:
            resolveRequireStatement(resolver, ast);
            break;
        case AST_STMT_RETURN:
            resolveReturnStatement(resolver, ast);
            break;
        case AST_STMT_SWITCH:
            resolveSwitchStatement(resolver, ast);
            break;
        case AST_STMT_THROW:
            resolveThrowStatement(resolver, ast);
            break;
        case AST_STMT_TRY:
            resolveTryStatement(resolver, ast);
            break;
        case AST_STMT_USING:
            resolveUsingStatement(resolver, ast);
            break;
        case AST_STMT_WHILE:
            resolveWhileStatement(resolver, ast);
            break;
        case AST_STMT_YIELD:
            resolveYieldStatement(resolver, ast);
            break;
        default:
            semanticError(resolver, "Invalid AST statement type.");
    }
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
    for (int i = 0; i < ast->children->count; i++) {
        resolveChild(resolver, ast, i);
    }
}

static void resolveChild(Resolver* resolver, Ast* ast, int index) {
    Ast* child = astGetChild(ast, index);
    resolver->currentToken = child->token;
    child->symtab = ast->symtab;

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
    FunctionResolver functionResolver;
    initFunctionResolver(resolver, &functionResolver, NULL, syntheticToken("script"), 0);
    resolver->currentFunction->modifier.isScript = true;
    resolver->symtab = newSymbolTable(resolver->vm->symtab, SYMBOL_SCOPE_MODULE, 0);
    resolveAst(resolver, ast);
    endFunctionResolver(resolver);
    if (resolver->debugSymtab) symbolTableOutput(resolver->symtab);
}