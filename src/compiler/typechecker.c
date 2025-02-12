#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "typechecker.h"
#include "../vm/native.h"
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
    typeChecker->debugTypetab = debugTypetab;
    typeChecker->hadError = false;
}

static ObjString* createSymbol(TypeChecker* typeChecker, Token token) {
    return copyString(typeChecker->vm, token.start, token.length);
}

static ObjString* createQualifiedSymbol(TypeChecker* typeChecker, Ast* ast) {
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
    return copyString(typeChecker->vm, start, length);
}

static ObjString* getClassFullName(TypeChecker* typeChecker, ObjString* name) {
    if (typeChecker->currentNamespace == NULL) return name;
    return concatenateString(typeChecker->vm, typeChecker->currentNamespace, name, ".");
}

static void defineAstType(TypeChecker* typeChecker, Ast* ast, const char* name, SymbolItem* item) {
    ObjString* typeName = newString(typeChecker->vm, name);
    ast->type = getNativeType(typeChecker->vm, name);
    if (item != NULL) item->type = ast->type;
}

static bool checkAstType(TypeChecker* typeChecker, Ast* ast, const char* name) {
    ObjString* typeName = newString(typeChecker->vm, name);
    TypeInfo* type = typeTableGet(typeChecker->vm->typetab, typeName);
    return isSubtypeOfType(ast->type, type);
}

static bool checkAstTypes(TypeChecker* typeChecker, Ast* ast, const char* name, const char* name2) {
    ObjString* typeName = newString(typeChecker->vm, name);
    TypeInfo* type = typeTableGet(typeChecker->vm->typetab, typeName);
    ObjString* typeName2 = newString(typeChecker->vm, name2);
    TypeInfo* type2 = typeTableGet(typeChecker->vm->typetab, typeName2);
    return isSubtypeOfType(ast->type, type) || isSubtypeOfType(ast->type, type2);
}

static void validateArguments(TypeChecker* typeChecker, const char* calleeDesc, Ast* ast, CallableTypeInfo* callableType) {
    if (!callableType->modifier.isVariadic) {
        if (callableType->paramTypes->count != ast->children->count) {
            typeError(typeChecker, "%s expects to receive a total of %d arguments but gets %d.", 
                calleeDesc, callableType->paramTypes->count, ast->children->count);
            return;
        }

        for (int i = 0; i < callableType->paramTypes->count; i++) {
            TypeInfo* paramType = callableType->paramTypes->elements[i];
            TypeInfo* argType = ast->children->elements[i]->type;
            if (!isSubtypeOfType(argType, paramType)) {
                typeError(typeChecker, "%s expects argument %d to be an instance of %s but gets %s.", 
                    calleeDesc, i + 1, paramType->shortName->chars, argType->shortName->chars);
            }
        }
    }
    else {
        TypeInfo* paramType = callableType->paramTypes->elements[0];
        for (int i = 0; i < ast->children->count; i++) {
            TypeInfo* argType = ast->children->elements[i]->type;
            if (!isSubtypeOfType(argType, paramType)) {
                typeError(typeChecker, "%s expects variadic arguments to be an instance of %s but gets %s.",
                    calleeDesc, paramType->shortName->chars, argType->shortName->chars);
            }
        }
    }
}

static void inferAstTypeFromChild(Ast* ast, int childIndex, SymbolItem* item) {
    Ast* child = astGetChild(ast, childIndex);
    ast->type = child->type;
    if (item != NULL) item->type = ast->type;
}

static void inferAstTypeFromUnary(TypeChecker* typeChecker, Ast* ast, SymbolItem* item) {
    Ast* child = astGetChild(ast, 0);
    if (child->type == NULL) return;

    switch (ast->token.type) {
        case TOKEN_BANG:
            defineAstType(typeChecker, ast, "Bool", item);
            break;
        case TOKEN_MINUS:
            if (!isSubtypeOfType(child->type, getNativeType(typeChecker->vm, "clox.std.lang.Number"))) {
                typeError(typeChecker, "Unary negate expects operand to be a Number, %s given.", child->type->shortName->chars);
            }
            else if (isSubtypeOfType(child->type, getNativeType(typeChecker->vm, "clox.std.lang.Int"))) {
                defineAstType(typeChecker, ast, "Int", item);
            }
            else {
                defineAstType(typeChecker, ast, "Number", item);
            }
            break;
        default:
            break;
    }
}

static void inferAstTypeFromBinary(TypeChecker* typeChecker, Ast* ast, SymbolItem* item) {
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
            defineAstType(typeChecker, ast, "Bool", item);
        break;
        case TOKEN_PLUS:
            if (checkAstType(typeChecker, left, "clox.std.lang.String") && checkAstType(typeChecker, right, "clox.std.lang.String")) {
                defineAstType(typeChecker, ast, "String", item);
            }
            else if (checkAstType(typeChecker, left, "clox.std.lang.Int") && checkAstType(typeChecker, right, "clox.std.lang.Int")) {
                defineAstType(typeChecker, ast, "Int", item);
            }
            else if (checkAstType(typeChecker, left, "clox.std.lang.Number") && checkAstType(typeChecker, right, "clox.std.lang.Number")) {
                defineAstType(typeChecker, ast, "Number", item);
            }
            break;
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_MODULO:
            if (checkAstType(typeChecker, left, "clox.std.lang.Int") && checkAstType(typeChecker, right, "clox.std.lang.Int")) {
                defineAstType(typeChecker, ast, "Int", item);
            }
            else if (checkAstType(typeChecker, left, "clox.std.lang.Number") && checkAstType(typeChecker, right, "clox.std.lang.Number")) {
                defineAstType(typeChecker, ast, "Number", item);
            }
            break;
        case TOKEN_SLASH:
            if (checkAstType(typeChecker, left, "clox.std.lang.Number") && checkAstType(typeChecker, right, "clox.std.lang.Number")) {
                defineAstType(typeChecker, ast, "Number", item);
            }
            break;
        case TOKEN_DOT_DOT:
            defineAstType(typeChecker, ast, "clox.std.collection.Range", item);
            break;
        default:
            break;
    }
}

static void inferAstTypeFromCall(TypeChecker* typeChecker, Ast* ast, SymbolItem* item) {
    Ast* callee = astGetChild(ast, 0);
    if (callee->type == NULL) return;
    Ast* args = astGetChild(ast, 1);
    ObjString* name = createSymbol(typeChecker, callee->token);
    char calleeDesc[UINT8_MAX];

    if (isSubtypeOfType(callee->type, getNativeType(typeChecker->vm, "Function"))) {
        CallableTypeInfo* functionType = AS_CALLABLE_TYPE(typeTableGet(typeChecker->vm->typetab, name));
        sprintf_s(calleeDesc, UINT8_MAX, "Function %s", name->chars);
        validateArguments(typeChecker, calleeDesc, args, functionType);
        ast->type = functionType->returnType;
    }
    else if (isSubtypeOfType(callee->type, getNativeType(typeChecker->vm, "Class"))) {
        ObjString* className = getClassFullName(typeChecker, name);
        TypeInfo* classType = typeTableGet(typeChecker->vm->typetab, className);
        if (classType == NULL) return;
        ObjString* initializerName = newString(typeChecker->vm, "__init__");
        TypeInfo* initializerType = typeTableMethodLookup(classType, initializerName);
        
        if (initializerType != NULL) {
            sprintf_s(calleeDesc, UINT8_MAX, "Class %s's initializer", name->chars);
            validateArguments(typeChecker, calleeDesc, args, AS_CALLABLE_TYPE(initializerType));
        }
        else if (astHasChild(args)) {
            typeError(typeChecker, "Class %s's initializer expects to receive a total of 0 arguments but gets %d.", name->chars, astNumChild(args));
        }
        ast->type = classType;
    }
}

static void block(TypeChecker* typeChecker, Ast* ast) {
    Ast* stmts = astGetChild(ast, 0);
    for (int i = 0; i < stmts->children->count; i++) {
        typeCheckChild(typeChecker, stmts, i);
    }
}

static void function(TypeChecker* typeChecker, Ast* ast, CallableTypeInfo* calleeType, bool isAsync) {
    FunctionTypeChecker functionTypeChecker;
    initFunctionTypeChecker(typeChecker, &functionTypeChecker, ast->token, calleeType, isAsync);
    functionTypeChecker.symtab = ast->symtab;
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    endFunctionTypeChecker(typeChecker);
}

static void behavior(TypeChecker* typeChecker, BehaviorType behaviorType, Ast* ast) {
    // to be implemented.
}

static void yield(TypeChecker* typeChecker, Ast* ast) {
    if (astNumChild(ast)) {
        typeCheckChild(typeChecker, ast, 0);
    }
}

static void await(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* child = astGetChild(ast, 0);
    if (!isSubtypeOfType(child->type, getNativeType(typeChecker->vm, "clox.std.util.Promise"))) {
        typeError(typeChecker, "'await' expects expression to be a Promise but gets %s.", child->type->shortName->chars);
    }
}

static void typeCheckAnd(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    defineAstType(typeChecker, ast, "Bool", NULL);
}

static void typeCheckArray(TypeChecker* typeChecker, Ast* ast) {
    if (astHasChild(ast)) {
        Ast* elements = astGetChild(ast, 0);
        for (int i = 0; i < elements->children->count; i++) {
            typeCheckChild(typeChecker, elements, i);
        }
    }
    defineAstType(typeChecker, ast, "clox.std.collection.Array", NULL);
}

static void typeCheckAssign(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckAwait(TypeChecker* typeChecker, Ast* ast) {
    await(typeChecker, ast);
    defineAstType(typeChecker, ast, "clox.std.util.Promise", NULL);
}

static void typeCheckBinary(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromBinary(typeChecker, ast, NULL);
}

static void typeCheckCall(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    inferAstTypeFromCall(typeChecker, ast, NULL);
}

static void typeCheckClass(TypeChecker* typeChecker, Ast* ast) {
    behavior(typeChecker, BEHAVIOR_CLASS, ast);
}

static void typeCheckDictionary(TypeChecker* typeChecker, Ast* ast) {
    uint8_t entryCount = 0;
    Ast* keys = astGetChild(ast, 0);
    Ast* values = astGetChild(ast, 1);
    while (entryCount < keys->children->count) {
        typeCheckChild(typeChecker, keys, entryCount);
        typeCheckChild(typeChecker, values, entryCount);
        entryCount++;
    }
    defineAstType(typeChecker, ast, "clox.std.collection.Dictionary", NULL);
}

static void typeCheckFunction(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckGrouping(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    inferAstTypeFromChild(ast, 0, NULL);
}

static void typeCheckInterpolation(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckInvoke(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckNil(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckOr(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    typeCheckChild(typeChecker, ast, 1);
    defineAstType(typeChecker, ast, "Bool", NULL);
}

static void typeCheckPropertyGet(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckPropertySet(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckSubscriptGet(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckSubscriptSet(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckSuperGet(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckSuperInvoke(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckTrait(TypeChecker* typeChecker, Ast* ast) {
    behavior(typeChecker, BEHAVIOR_TRAIT, ast);
}

static void typeCheckUnary(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    inferAstTypeFromUnary(typeChecker, ast, NULL);
}

static void typeCheckVariable(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableLookup(ast->symtab, createSymbol(typeChecker, ast->token));
    ast->type = item->type;
}

static void typeCheckYield(TypeChecker* typeChecker, Ast* ast) {
    yield(typeChecker, ast);
    defineAstType(typeChecker, ast, "clox.std.lang.Generator", NULL);
}

static void typeCheckExpression(TypeChecker* typeChecker, Ast* ast) {
    switch (ast->kind) {
        case AST_EXPR_AND:
            typeCheckAnd(typeChecker, ast);
            break;
        case AST_EXPR_ARRAY:
            typeCheckArray(typeChecker, ast);
            break;
        case AST_EXPR_ASSIGN:
            typeCheckAssign(typeChecker, ast);
            break;
        case AST_EXPR_AWAIT:
            typeCheckAwait(typeChecker, ast);
            break;
        case AST_EXPR_BINARY:
            typeCheckBinary(typeChecker, ast);
            break;
        case AST_EXPR_CALL:
            typeCheckCall(typeChecker, ast);
            break;
        case AST_EXPR_CLASS:
            typeCheckClass(typeChecker, ast);
            break;
        case AST_EXPR_DICTIONARY:
            typeCheckDictionary(typeChecker, ast);
            break;
        case AST_EXPR_FUNCTION:
            typeCheckFunction(typeChecker, ast);
            break;
        case AST_EXPR_GROUPING:
            typeCheckGrouping(typeChecker, ast);
            break;
        case AST_EXPR_INTERPOLATION:
            typeCheckInterpolation(typeChecker, ast);
            break;
        case AST_EXPR_INVOKE:
            typeCheckInvoke(typeChecker, ast);
            break;
        case AST_EXPR_NIL:
            typeCheckNil(typeChecker, ast);
            break;
        case AST_EXPR_OR:
            typeCheckOr(typeChecker, ast);
            break;
        case AST_EXPR_PROPERTY_GET:
            typeCheckPropertyGet(typeChecker, ast);
            break;
        case AST_EXPR_PROPERTY_SET:
            typeCheckPropertySet(typeChecker, ast);
            break;
        case AST_EXPR_SUBSCRIPT_GET:
            typeCheckSubscriptGet(typeChecker, ast);
            break;
        case AST_EXPR_SUBSCRIPT_SET:
            typeCheckSubscriptSet(typeChecker, ast);
            break;
        case AST_EXPR_SUPER_GET:
            typeCheckSuperGet(typeChecker, ast);
            break;
        case AST_EXPR_SUPER_INVOKE:
            typeCheckSuperInvoke(typeChecker, ast);
            break;
        case AST_EXPR_TRAIT:
            typeCheckTrait(typeChecker, ast);
            break;
        case AST_EXPR_UNARY:
            typeCheckUnary(typeChecker, ast);
            break;
        case AST_EXPR_VARIABLE:
            typeCheckVariable(typeChecker, ast);
            break;
        case AST_EXPR_YIELD:
            typeCheckYield(typeChecker, ast);
            break;
        default:
            break;
    }
}

static void typeCheckAwaitStatement(TypeChecker* typeChecker, Ast* ast) {
    await(typeChecker, ast);
    defineAstType(typeChecker, ast, "Nil", NULL);
}

static void typeCheckBlockStatement(TypeChecker* typeChecker, Ast* ast) {
    block(typeChecker, ast);
}

static void typeCheckCaseStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckCatchStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckDefaultStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckExpressionStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckFinallyStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckForStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckIfStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckRequireStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* child = astGetChild(ast, 0);
    if (!checkAstType(typeChecker, child, "clox.std.lang.String")) {
        typeError(typeChecker, "'require' expects expression to be a String but gets %s.", child->type->shortName->chars);
    }
}

static void typeCheckReturnStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckSwitchStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckThrowStatement(TypeChecker* typeChecker, Ast* ast) {
    typeCheckChild(typeChecker, ast, 0);
    Ast* child = astGetChild(ast, 0);
    if (!checkAstType(typeChecker, child, "clox.std.lang.Exception")) {
        typeError(typeChecker, "'throw' expects expression to be an Exception but gets %s.", child->type->shortName->chars);
    }
}

static void typeCheckTryStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckUsingStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckWhileStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckYieldStatement(TypeChecker* typeChecker, Ast* ast) {
    yield(typeChecker, ast);
    defineAstType(typeChecker, ast, "Nil", NULL);
}

static void typeCheckStatement(TypeChecker* typeChecker, Ast* ast) {
    switch (ast->kind) {
        case AST_STMT_AWAIT:
            typeCheckAwaitStatement(typeChecker, ast);
            break;
        case AST_STMT_BLOCK:
            typeCheckBlockStatement(typeChecker, ast);
            break;
        case AST_STMT_CASE:
            typeCheckCaseStatement(typeChecker, ast);
            break;
        case AST_STMT_CATCH:
            typeCheckCatchStatement(typeChecker, ast);
            break;
        case AST_STMT_DEFAULT:
            typeCheckDefaultStatement(typeChecker, ast);
            break;
        case AST_STMT_EXPRESSION:
            typeCheckExpressionStatement(typeChecker, ast);
            break;
        case AST_STMT_FINALLY:
            typeCheckFinallyStatement(typeChecker, ast);
            break;
        case AST_STMT_FOR:
            typeCheckForStatement(typeChecker, ast);
            break;
        case AST_STMT_IF:
            typeCheckIfStatement(typeChecker, ast);
            break;
        case AST_STMT_REQUIRE:
            typeCheckRequireStatement(typeChecker, ast);
            break;
        case AST_STMT_RETURN:
            typeCheckReturnStatement(typeChecker, ast);
            break;
        case AST_STMT_SWITCH:
            typeCheckSwitchStatement(typeChecker, ast);
            break;
        case AST_STMT_THROW:
            typeCheckThrowStatement(typeChecker, ast);
            break;
        case AST_STMT_TRY:
            typeCheckTryStatement(typeChecker, ast);
            break;
        case AST_STMT_USING:
            typeCheckUsingStatement(typeChecker, ast);
            break;
        case AST_STMT_WHILE:
            typeCheckWhileStatement(typeChecker, ast);
            break;
        case AST_STMT_YIELD:
            typeCheckYieldStatement(typeChecker, ast);
            break;
        default:
            break;
    }
}

static void typeCheckClassDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    defineAstType(typeChecker, ast, "Class", item);
    Ast* _class = astGetChild(ast, 0);
    _class->type = ast->type;
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckFunDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    defineAstType(typeChecker, ast, "Function", item);
    Ast* function = astGetChild(ast, 0);
    function->type = ast->type;
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckMethodDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    ObjString* name = copyString(typeChecker->vm, item->token.start, item->token.length);
    defineAstType(typeChecker, ast, "Method", item);

    if (!typeChecker->currentClass->isAnonymous) {
        CallableTypeInfo* methodType = AS_CALLABLE_TYPE(typeTableGet(typeChecker->currentClass->type->methods, name));
        function(typeChecker, ast, methodType, ast->modifier.isAsync);
    }
}

static void typeCheckNamespaceDeclaration(TypeChecker* typeChecker, Ast* ast) {
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, identifier->token));
    defineAstType(typeChecker, ast, "Namespace", item);
    typeChecker->currentNamespace = createQualifiedSymbol(typeChecker, ast);
}

static void typeCheckTraitDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    defineAstType(typeChecker, ast, "Trait", item);
    Ast* trait = astGetChild(ast, 0);
    trait->type = ast->type;
    typeCheckChild(typeChecker, ast, 0);
}

static void typeCheckVarDeclaration(TypeChecker* typeChecker, Ast* ast) {
    SymbolItem* item = symbolTableGet(ast->symtab, createSymbol(typeChecker, ast->token));
    if (astHasChild(ast)) {
        typeCheckChild(typeChecker, ast, 0);
        inferAstTypeFromChild(ast, 0, item);
    }
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
            break;
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