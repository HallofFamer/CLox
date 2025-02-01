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
    typeChecker->scopeDepth = 0;
    typeChecker->debugTypetab = debugTypetab;
    typeChecker->hadError = false;
}

static ObjString* createSymbol(TypeChecker* typeChecker, Token token) {
    return copyString(typeChecker->vm, token.start, token.length);
}

static void defineAstType(TypeChecker* typeChecker, Ast* ast, const char* name, SymbolItem* item) {
    ObjString* typeName = newString(typeChecker->vm, name);
    ast->type = getNativeType(typeChecker->vm, name);
    if (item != NULL) item->type = ast->type;
}

static void inferAstTypeFromChild(Ast* ast, int childIndex, SymbolItem* item) {
    Ast* child = astGetChild(ast, childIndex);
    ast->type = child->type;
    if (item != NULL) item->type = ast->type;
}

static void function(TypeChecker* typeChecker, Ast* ast, CallableTypeInfo* methodType, bool isAsync) {
    FunctionTypeChecker functionTypeChecker;
    initFunctionTypeChecker(typeChecker, &functionTypeChecker, ast->token, methodType, isAsync);
    functionTypeChecker.symtab = ast->symtab;
    typeCheckChild(typeChecker, ast, 1);
    endFunctionTypeChecker(typeChecker);
}

static void typeCheckAnd(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckArray(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckAssign(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckAwait(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckBinary(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckCall(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckClass(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckDictionary(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckFunction(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckGrouping(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
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
    // to be implemented.
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
    // to be implemented.
}

static void typeCheckUnary(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckVariable(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckYield(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
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
    // to be implemented.
}

static void typeCheckBlockStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
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
    // to be implemented.
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
    // to be implemented.
}

static void typeCheckReturnStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckSwitchStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
}

static void typeCheckThrowStatement(TypeChecker* typeChecker, Ast* ast) {
    // to be implemented.
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
    // to be implemented. 
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
    // To be implemented.
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
    // To be implemented.
}

static void typeCheckTraitDeclaration(TypeChecker* typeChecker, Ast* ast) {
    // To be implemented.
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