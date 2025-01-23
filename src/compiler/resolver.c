#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resolver.h"
#include "../vm/native.h"
#include "../vm/vm.h"

typedef struct {
    bool isAsync;
    bool isClassMethod;
    bool isGenerator;
    bool isInitializer;
    bool isInstanceMethod;
    bool isLambda;
    bool isVariadic;
} ResolverModifier;

struct ClassResolver {
    ClassResolver* enclosing;
    Token name;
    Token superClass;
    SymbolTable* symtab;
    int scopeDepth;
    bool isAnonymous;
    BehaviorType type;
};

struct FunctionResolver {
    FunctionResolver* enclosing;
    Token name;
    SymbolTable* symtab;
    int scopeDepth;
    int numLocals;
    int numUpvalues;
    int numGlobals;
    ResolverModifier modifier;
};

static int currentLine(Resolver* resolver) {
    return resolver->currentToken.line;
}

static void semanticWarning(Resolver* resolver, const char* format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "[line %d] Semantic Warning: ", currentLine(resolver));
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);
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
        .isClassMethod = false,
        .isGenerator = false,
        .isInitializer = false,
        .isInstanceMethod = false,
        .isLambda = false,
        .isVariadic = false
    };
    return modifier;
}

static void initClassResolver(Resolver* resolver, ClassResolver* klass, Token name, int scopeDepth, BehaviorType type) {
    klass->enclosing = resolver->currentClass;
    klass->name = name;
    klass->symtab = NULL;
    klass->scopeDepth = scopeDepth;
    klass->isAnonymous = (name.length == 1 && name.start[0] == '@');
    klass->type = type;
    resolver->currentClass = klass;
}

static void endClassResolver(Resolver* resolver) {
    resolver->currentClass = resolver->currentClass->enclosing;
}

static void initFunctionResolver(Resolver* resolver, FunctionResolver* function, Token name, int scopeDepth) {
    function->enclosing = resolver->currentFunction;
    function->name = name;
    function->symtab = NULL;
    function->numLocals = 0;
    function->numUpvalues = 0;
    function->numGlobals = 0;
    function->scopeDepth = scopeDepth;
    function->modifier = resolverInitModifier();
    resolver->currentFunction = function;
    if (resolver->currentFunction->enclosing != NULL) resolver->isTopLevel = false;
}

static void endFunctionResolver(Resolver* resolver) {
    resolver->currentFunction = resolver->currentFunction->enclosing;
    if (resolver->currentFunction == NULL || resolver->currentFunction->enclosing == NULL) {
        resolver->isTopLevel = true;
    }
}

void initResolver(VM* vm, Resolver* resolver, bool debugSymtab) {
    resolver->vm = vm;
    resolver->currentNamespace = emptyString(vm);
    resolver->currentClass = NULL;
    resolver->currentFunction = NULL;
    resolver->currentSymtab = NULL;
    resolver->globalSymtab = NULL;
    resolver->rootSymtab = NULL;
    resolver->rootClass = syntheticToken("Object");
    resolver->thisVar = syntheticToken("this");
    resolver->superVar = syntheticToken("super");
    resolver->loopDepth = 0;
    resolver->switchDepth = 0;
    resolver->tryDepth = 0;
    resolver->isTopLevel = true;
    resolver->debugSymtab = debugSymtab;
    resolver->hadError = false;
}

static int nextSymbolTableIndex(Resolver* resolver) {
    return resolver->vm->numSymtabs++;
}

static ObjString* createSymbol(Resolver* resolver, Token token) {
    return copyString(resolver->vm, token.start, token.length);
}

static ObjString* createQualifiedSymbol(Resolver* resolver, Ast* ast) {
    Ast* identifiers = astGetChild(ast, 0);
    identifiers->symtab = ast->symtab;
    Ast* identifier = astGetChild(identifiers, 0);
    identifier->symtab = identifiers->symtab;
    const char* start = identifier->token.start;
    int length = identifier->token.length;
    for (int i = 1; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        identifier->symtab = identifiers->symtab;
        length += identifier->token.length + 1;
    }
    return copyString(resolver->vm, start, length);
}

static ObjString* getSymbolFullName(Resolver* resolver, Token token) {
    int length = resolver->currentNamespace->length + token.length + 1;
    char* fullName = bufferNewCString(length);
    memcpy(fullName, resolver->currentNamespace->chars, resolver->currentNamespace->length);
    fullName[resolver->currentNamespace->length] = '.';
    memcpy(fullName + resolver->currentNamespace->length + 1, token.start, token.length);
    fullName[length] = '\0';
    return takeString(resolver->vm, fullName, length);
}

static TypeInfo* getTypeForSymbol(Resolver* resolver, Token token) {
    ObjString* shortName = copyString(resolver->vm, token.start, token.length);
    TypeInfo* type = typeTableGet(resolver->vm->typetab, shortName);
    ObjString* fullName = NULL;

    if (type == NULL) {
        fullName = concatenateString(resolver->vm, resolver->currentNamespace, shortName, ".");
        type = typeTableGet(resolver->vm->typetab, fullName);

        if (type == NULL) {
            fullName = concatenateString(resolver->vm, resolver->vm->langNamespace->fullName, shortName, ".");
            type = typeTableGet(resolver->vm->typetab, fullName);
        }
    }
    return type;
}

static void setFunctionTypeModifier(Ast* ast, CallableTypeInfo* functionType) {
    functionType->modifier.isAsync = ast->modifier.isAsync;
    functionType->modifier.isClassMethod = ast->modifier.isClass;
    functionType->modifier.isInitializer = ast->modifier.isInitializer;
    functionType->modifier.isInstanceMethod = !ast->modifier.isClass;
    functionType->modifier.isLambda = ast->modifier.isLambda;
    functionType->modifier.isVariadic = ast->modifier.isVariadic;
}

static bool findSymbol(Resolver* resolver, Token token) {
    ObjString* symbol = createSymbol(resolver, token);
    SymbolItem* item = symbolTableGet(resolver->currentSymtab, symbol);
    return (item != NULL);
}

static SymbolItem* insertSymbol(Resolver* resolver, Token token, SymbolCategory category, SymbolState state, TypeInfo* type, bool isMutable) {
    ObjString* symbol = createSymbol(resolver, token);
    SymbolItem* item = newSymbolItem(token, category, state, isMutable);
    item->type = type;
    bool inserted = symbolTableSet(resolver->currentSymtab, symbol, item);

    if (inserted) return item;
    else {
        free(item);
        return NULL;
    }
}

static SymbolItem* findThis(Resolver* resolver) {
    ObjString* symbol = createSymbol(resolver, resolver->thisVar);
    SymbolItem* item = symbolTableGet(resolver->currentSymtab, symbol);

    if (item == NULL) {
        ObjString* klass = getSymbolFullName(resolver, resolver->currentClass->name);

        if (resolver->currentSymtab->scope == SYMBOL_SCOPE_METHOD) {
            item = newSymbolItem(resolver->thisVar, SYMBOL_CATEGORY_LOCAL, SYMBOL_STATE_ACCESSED, false);
        }
        else {
            item = newSymbolItem(resolver->thisVar, SYMBOL_CATEGORY_UPVALUE, SYMBOL_STATE_ACCESSED, false);
        }

        item->type = typeTableGet(resolver->vm->typetab, klass);
        symbolTableSet(resolver->currentSymtab, symbol, item);
    }

    return item;
}

static ObjString* getSymbolTypeName(Resolver* resolver, TypeCategory category) {
    switch (category) {
        case TYPE_CATEGORY_CLASS:
            return newString(resolver->vm, "clox.std.lang.Class");
        case TYPE_CATEGORY_METACLASS:
            return newString(resolver->vm, "clox.std.lang.Metaclass");
        case TYPE_CATEGORY_TRAIT:
            return newString(resolver->vm, "clox.std.lang.Trait");
        case TYPE_CATEGORY_FUNCTION:
            return newString(resolver->vm, "clox.std.lang.Function");
        case TYPE_CATEGORY_METHOD:
            return newString(resolver->vm, "clox.std.lang.Method");
        default:
            return NULL;
    }
}

static ObjString* getMetaclassSymbol(Resolver* resolver, ObjString* className) {
    ObjString* metaclassSuffix = newString(resolver->vm, "class");
    return concatenateString(resolver->vm, className, metaclassSuffix, " ");
}

static void insertMetaclassType(Resolver* resolver, ObjString* classShortName, ObjString* classFullName) {
    ObjString* metaclassShortName = getMetaclassSymbol(resolver, classShortName);
    ObjString* metaclassFullName = getMetaclassSymbol(resolver, classFullName);
    typeTableInsertBehavior(resolver->vm->typetab, TYPE_CATEGORY_CLASS, metaclassShortName, metaclassFullName, NULL);
}

static SymbolItem* insertBehaviorType(Resolver* resolver, SymbolItem* item, TypeCategory category) {
    ObjString* shortName = copyString(resolver->vm, item->token.start, item->token.length);
    ObjString* fullName = getSymbolFullName(resolver, item->token);
    BehaviorTypeInfo* behaviorType = typeTableInsertBehavior(resolver->vm->typetab, category, shortName, fullName, NULL);
    if (category == TYPE_CATEGORY_CLASS) insertMetaclassType(resolver, shortName, fullName);
    ObjString* typeName = getSymbolTypeName(resolver, category);
    item->type = typeTableGet(resolver->vm->typetab, typeName);
    return item;
}

static void bindSuperclassType(Resolver* resolver, Token currentClass, Token superclass) {
    BehaviorTypeInfo* currentClassType = AS_BEHAVIOR_TYPE(getTypeForSymbol(resolver, currentClass));
    TypeInfo* superclassType = getTypeForSymbol(resolver, superclass);
    currentClassType->superclassType = superclassType;

    BehaviorTypeInfo* currentMetaclassType = AS_BEHAVIOR_TYPE(typeTableGet(resolver->vm->typetab, getMetaclassSymbol(resolver, currentClassType->baseType.fullName)));
    TypeInfo* superMetaclassType = typeTableGet(resolver->vm->typetab, getMetaclassSymbol(resolver, superclassType->fullName));
    currentMetaclassType->superclassType = superMetaclassType;
}

static void checkUnusedVariables(Resolver* resolver, int flag) {
    for (int i = 0; i < resolver->currentSymtab->capacity; i++) {
        SymbolEntry* entry = &resolver->currentSymtab->entries[i];
        if (entry->key == NULL) continue;
        else if (entry->value->state == SYMBOL_STATE_DECLARED || entry->value->state == SYMBOL_STATE_DEFINED) {
            if (flag == 1) semanticWarning(resolver, "Variable '%s' is never used.", entry->key->chars);
            else if (flag == 2) semanticError(resolver, "Variable '%s' is never used.", entry->key->chars);
        }
    }
}

static void checkUnmodifiedVariables(Resolver* resolver, int flag) {
    for (int i = 0; i < resolver->currentSymtab->capacity; i++) {
        SymbolEntry* entry = &resolver->currentSymtab->entries[i];
        if (entry->key == NULL) continue;
        else if (entry->value->isMutable && entry->value->state != SYMBOL_STATE_MODIFIED) {
            if (flag == 1) semanticWarning(resolver, "Mutable variable '%s' is not modified.", entry->key->chars);
            else if (flag == 2) semanticError(resolver, "Mutable variable '%s' is not modified.", entry->key->chars);
        }
    }
}

static bool isTopLevel(Resolver* resolver) {
    return (resolver->currentFunction->enclosing == NULL);
}

static SymbolScope getFunctionScope(Ast* ast) {
    return (ast->kind == AST_DECL_METHOD) ? SYMBOL_SCOPE_METHOD : SYMBOL_SCOPE_FUNCTION;
}

static void beginScope(Resolver* resolver, Ast* ast, SymbolScope scope) {
    resolver->currentSymtab = newSymbolTable(nextSymbolTableIndex(resolver), resolver->currentSymtab, scope, resolver->currentSymtab->depth + 1);
    ast->symtab = resolver->currentSymtab;
    if (isFunctionScope(scope)) resolver->currentFunction->symtab = resolver->currentSymtab;
    if (isTopLevel(resolver)) resolver->globalSymtab = resolver->currentSymtab;
}

static void endScope(Resolver* resolver) {
    if (resolver->debugSymtab) symbolTableOutput(resolver->currentSymtab);
    checkUnusedVariables(resolver, resolver->vm->config.flagUnusedVariable);
    checkUnmodifiedVariables(resolver, resolver->vm->config.flagMutableVariable);
    resolver->currentSymtab = resolver->currentSymtab->parent;
    if (isTopLevel(resolver)) resolver->globalSymtab = resolver->currentSymtab;
}

static SymbolItem* declareVariable(Resolver* resolver, Ast* ast, bool isMutable) {
    SymbolCategory category = (resolver->currentSymtab == resolver->rootSymtab) ? SYMBOL_CATEGORY_GLOBAL : SYMBOL_CATEGORY_LOCAL;
    if (ast->kind == AST_DECL_METHOD) category = SYMBOL_CATEGORY_METHOD;
    SymbolItem* item = insertSymbol(resolver, ast->token, category, SYMBOL_STATE_DECLARED, NULL, isMutable);

    if (item == NULL) {
        char* name = tokenToCString(ast->token);
        semanticError(resolver, "Already a variable with name '%s' in this scope.", name);
        free(name);
    }
    return item;
}

static SymbolItem* defineVariable(Resolver* resolver, Ast* ast) {
    ObjString* symbol = copyString(resolver->vm, ast->token.start, ast->token.length);
    SymbolItem* item = symbolTableLookup(resolver->currentSymtab, symbol);
    if (item == NULL) semanticError(resolver, "Variable name '%s' does not exist in this scope.");
    else item->state = SYMBOL_STATE_DEFINED;
    return item;
}

static SymbolItem* findLocal(Resolver* resolver, Ast* ast) {
    SymbolTable* currentSymtab = resolver->currentSymtab;
    SymbolTable* functionSymtab = resolver->currentFunction->symtab;
    ObjString* symbol = copyString(resolver->vm, ast->token.start, ast->token.length);
    SymbolItem* item = NULL;

    do {
        item = symbolTableGet(currentSymtab, symbol);
        if (item != NULL || currentSymtab->id == functionSymtab->id) {
            break;
        }
        currentSymtab = currentSymtab->parent;
    } while (currentSymtab != NULL);
    return item;
}

static void assignLocal(Resolver* resolver, SymbolItem* item) {
    item->state = SYMBOL_STATE_MODIFIED;
    if (item->category != SYMBOL_CATEGORY_LOCAL) {
        SymbolTable* currentSymtab = resolver->currentFunction->enclosing->symtab;
        ObjString* symbol = copyString(resolver->vm, item->token.start, item->token.length);
        SymbolItem* local = NULL;

        do {
            item = symbolTableGet(currentSymtab, symbol);
            if (item != NULL) {
                item->state = SYMBOL_STATE_MODIFIED;
                if(item->category == SYMBOL_CATEGORY_LOCAL) return;
            }
            currentSymtab = currentSymtab->parent;
        } while (currentSymtab != NULL);
    }
}

static SymbolItem* addUpvalue(Resolver* resolver, SymbolItem* item) {
    if (item->state == SYMBOL_STATE_DEFINED) item->state = SYMBOL_STATE_ACCESSED;
    return insertSymbol(resolver, item->token, SYMBOL_CATEGORY_UPVALUE, SYMBOL_STATE_ACCESSED, item->type, item->isMutable);
}

static SymbolItem* findUpvalue(Resolver* resolver, Ast* ast) {
    if (resolver->currentFunction->enclosing == NULL) return NULL;
    SymbolTable* currentSymtab = resolver->currentFunction->enclosing->symtab;
    ObjString* symbol = copyString(resolver->vm, ast->token.start, ast->token.length);
    FunctionResolver* functionResolver = resolver->currentFunction->enclosing;
    SymbolItem* item = NULL;

    do {
        if (functionResolver->enclosing == NULL) break;
        item = symbolTableGet(currentSymtab, symbol);
        if (item != NULL && item->category != SYMBOL_CATEGORY_GLOBAL) addUpvalue(resolver, item);

        if (currentSymtab->id == functionResolver->symtab->id) {
            functionResolver = functionResolver->enclosing;
        }
        currentSymtab = currentSymtab->parent;
    } while (currentSymtab != NULL);
    return NULL;
}

static SymbolItem* findGlobal(Resolver* resolver, Ast* ast) {
    ObjString* symbol = copyString(resolver->vm, ast->token.start, ast->token.length);
    SymbolItem* item = symbolTableGet(resolver->globalSymtab, symbol);
    if (item == NULL) {
        item = symbolTableGet(resolver->rootSymtab, symbol);
        if (item == NULL) item = symbolTableGet(resolver->vm->symtab, symbol);
        if (item != NULL) {
            return insertSymbol(resolver, ast->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, item->type, item->isMutable);
        }
    }
    return item;
}

static void checkMutability(Resolver* resolver, SymbolItem* item) {
    if (!item->isMutable) {
        char* name = tokenToCString(item->token);
        switch (item->category) {
            case SYMBOL_CATEGORY_LOCAL:
                semanticError(resolver, "Cannot assign to immutable local variable '%s'.", name);
                break;
            case SYMBOL_CATEGORY_UPVALUE:
                semanticError(resolver, "Cannot assign to immutable captured upvalue '%s'.", name);
                break;
            case SYMBOL_CATEGORY_GLOBAL:
                semanticError(resolver, "Cannot assign to immutable global variables '%s'.", name);
                break;
            default:
                break;
        }
        free(name);
    }
}

static bool checkAstType(Ast* ast, const char* name) {
    return (strcmp(ast->type->fullName->chars, name) == 0);
}

static bool checkAstTypes(Ast* ast, const char* name, const char* name2) {
    return (strcmp(ast->type->fullName->chars, name) == 0 || strcmp(ast->type->fullName->chars, name2) == 0);
}

static void defineAstType(Resolver* resolver, Ast* ast, const char* name) {
    ObjString* typeName = newString(resolver->vm, name);
    ast->type = typeTableGet(resolver->vm->typetab, typeName);
}

static void deriveAstTypeFromChild(Ast* ast, int childIndex, SymbolItem* item) {
    Ast* child = astGetChild(ast, childIndex);
    ast->type = child->type;
    if (item != NULL) item->type = ast->type;
}

static void deriveAstTypeFromUnary(Resolver* resolver, Ast* ast, SymbolItem* item) {
    Ast* child = astGetChild(ast, 0);
    if (child->type == NULL) return;

    switch (ast->token.type) {
        case TOKEN_BANG:
            defineAstType(resolver, ast, "clox.std.lang.Bool");
            break;
        case TOKEN_MINUS:
            if (strcmp(child->type->fullName->chars, "clox.std.lang.Int") == 0) {
                defineAstType(resolver, ast, "clox.std.lang.Int");
            }
            else if (strcmp(child->type->fullName->chars, "clox.std.lang.Float") == 0) {
                defineAstType(resolver, ast, "clox.std.lang.Float");
            }
            break;
        default: 
            break;
    }
}

static void deriveAstTypeFromBinary(Resolver* resolver, Ast* ast, SymbolItem* item) {
    Ast* left = astGetChild(ast, 0);
    Ast* right = astGetChild(ast, 1);
    if (left->type == NULL || right->type == NULL) return;

    switch (ast->token.type) {
        case TOKEN_BANG_EQUAL:
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_GREATER_EQUAL:
        case TOKEN_LESS:
        case TOKEN_LESS_EQUAL:
            defineAstType(resolver, ast, "Clox.std.lang.Bool");
            break;
        case TOKEN_PLUS:
            if (checkAstType(left, "clox.std.lang.String") && checkAstType(right, "clox.std.lang.String")) {
                defineAstType(resolver, ast, "clox.std.lang.String");
            }
            else if (checkAstType(left, "clox.std.lang.Int") && checkAstType(right, "clox.std.lang.Int")) {
                defineAstType(resolver, ast, "clox.std.lang.Int");
            }
            else if (checkAstTypes(left, "clox.std.lang.Int", "clox.std.lang.Float") || checkAstTypes(right, "clox.std.lang.Int", "clox.std.lang.Float")) {
                defineAstType(resolver, ast, "clox.std.lang.Float");
            }
            break;
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_MODULO:
            if (checkAstType(left, "clox.std.lang.Int") && checkAstType(right, "clox.std.lang.Int")) {
                defineAstType(resolver, ast, "clox.std.lang.Int");
            }
            else if (checkAstTypes(left, "clox.std.lang.Int", "clox.std.lang.Float") || checkAstTypes(right, "clox.std.lang.Int", "clox.std.lang.Float")) {
                defineAstType(resolver, ast, "clox.std.lang.Float");
            }
            break;
        case TOKEN_SLASH:
            if (checkAstTypes(left, "clox.std.lang.Int", "clox.std.lang.Float") || checkAstTypes(right, "clox.std.lang.Int", "clox.std.lang.Float")) {
                defineAstType(resolver, ast, "clox.std.lang.Float");
            }
            break;
        case TOKEN_DOT_DOT:
            defineAstType(resolver, ast, "Clox.std.collection.Range");
            break;
        default: 
            break;
    }
}

static void deriveAstTypeForParam(Resolver* resolver, Ast* ast) {
    Ast* child = astGetChild(ast, 0);
    ast->type = getTypeForSymbol(resolver, child->token);
    switch (resolver->currentFunction->symtab->scope) {
        case SYMBOL_SCOPE_FUNCTION: {
            ObjString* functionName = createSymbol(resolver, resolver->currentFunction->name);
            CallableTypeInfo* functionType = AS_CALLABLE_TYPE(typeTableGet(resolver->vm->typetab, functionName));
            if (functionType != NULL && functionType->paramTypes != NULL) {
                TypeInfoArrayAdd(functionType->paramTypes, ast->type);
            }
            break;
        }
        case SYMBOL_SCOPE_METHOD: {
            if (!resolver->currentClass->isAnonymous) {
                BehaviorTypeInfo* behaviorType = AS_BEHAVIOR_TYPE(getTypeForSymbol(resolver, resolver->currentClass->name));
                ObjString* methodName = createSymbol(resolver, resolver->currentFunction->name);
                CallableTypeInfo* methodType = AS_CALLABLE_TYPE(typeTableGet(behaviorType->methods, methodName));
                if (methodType != NULL && methodType->paramTypes != NULL) {
                    TypeInfoArrayAdd(methodType->paramTypes, ast->type);
                }
            }
            break;
        }
        default: 
            break;
    }
}

static SymbolItem* getVariable(Resolver* resolver, Ast* ast) {
    ObjString* symbol = createSymbol(resolver, ast->token);
    SymbolItem* item = findLocal(resolver, ast);
    if (item != NULL) {
        if (item->state == SYMBOL_STATE_DEFINED) item->state = SYMBOL_STATE_ACCESSED;
        return item;
    }

    item = findUpvalue(resolver, ast);
    if (item != NULL) return item;
    return findGlobal(resolver, ast);
}

static void parameters(Resolver* resolver, Ast* ast) {
    if (!astHasChild(ast)) return;
    for (int i = 0; i < ast->children->count; i++) {
        resolveChild(resolver, ast, i);
    }
}

static void block(Resolver* resolver, Ast* ast) {
    Ast* stmts = astGetChild(ast, 0);
    stmts->symtab = ast->symtab;
    for (int i = 0; i < stmts->children->count; i++) {
        resolveChild(resolver, stmts, i);
    }
}

static void function(Resolver* resolver, Ast* ast, bool isLambda, bool isAsync) {
    FunctionResolver functionResolver;
    initFunctionResolver(resolver, &functionResolver, ast->token, resolver->currentFunction->scopeDepth + 1);
    functionResolver.modifier.isAsync = isAsync;
    functionResolver.modifier.isClassMethod = ast->modifier.isClass;
    functionResolver.modifier.isInitializer = ast->modifier.isInitializer;
    functionResolver.modifier.isInstanceMethod = !ast->modifier.isClass;
    functionResolver.modifier.isLambda = isLambda;
    SymbolScope scope = getFunctionScope(ast);

    beginScope(resolver, ast, scope);
    Ast* params = astGetChild(ast, 0);
    params->symtab = ast->symtab;
    parameters(resolver, params);
    Ast* blk = astGetChild(ast, 1);
    blk->symtab = ast->symtab;
    block(resolver, blk);
    endScope(resolver);
    endFunctionResolver(resolver);
}

static void behavior(Resolver* resolver, BehaviorType type, Ast* ast) {
    Token name = ast->token;
    ClassResolver classResolver;
    initClassResolver(resolver, &classResolver, name, resolver->currentFunction->scopeDepth + 1, type);
    int childIndex = 0;

    if (type == BEHAVIOR_CLASS) {
        Ast* superclass = astGetChild(ast, childIndex);
        superclass->symtab = ast->symtab;
        classResolver.superClass = superclass->token;
        resolveChild(resolver, ast, childIndex);
        if(!classResolver.isAnonymous) bindSuperclassType(resolver, resolver->currentClass->name, resolver->currentClass->superClass);
        childIndex++;

        if (tokensEqual(&name, &resolver->rootClass)) {
            semanticError(resolver, "Cannot redeclare root class Object.");
        }
        else if (tokensEqual(&name, &superclass->token)) {
            semanticError(resolver, "A class cannot inherit from itself.");
        }
    }

    beginScope(resolver, ast, (type == BEHAVIOR_TRAIT) ? SYMBOL_SCOPE_TRAIT : SYMBOL_SCOPE_CLASS);
    Ast* traitList = astGetChild(ast, childIndex);
    traitList->symtab = ast->symtab;
    uint8_t traitCount = astNumChild(traitList);
    if (traitCount > 0) resolveChild(resolver, ast, childIndex);

    childIndex++;
    resolveChild(resolver, ast, childIndex);
    endScope(resolver);
    endClassResolver(resolver);
}

static void yield(Resolver* resolver, Ast* ast) {
    if (resolver->isTopLevel) {
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
    if (resolver->isTopLevel) {
        resolver->currentFunction->modifier.isAsync = true;
    }
    else if (!resolver->currentFunction->modifier.isAsync) {
        semanticError(resolver, "Cannot use await unless in top level code or inside async functions/methods.");
    }
    resolveChild(resolver, ast, 0);
}

static void resolveAnd(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveArray(Resolver* resolver, Ast* ast) {
    if (astHasChild(ast)) {
        Ast* elements = astGetChild(ast, 0);
        elements->symtab = ast->symtab;
        for (int i = 0; i < elements->children->count; i++) {
            resolveChild(resolver, elements, i);
        }
    }
    defineAstType(resolver, ast, "clox.std.collection.Array");
}

static void resolveAssign(Resolver* resolver, Ast* ast) {
    SymbolItem* item = getVariable(resolver, ast);
    if (item == NULL) return;
    else {
        checkMutability(resolver, item);
        if (item->category == SYMBOL_CATEGORY_UPVALUE) assignLocal(resolver, item);
        else if (item->state == SYMBOL_STATE_DECLARED) item->state = SYMBOL_STATE_DEFINED;
        else item->state = SYMBOL_STATE_MODIFIED;
        resolveChild(resolver, ast, 0);
    }
}

static void resolveAwait(Resolver* resolver, Ast* ast) {
    await(resolver, ast);
}

static void resolveBinary(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
    deriveAstTypeFromBinary(resolver, ast, NULL);
}

static void resolveCall(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveClass(Resolver* resolver, Ast* ast) {
    behavior(resolver, BEHAVIOR_CLASS, ast);
}

static void resolveDictionary(Resolver* resolver, Ast* ast) {
    uint8_t entryCount = 0;
    Ast* keys = astGetChild(ast, 0);
    keys->symtab = ast->symtab;
    Ast* values = astGetChild(ast, 1);
    values->symtab = ast->symtab;
    while (entryCount < keys->children->count) {
        resolveChild(resolver, keys, entryCount);
        resolveChild(resolver, values, entryCount);
        entryCount++;
    }
    defineAstType(resolver, ast, "clox.std.collection.Dictionary");
}

static void resolveFunction(Resolver* resolver, Ast* ast) {
    function(resolver, ast, ast->modifier.isLambda, ast->modifier.isAsync);
}

static void resolveGrouping(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
}

static void resolveInterpolation(Resolver* resolver, Ast* ast) {
    Ast* exprs = astGetChild(ast, 0);
    exprs->symtab = ast->symtab;
    int count = 0;

    while (count < exprs->children->count) {
        bool concatenate = false;
        bool isString = false;
        Ast* expr = astGetChild(exprs, count);
        expr->symtab = exprs->symtab;

        if (expr->kind == AST_EXPR_LITERAL && expr->token.type == TOKEN_STRING) {
            resolveChild(resolver, exprs, count);
            concatenate = true;
            isString = true;
            count++;
            if (count >= exprs->children->count) break;
        }

        resolveChild(resolver, exprs, count);
        count++;
    }
}

static void resolveInvoke(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveLiteral(Resolver* resolver, Ast* ast) {
    switch (ast->token.type) {
        case TOKEN_NIL: 
            defineAstType(resolver, ast, "clox.std.lang.Nil");
            break;
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            defineAstType(resolver, ast, "clox.std.lang.Bool");
            break;
        case TOKEN_INT:
            defineAstType(resolver, ast, "clox.std.lang.Int");
            break;
        case TOKEN_NUMBER:
            defineAstType(resolver, ast, "clox.std.lang.Float");
            break;
        case TOKEN_STRING:
            defineAstType(resolver, ast, "clox.std.lang.String");
            break;
        default:
            semanticError(resolver, "Invalid AST literal type.");
    }
}

static void resolveNil(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveOr(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveParam(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, ast->modifier.isMutable);
    item->state = SYMBOL_STATE_DEFINED;
    if (astNumChild(ast) > 0) {
        deriveAstTypeForParam(resolver, ast);
        item->type = ast->type;
    }
}

static void resolvePropertyGet(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
}

static void resolvePropertySet(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveSubscriptGet(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveSubscriptSet(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
    resolveChild(resolver, ast, 2);
}

static void resolveSuperGet(Resolver* resolver, Ast* ast) {
    if (resolver->currentClass == NULL) {
        semanticError(resolver, "Cannot use 'super' outside of a class.");
    }
    findThis(resolver);
    resolveChild(resolver, ast, 0);
}

static void resolveSuperInvoke(Resolver* resolver, Ast* ast) {
    if (resolver->currentClass == NULL) {
        semanticError(resolver, "Cannot use 'super' outside of a class.");
    }
    findThis(resolver);
    resolveChild(resolver, ast, 0);
}

static void resolveThis(Resolver* resolver, Ast* ast) {
    if (resolver->currentClass == NULL) {
        semanticError(resolver, "Cannot use 'this' outside of a class.");
    }
    findThis(resolver);
}

static void resolveTrait(Resolver* resolver, Ast* ast) {
    behavior(resolver, BEHAVIOR_TRAIT, ast);
}

static void resolveUnary(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    deriveAstTypeFromUnary(resolver, ast, NULL);
}

static void resolveVariable(Resolver* resolver, Ast* ast) {
    SymbolItem* item = getVariable(resolver, ast);
    if (item != NULL) {
        ast->type = item->type;
        if (item->state == SYMBOL_STATE_DECLARED) {
            ObjString* name = createSymbol(resolver, ast->token);
            semanticError(resolver, "Cannot use variable '%s' before it is defined.", name->chars);
        }
    }
    else {
        ObjString* name = createSymbol(resolver, ast->token);
        semanticError(resolver, "undefined variable '%s'.", name->chars);
    }
}

static void resolveYield(Resolver* resolver, Ast* ast) {
    yield(resolver, ast);
}

static void resolveExpression(Resolver* resolver, Ast* ast) {
    switch (ast->kind) {
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
    block(resolver, ast);
    endScope(resolver);
}

static void resolveBreakStatement(Resolver* resolver, Ast* ast) {
    if (resolver->loopDepth == 0) {
        semanticError(resolver, "Cannot use 'break' outside of a loop.");
    }
}

static void resolveCaseStatement(Resolver* resolver, Ast* ast) {
    if (resolver->switchDepth == 0) {
        semanticError(resolver, "Cannot use 'case' outside of switch statement.");
    }
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
}

static void resolveCatchStatement(Resolver* resolver, Ast* ast) {
    beginScope(resolver, ast, SYMBOL_SCOPE_BLOCK);
    Ast* exceptionVar = astGetChild(ast, 0);
    exceptionVar->symtab = ast->symtab;
    exceptionVar->type = getTypeForSymbol(resolver, ast->token);
    SymbolItem* exceptionItem = declareVariable(resolver, exceptionVar, false);
    exceptionItem->state = SYMBOL_STATE_DEFINED;
    exceptionItem->type = exceptionVar->type;
    Ast* blk = astGetChild(ast, 1);
    blk->symtab = ast->symtab;
    block(resolver, blk);
    endScope(resolver);
}

static void resolveContinueStatement(Resolver* resolver, Ast* ast) {
    if (resolver->loopDepth == 0) {
        semanticError(resolver, "Cannot use 'break' outside of a loop.");
    }
}

static void resolveDefaultStatement(Resolver* resolver, Ast* ast) {
    if (resolver->switchDepth == 0) {
        semanticError(resolver, "Cannot use 'default' outside of switch statement.");
    }
    resolveChild(resolver, ast, 0);
}

static void resolveExpressionStatement(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
}

static void resolveFinallyStatement(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
}

static void resolveForStatement(Resolver* resolver, Ast* ast) {
    resolver->loopDepth++;
    beginScope(resolver, ast, SYMBOL_SCOPE_BLOCK);
    Ast* decl = astGetChild(ast, 0);
    decl->symtab = ast->symtab;

    for (int i = 0; i < decl->children->count; i++) {
        Ast* varDecl = astGetChild(decl, i);
        varDecl->symtab = decl->symtab;
        SymbolItem* item = declareVariable(resolver, varDecl, varDecl->modifier.isMutable);
        item->state = SYMBOL_STATE_DEFINED;
    }

    resolveChild(resolver, ast, 1);
    resolveChild(resolver, ast, 2);
    endScope(resolver);
    resolver->loopDepth--;
}

static void resolveIfStatement(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
    if (astNumChild(ast) > 2) resolveChild(resolver, ast, 2);
}

static void resolveRequireStatement(Resolver* resolver, Ast* ast) {
    if (resolver->isTopLevel) {
        semanticError(resolver, "Can only require source files from top-level code.");
    }
    resolveChild(resolver, ast, 0);
}

static void resolveReturnStatement(Resolver* resolver, Ast* ast) {
    if (resolver->isTopLevel) {
        semanticError(resolver, "Can't return from top-level code.");
    }
    else if (resolver->currentFunction->modifier.isInitializer) {
        semanticError(resolver, "Cannot return value from an initializer.");
    }
    else if (astHasChild(ast)) {
        resolveChild(resolver, ast, 0);
    }
}

static void resolveSwitchStatement(Resolver* resolver, Ast* ast) {
    resolver->switchDepth++;
    resolveChild(resolver, ast, 0);
    Ast* caseList = astGetChild(ast, 1);
    caseList->symtab = ast->symtab;

    for (int i = 0; i < caseList->children->count; i++) {
        resolveChild(resolver, caseList, i);
    }

    if (astNumChild(caseList) > 2) {
        resolveChild(resolver, ast, 2);
    }
    resolver->switchDepth--;
}

static void resolveThrowStatement(Resolver* resolver, Ast* ast) {
    resolveChild(resolver, ast, 0);
}

static void resolveTryStatement(Resolver* resolver, Ast* ast) {
    resolver->tryDepth++;
    resolveChild(resolver, ast, 0);
    resolveChild(resolver, ast, 1);
    if (astNumChild(ast) > 2) resolveChild(resolver, ast, 2);
    resolver->tryDepth--;
}

static void resolveUsingStatement(Resolver* resolver, Ast* ast) {
    Ast* _namespace = astGetChild(ast, 0);
    _namespace->symtab = ast->symtab;
    int namespaceDepth = astNumChild(_namespace);
    uint8_t index = 0;

    for (int i = 0; i < namespaceDepth; i++) {
        Ast* subNamespace = astGetChild(_namespace, i);
        subNamespace->symtab = _namespace->symtab;
        insertSymbol(resolver, subNamespace->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, NULL, false);
    }

    if (astNumChild(ast) > 1) {
        Ast* alias = astGetChild(ast, 1);
        alias->symtab = _namespace->symtab;
        insertSymbol(resolver, alias->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, NULL, false);
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
    switch (ast->kind) {
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
    insertBehaviorType(resolver, item, TYPE_CATEGORY_CLASS);
    resolveChild(resolver, ast, 0);
    item->state = SYMBOL_STATE_ACCESSED;
}

static void resolveFunDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, false);
    ObjString* name = copyString(resolver->vm, item->token.start, item->token.length);
    defineAstType(resolver, ast, "clox.std.lang.Function");
    item->type = ast->type;

    CallableTypeInfo* functionType = typeTableInsertCallable(resolver->vm->typetab, TYPE_CATEGORY_FUNCTION, name, NULL);
    if (astNumChild(ast) > 1) {
        Ast* returnType = astGetChild(ast, 1);
        functionType->returnType = getTypeForSymbol(resolver, returnType->token);
    }
    resolveChild(resolver, ast, 0);
    item->state = SYMBOL_STATE_ACCESSED;
}

static void resolveMethodDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, false);
    ObjString* name = copyString(resolver->vm, item->token.start, item->token.length);
    defineAstType(resolver, ast, "clox.std.lang.Method");
    item->type = ast->type;

    if (!resolver->currentClass->isAnonymous) {
        BehaviorTypeInfo* klass = AS_BEHAVIOR_TYPE(getTypeForSymbol(resolver, resolver->currentClass->name));
        if (ast->modifier.isClass) {
            klass = AS_BEHAVIOR_TYPE(typeTableGet(resolver->vm->typetab, getMetaclassSymbol(resolver, klass->baseType.fullName)));
        }

        CallableTypeInfo* methodType = typeTableInsertCallable(klass->methods, TYPE_CATEGORY_METHOD, name, NULL);
        setFunctionTypeModifier(ast, methodType);
        if (astNumChild(ast) > 2) {
            Ast* returnType = astGetChild(ast, 2);
            methodType->returnType = getTypeForSymbol(resolver, returnType->token);
        }
    }

    function(resolver, ast, false, ast->modifier.isAsync);
    item->state = SYMBOL_STATE_ACCESSED;
}

static void resolveNamespaceDeclaration(Resolver* resolver, Ast* ast) {
    Ast* identifiers = astGetChild(ast, 0);
    identifiers->symtab = ast->symtab;
    ObjString* typeName = newString(resolver->vm, "clox.std.lang.Namespace");
    TypeInfo* type = typeTableGet(resolver->vm->typetab, typeName);

    Ast* identifier = astGetChild(identifiers, 0);
    identifier->symtab = identifiers->symtab;
    insertSymbol(resolver, identifier->token, SYMBOL_CATEGORY_GLOBAL, SYMBOL_STATE_ACCESSED, type, false);
    resolver->currentNamespace = createQualifiedSymbol(resolver, ast);
}

static void resolveTraitDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, false);
    insertBehaviorType(resolver, item, TYPE_CATEGORY_TRAIT);
    resolveChild(resolver, ast, 0);
    item->state = SYMBOL_STATE_ACCESSED;
}

static void resolveVarDeclaration(Resolver* resolver, Ast* ast) {
    SymbolItem* item = declareVariable(resolver, ast, ast->modifier.isMutable);
    bool hasValue = astHasChild(ast);
    if (hasValue) {
        resolveChild(resolver, ast, 0);
        defineVariable(resolver, ast);
        deriveAstTypeFromChild(ast, 0, item);
    }
    else if (!ast->modifier.isMutable) {
        semanticError(resolver, "Immutable variable must be initialized upon declaration.");
    }
}

static void resolveDeclaration(Resolver* resolver, Ast* ast) {
    switch (ast->kind) {
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
    child->symtab = ast->symtab;
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
    FunctionResolver functionResolver;
    initFunctionResolver(resolver, &functionResolver, syntheticToken("script"), 0);
    int symtabIndex = nextSymbolTableIndex(resolver);
    resolver->currentSymtab = newSymbolTable(symtabIndex, resolver->vm->symtab, SYMBOL_SCOPE_MODULE, 0);
    resolver->currentFunction->symtab = resolver->currentSymtab;
    resolver->globalSymtab = resolver->currentSymtab;
    resolver->rootSymtab = resolver->currentSymtab;
    ast->symtab = resolver->currentSymtab;
    resolveAst(resolver, ast);
    endFunctionResolver(resolver);
    if (resolver->debugSymtab) {
        symbolTableOutput(resolver->rootSymtab);
        symbolTableOutput(resolver->vm->symtab);
    }
}