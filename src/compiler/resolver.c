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
    int numLocals;
    int numUpvalues;
    int numGlobals;
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
    function->numLocals = 0;
    function->numGlobals = 0;
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
    resolver->tryDepth = 0;
    resolver->debugSymtab = debugSymtab;
    resolver->hadError = false;
}

static uint8_t nextSymbolIndex(Resolver* resolver, SymbolCategory category) {
    switch (category) {
        case SYMBOL_CATEGORY_LOCAL:
            return ++resolver->currentFunction->numLocals;
        case SYMBOL_CATEGORY_UPVALUE:
            return resolver->currentFunction->numUpvalues++;
        case SYMBOL_CATEGORY_GLOBAL:
            return resolver->currentFunction->numGlobals++;
        default:
            semanticError(resolver, "Invalid symbol category specified.");
            return -1;
    }
}

static SymbolItem* insertSymbol(Resolver* resolver, Token token, SymbolCategory category, SymbolState state, bool isMutable) {
    ObjString* symbol = copyString(resolver->vm, token.start, token.length);
    uint8_t index = nextSymbolIndex(resolver, category);
    SymbolItem* item = newSymbolItem(token, category, state, index, isMutable);
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

static void markVariableInitialized(Resolver* resolver, SymbolItem* item) {
    item->state = resolver->currentFunction->modifier.isScript ? SYMBOL_STATE_ACCESSED : SYMBOL_STATE_DEFINED;
}

static SymbolItem* declareVariable(Resolver* resolver, Ast* ast, bool isMutable) {
    SymbolCategory category = symbolScopeToCategory(resolver->symtab->scope);
    SymbolItem* item = insertSymbol(resolver, ast->token, category, SYMBOL_STATE_DECLARED, isMutable);
    if (item == NULL) semanticError(resolver, "Already a variable with this name in this scope.");
    return item;
}

static SymbolItem* defineVariable(Resolver* resolver, Ast* ast) {
    ObjString* symbol = copyString(resolver->vm, ast->token.start, ast->token.length);
    SymbolItem* item = symbolTableLookup(resolver->symtab, symbol);
    if (item == NULL) semanticError(resolver, "Variable %s does not exist in this scope.");
    else markVariableInitialized(resolver, item);
    return item;
}

static void yield(Resolver* resolver, Ast* ast) {
    if (resolver->currentFunction->modifier.isScript) {
        semanticError(resolver, "Can't yield from top-level code.");
    }
    else if (resolver->currentFunction->modifier.isInitializer) {
        semanticError(resolver, "Cannot yield from an initializer.");
    }

    resolver->currentFunction->modifier.isGenerator = true;
    if (astHasChild(ast)) {
        resolveChild(resolver, ast, 0);
    }
}

static void await(Resolver* resolver, Ast* ast) {
    if (resolver->currentFunction->modifier.isScript) {
        resolver->currentFunction->modifier.isAsync = true;
    }
    else if (!resolver->currentFunction->modifier.isAsync) {
        semanticError(resolver, "Cannot use await unless in top level code or inside async functions/methods.");
    }
    resolveChild(resolver, ast, 0);
}

static void resolveAnd(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveArray(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveAssign(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveAwait(Resolver* resolver, Ast* ast) {
    await(resolver, ast);
}

static void resolveBinary(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveCall(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveClass(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveDictionary(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveFunction(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveGrouping(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveInterpolation(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveInvoke(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveLiteral(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveNil(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveOr(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveParam(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolvePropertyGet(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolvePropertySet(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveSubscriptGet(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveSubscriptSet(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveSuperGet(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveSuperInvoke(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveThis(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveTrait(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveUnary(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveVariable(Resolver* resolver, Ast* ast) {
    // To be implemented
}

static void resolveYield(Resolver* resolver, Ast* ast) {
    yield(resolver, ast);
}

static void resolveExpression(Resolver* resolver, Ast* ast) {
    switch (ast->type) {
        case AST_EXPR_AND:
            resolveAnd(resolver, ast);
            break;
        case AST_EXPR_ARRAY:
            resolveArray(resolver, ast);
            break;
        case AST_EXPR_ASSIGN:
            resolveAssign(resolver, ast);
            break;
        case AST_EXPR_AWAIT:
            resolveAwait(resolver, ast);
            break;
        case AST_EXPR_BINARY:
            resolveBinary(resolver, ast);
            break;
        case AST_EXPR_CALL:
            resolveCall(resolver, ast);
            break;
        case AST_EXPR_CLASS:
            resolveClass(resolver, ast);
            break;
        case AST_EXPR_DICTIONARY:
            resolveDictionary(resolver, ast);
            break;
        case AST_EXPR_FUNCTION:
            resolveFunction(resolver, ast);
            break;
        case AST_EXPR_GROUPING:
            resolveGrouping(resolver, ast);
            break;
        case AST_EXPR_INTERPOLATION:
            resolveInterpolation(resolver, ast);
            break;
        case AST_EXPR_INVOKE:
            resolveInvoke(resolver, ast);
            break;
        case AST_EXPR_LITERAL:
            resolveLiteral(resolver, ast);
            break;
        case AST_EXPR_NIL:
            resolveNil(resolver, ast);
            break;
        case AST_EXPR_OR:
            resolveOr(resolver, ast);
            break;
        case AST_EXPR_PARAM:
            resolveParam(resolver, ast);
            break;
        case AST_EXPR_PROPERTY_GET:
            resolvePropertyGet(resolver, ast);
            break;
        case AST_EXPR_PROPERTY_SET:
            resolvePropertySet(resolver, ast);
            break;
        case AST_EXPR_SUBSCRIPT_GET:
            resolveSubscriptGet(resolver, ast);
            break;
        case AST_EXPR_SUBSCRIPT_SET:
            resolveSubscriptSet(resolver, ast);
            break;
        case AST_EXPR_SUPER_GET:
            resolveSuperGet(resolver, ast);
            break;
        case AST_EXPR_SUPER_INVOKE:
            resolveSuperInvoke(resolver, ast);
            break;
        case AST_EXPR_THIS:
            resolveThis(resolver, ast);
            break;
        case AST_EXPR_TRAIT:
            resolveTrait(resolver, ast);
            break;
        case AST_EXPR_UNARY:
            resolveUnary(resolver, ast);
            break;
        case AST_EXPR_VARIABLE:
            resolveVariable(resolver, ast);
            break;
        case AST_EXPR_YIELD:
            resolveYield(resolver, ast);
            break;
        default:
            semanticError(resolver, "Invalid AST expression type.");
    }
}

static void resolverAwaitStatement(Resolver* resolver, Ast* ast) {
    await(resolver, ast);
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
    Ast* _namespace = astGetChild(ast, 0);
    int namespaceDepth = astNumChild(_namespace);
    uint8_t index = 0;

    for (int i = 0; i < namespaceDepth; i++) {
        Ast* subNamespace = astGetChild(_namespace, i);
        insertSymbol(resolver, subNamespace->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, false);
    }

    if (astNumChild(ast) > 1) {
        Ast* alias = astGetChild(ast, 1);
        insertSymbol(resolver, alias->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, false);
    }
}

static void resolveWhileStatement(Resolver* resolver, Ast* ast) {
    resolver->loopDepth++;
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
    resolver->loopDepth--;
}

static void resolveYieldStatement(Resolver* resolver, Ast* ast) {
    yield(resolver, ast);
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
    SymbolItem* item = declareVariable(resolver, ast, false);
    resolveChild(resolver, ast, 0);
    markVariableInitialized(resolver, item);
}

static void resolveFunDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, false);
    resolveChild(resolver, ast, 0);
    markVariableInitialized(resolver, item);
}

static void resolveMethodDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, false);
    resolveChild(resolver, ast, 0);
    markVariableInitialized(resolver, item);
}

static void resolveNamespaceDeclaration(Resolver* resolver, Ast* ast) {
    uint8_t namespaceDepth = 0;
    Ast* identifiers = astGetChild(ast, 0);
    while (namespaceDepth < identifiers->children->count) {
        Ast* identifier = astGetChild(identifiers, namespaceDepth);
        insertSymbol(resolver, identifier->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, false);
        namespaceDepth++;
    }
}

static void resolveTraitDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, false);
    resolveChild(resolver, ast, 0);
    markVariableInitialized(resolver, item);
}

static void resolveVarDeclaration(Resolver* resolver, Ast* ast) {
    declareVariable(resolver, ast, ast->modifier.isMutable);
    bool hasValue = astHasChild(ast);
    if (hasValue) {
        resolveChild(resolver, ast, 0);
        defineVariable(resolver, ast);
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