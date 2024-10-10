#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

DEFINE_BUFFER(AstArray, Ast*)

Ast* emptyAst(AstNodeType type, Token token) {
    Ast* ast = (Ast*)malloc(sizeof(Ast));
    if (ast != NULL) {
        ast->category = astNodeCategory(type);
        ast->type = type;
        ast->modifier = astInitModifier();
        ast->token = token;
        ast->parent = NULL;
        ast->sibling = NULL;
        ast->children = (AstArray*)malloc(sizeof(AstArray));
        if (ast->children != NULL) AstArrayInit(ast->children);
        ast->symtab = NULL;
    }
    return ast;
}

Ast* newAst(AstNodeType type, Token token, int numChildren, ...) {
    Ast* ast = emptyAst(type, token);
    va_list children;
    va_start(children, numChildren);
    for (int i = 0; i < numChildren; i++) {
        astAppendChild(ast, va_arg(children, Ast*));
    }
    va_end(children);
    return ast;
}

Ast* newAstWithChildren(AstNodeType type, Token token, AstArray* children) {
    Ast* ast = (Ast*)malloc(sizeof(Ast));
    if (ast != NULL) {
        ast->category = astNodeCategory(type);
        ast->type = type;
        ast->modifier = astInitModifier();
        ast->token = token;
        ast->parent = NULL;
        ast->sibling = NULL;
        if (children == NULL) {
            ast->children = (AstArray*)malloc(sizeof(AstArray));
            if (ast->children != NULL) AstArrayInit(ast->children);
        }
        else ast->children = children;
        ast->symtab = NULL;
    }
    return ast;
}

static void freeAstChildren(AstArray* children, bool freeChildren) {
    for (int i = 0; i < children->count; i++) {
        freeAst(children->elements[i], freeChildren);
    }
}

void freeAst(Ast* ast, bool freeChildren) {
    if (ast->children != NULL) {
        if (freeChildren) freeAstChildren(ast->children, freeChildren);
        AstArrayFree(ast->children);
        free(ast->children);
    }
    if (ast->symtab != NULL) freeSymbolTable(ast->symtab);
    free(ast);
}

void astAppendChild(Ast* ast, Ast* child) {
    if (ast->children == NULL) {
        fprintf(stderr, "Not enough memory to add child AST node to parent.");
        exit(1);
    }

    if (child != NULL) child->parent = ast;
    Ast* sibling = astLastChild(ast);
    if (sibling != NULL) sibling->sibling = child;
    AstArrayAdd(ast->children, child);
}

Ast* astFirstChild(Ast* ast) {
    if (!astHasChild(ast)) return NULL;
    return ast->children->elements[0];
}

Ast* astGetChild(Ast* ast, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    return ast->children->elements[index];
}

bool astHasChild(Ast* ast) {
    return (ast->children != NULL && ast->children->count > 0);
}

Ast* astLastChild(Ast* ast) {
    if (!astHasChild(ast)) return NULL;
    return ast->children->elements[ast->children->count - 1];
}

int astNumChild(Ast* ast) {
    return (ast->children != NULL) ? ast->children->count : 0;
}

static void astOutputIndent(int indentLevel) {
    printf("%*s", indentLevel * 2, "");
}

static void astOutputChild(Ast* ast, int indentLevel, int index) {
    if (ast->children == NULL || ast->children->count <= index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    Ast* expr = ast->children->elements[index];
    astOutput(expr, indentLevel);
}

static void astOutputExprAnd(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("and\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputExprArray(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("array\n");
    if (astHasChild(ast)) astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputExprAssign(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* name = tokenToCString(ast->token);
    printf("assign %s\n", name);
    astOutputChild(ast, indentLevel + 1, 0);
    free(name);
}

static void astOutputExprAwait(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("await\n");
    astOutputChild(ast, indentLevel + 1, 0);;
}

static void astOutputExprBinary(Ast* ast, int indentLevel) {    
    astOutputIndent(indentLevel);
    char* op = tokenToCString(ast->token);
    printf("binary %s\n", op);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(op);
}

static void astOutputExprCall(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isOptional ? "?" : "";
    printf("call%s\n", modifier);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputExprClass(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("class\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
}

static void astOutputExprDictionary(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("dictionary\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1); 
}

static void astOutputExprFunction(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("function\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputExprGrouping(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("grouping\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputExprInvoke(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isOptional ? "?" : "";
    char* method = tokenToCString(ast->token);
    printf("invoke %s.%s\n", modifier, method);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(method);
}

static void astOutputExprInterpolation(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("interpolation\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputExprLiteral(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* token = tokenToCString(ast->token);
    switch (ast->token.type) {
        case TOKEN_NIL:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_INT:
        case TOKEN_NUMBER: {
            printf("%s\n", token);
            break;
        }
        case TOKEN_STRING: {
            printf("\"%s\"\n", token);
            break;
        }
        default: break;
    }
    free(token);
}

static void astOutputExprNil(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* op = tokenToCString(ast->token);
    printf("nil ?%s\n", op);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(op);
}

static void astOutputExprOr(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("or\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputExprParam(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* mutable = ast->modifier.isMutable ? "var " : "";
    char* variadic = ast->modifier.isVariadic ? ".." : "";
    char* name = tokenToCString(ast->token);
    printf("param %s%s%s\n", mutable, variadic, name);
    free(name);
}

static void astOutputExprPropertyGet(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isOptional ? "?" : "";
    char* prop = tokenToCString(ast->token);
    printf("propertyGet %s.%s\n", modifier, prop);
    astOutputChild(ast, indentLevel + 1, 0);
    free(prop);
}

static void astOutputExprPropertySet(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* prop = tokenToCString(ast->token);
    printf("propertySet %s\n", prop);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(prop);
}

static void astOutputExprSubscriptGet(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isOptional ? "?" : "";
    printf("subscriptGet%s\n", modifier);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputExprSubscriptSet(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("subscriptSet\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
}

static void astOutputExprSuperGet(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* prop = tokenToCString(ast->token);
    printf("superGet %s\n", prop);
    free(prop);
}

static void astOutputExprSuperInvoke(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* method = tokenToCString(ast->token);
    printf("superInvoke %s\n", method);
    astOutputChild(ast, indentLevel + 1, 0);
    free(method);
}

static void astOutputExprThis(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("this\n");
}

static void astOutputExprTrait(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("trait\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputExprUnary(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* op = tokenToCString(ast->token);
    printf("unary %s\n", op);
    astOutputChild(ast, indentLevel + 1, 0);
    free(op);
}

static void astOutputExprVariable(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isMutable ? "var " : "";
    char* name = tokenToCString(ast->token);
    printf("%s%s\n", modifier, name);
    free(name);
}

static void astOutputExprYield(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isWith ? " with" : "";
    printf("yield%s\n", modifier);
    if(astHasChild(ast)) astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputStmtAwait(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("awaitStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputStmtBlock(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("blockStmt\n");
    Ast* stmtList = astGetChild(ast, 0);
    astOutput(stmtList, indentLevel + 1);
}

static void astOutputStmtBreak(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("breakStmt\n");
}

static void astOutputStmtCase(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("caseStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputStmtCatch(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* type = tokenToCString(ast->token);
    printf("catchStmt %s\n", type);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(type);
}

static void astOutputStmtContinue(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("continueStmt\n");
}

static void astOutputStmtDefault(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("defaultStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputStmtExpression(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("exprStmt\n");
    Ast* expr = astGetChild(ast, 0);
    astOutput(expr, indentLevel + 1);
}

static void astOutputStmtFinally(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("finallyStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputStmtFor(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("forStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
}

static void astOutputStmtIf(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("ifStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    if (ast->children->count > 2) {
        astOutputChild(ast, indentLevel + 1, 2);
    }
}

static void astOutputStmtRequire(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("requireStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputStmtReturn(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("returnStmt\n");
    if (astHasChild(ast)) {
        astOutputChild(ast, indentLevel + 1, 0);
    }
}

static void astOutputStmtSwitch(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("switchStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    if (ast->children->count > 2) {
        astOutputChild(ast, indentLevel + 1, 2);
    }
}

static void astOutputStmtThrow(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("throwStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputStmtTry(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("tryStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    if (ast->children->count > 2) {
        astOutputChild(ast, indentLevel + 1, 2);
    }
}

static void astOutputStmtUsing(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    char* name = tokenToCString(identifier->token);
    printf("usingStmt %s", name);
    free(name);

    for (int i = 1; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        name = tokenToCString(identifier->token);
        printf(".%s", name);
        free(name);
    }

    if (astNumChild(ast) > 1) {
        Ast* alias = astGetChild(ast, 1);
        name = tokenToCString(alias->token);
        printf(" as %s", name);
        free(name);
    }
    printf("\n");
}

static void astOutputStmtWhile(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("whileStmt\n");
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
}

static void astOutputStmtYield(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isWith ? " with" : "";
    printf("yieldStmt%s\n", modifier);
    if(astHasChild(ast)) astOutputChild(ast, indentLevel + 1, 0);
}

static void astOutputDeclClass(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* className = tokenToCString(ast->token);
    printf("classDecl %s\n", className);
    astOutputChild(ast, indentLevel + 1, 0);
    free(className);
}

static void astOutputDeclFun(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* funName = tokenToCString(ast->token);
    printf("funDecl %s%s\n", async, funName);
    astOutputChild(ast, indentLevel + 1, 0);
    free(funName);
}

static void astOutputDeclMethod(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* _class = ast->modifier.isClass ? "class " : "";
    char* methodName = tokenToCString(ast->token);
    printf("methodDecl %s%s%s\n", async, _class, methodName);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(methodName);
}

static void astOutputDeclNamespace(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    char* name = tokenToCString(identifier->token);
    printf("namespaceDecl %s", name);    free(name);

    for (int i = 1; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        name = tokenToCString(identifier->token);
        printf(".%s", name);
        free(name);
    }
    printf("\n");
}

static void astOutputDeclTrait(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* traitName = tokenToCString(ast->token);
    printf("traitDecl %s\n", traitName);
    astOutputChild(ast, indentLevel + 1, 0);
    free(traitName);
}

static void astOutputDeclVar(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    char* modifier = ast->modifier.isMutable ? "var" : "val";
    char* varName = tokenToCString(ast->token);
    printf("varDecl %s %s\n", modifier, varName);
    if (astHasChild(ast)) astOutputChild(ast, indentLevel + 1, 0);
    free(varName);
}

static void astOutputListExpr(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("listExpr(%d)\n", astNumChild(ast));
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(astGetChild(ast, i), indentLevel + 1);
    }
}

static void astOutputListMethod(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("listMethod(%d)\n", astNumChild(ast));
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(astGetChild(ast, i), indentLevel + 1);
    }
}

static void astOutputListStmt(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("listStmt(%d)\n", astNumChild(ast));
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(ast->children->elements[i], indentLevel + 1);
    }
}

static void astOutputListVar(Ast* ast, int indentLevel) {
    astOutputIndent(indentLevel);
    printf("listVar(%d)\n", astNumChild(ast));
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(ast->children->elements[i], indentLevel + 1);
    }
}

static void astOutputScript(Ast* ast, int indentLevel) {
    printf("script\n");
    if (astHasChild(ast)) {
        for (int i = 0; i < ast->children->count; i++) {
            astOutput(ast->children->elements[i], indentLevel + 1);
        }
    }
}

void astOutput(Ast* ast, int indentLevel) {
    switch (ast->category) {
        case AST_CATEGORY_SCRIPT: 
            astOutputScript(ast, indentLevel);
            break;
        case AST_CATEGORY_EXPR: {
            switch (ast->type) {
                case AST_EXPR_AND:
                    astOutputExprAnd(ast, indentLevel);
                    break;
                case AST_EXPR_ARRAY:
                    astOutputExprArray(ast, indentLevel);
                    break;
                case AST_EXPR_ASSIGN:
                    astOutputExprAssign(ast, indentLevel);
                    break;
                case AST_EXPR_AWAIT:
                    astOutputExprAwait(ast, indentLevel);
                    break;
                case AST_EXPR_BINARY: 
                    astOutputExprBinary(ast, indentLevel);
                    break;
                case AST_EXPR_CALL:
                    astOutputExprCall(ast, indentLevel);
                    break;
                case AST_EXPR_CLASS:
                    astOutputExprClass(ast, indentLevel);
                    break;
                case AST_EXPR_DICTIONARY:
                    astOutputExprDictionary(ast, indentLevel);
                    break;
                case AST_EXPR_FUNCTION:
                    astOutputExprFunction(ast, indentLevel);
                    break;
                case AST_EXPR_GROUPING:
                    astOutputExprGrouping(ast, indentLevel);
                    break;
                case AST_EXPR_INVOKE:
                    astOutputExprInvoke(ast, indentLevel);
                    break;
                case AST_EXPR_INTERPOLATION:
                    astOutputExprInterpolation(ast, indentLevel);
                    break;
                case AST_EXPR_LITERAL:
                    astOutputExprLiteral(ast, indentLevel);
                    break;
                case AST_EXPR_NIL:
                    astOutputExprNil(ast, indentLevel);
                    break;
                case AST_EXPR_OR:
                    astOutputExprOr(ast, indentLevel);
                    break;
                case AST_EXPR_PARAM:
                    astOutputExprParam(ast, indentLevel);
                    break;
                case AST_EXPR_PROPERTY_GET:
                    astOutputExprPropertyGet(ast, indentLevel);
                    break;
                case AST_EXPR_PROPERTY_SET:
                    astOutputExprPropertySet(ast, indentLevel);
                    break;
                case AST_EXPR_SUBSCRIPT_GET:
                    astOutputExprSubscriptGet(ast, indentLevel);
                    break;
                case AST_EXPR_SUBSCRIPT_SET:
                    astOutputExprSubscriptSet(ast, indentLevel);
                    break;
                case AST_EXPR_SUPER_GET:
                    astOutputExprSuperGet(ast, indentLevel);
                    break;
                case AST_EXPR_SUPER_INVOKE:
                    astOutputExprSuperInvoke(ast, indentLevel);
                    break;
                case AST_EXPR_THIS: 
                    astOutputExprThis(ast, indentLevel);
                    break;
                case AST_EXPR_TRAIT:
                    astOutputExprTrait(ast, indentLevel);
                    break;
                case AST_EXPR_UNARY:
                    astOutputExprUnary(ast, indentLevel);
                    break;
                case AST_EXPR_VARIABLE:
                    astOutputExprVariable(ast, indentLevel);
                    break;
                case AST_EXPR_YIELD:
                    astOutputExprYield(ast, indentLevel);
                    break;
                default: return;
            }
        }
        case AST_CATEGORY_STMT: {
            switch (ast->type) {
                case AST_STMT_AWAIT:
                    astOutputStmtAwait(ast, indentLevel);
                    break;
                case AST_STMT_BLOCK:
                    astOutputStmtBlock(ast, indentLevel);
                    break;
                case AST_STMT_BREAK:
                    astOutputStmtBreak(ast, indentLevel);
                    break;
                case AST_STMT_CASE:
                    astOutputStmtCase(ast, indentLevel);
                    break;
                case AST_STMT_CATCH:
                    astOutputStmtCatch(ast, indentLevel);
                    break;
                case AST_STMT_CONTINUE:
                    astOutputStmtContinue(ast, indentLevel);
                    break;
                case AST_STMT_DEFAULT:
                    astOutputStmtDefault(ast, indentLevel);
                    break;
                case AST_STMT_EXPRESSION:
                    astOutputStmtExpression(ast, indentLevel);
                    break;
                case AST_STMT_FOR:
                    astOutputStmtFor(ast, indentLevel);
                    break;
                case AST_STMT_IF:
                    astOutputStmtIf(ast, indentLevel);
                    break;
                case AST_STMT_REQUIRE:
                    astOutputStmtRequire(ast, indentLevel);
                    break;
                case AST_STMT_RETURN:
                    astOutputStmtReturn(ast, indentLevel);
                    break;
                case AST_STMT_SWITCH:
                    astOutputStmtSwitch(ast, indentLevel);
                    break;
                case AST_STMT_THROW:
                    astOutputStmtThrow(ast, indentLevel);
                    break;
                case AST_STMT_TRY:
                    astOutputStmtTry(ast, indentLevel);
                    break;
                case AST_STMT_USING:
                    astOutputStmtUsing(ast, indentLevel);
                    break;
                case AST_STMT_WHILE:
                    astOutputStmtWhile(ast, indentLevel);
                    break;
                case AST_STMT_YIELD:
                    astOutputStmtYield(ast, indentLevel);
                    break;
                default: return;
            }
        }
        case AST_CATEGORY_DECL: {
            switch (ast->type) {
                case AST_DECL_CLASS:
                    astOutputDeclClass(ast, indentLevel);
                    break;
                case AST_DECL_FUN:
                    astOutputDeclFun(ast, indentLevel);
                    break;
                case AST_DECL_METHOD:
                    astOutputDeclMethod(ast, indentLevel);
                    break;
                case AST_DECL_NAMESPACE:
                    astOutputDeclNamespace(ast, indentLevel);
                    break;
                case AST_DECL_TRAIT:
                    astOutputDeclTrait(ast, indentLevel);
                    break;
                case AST_DECL_VAR:
                    astOutputDeclVar(ast, indentLevel);
                    break;
                default: return;
            }
        }
        case AST_CATEGORY_OTHER: {
            switch (ast->type) {
                case AST_LIST_EXPR:
                    astOutputListExpr(ast, indentLevel);
                    break;
                case AST_LIST_METHOD:
                    astOutputListMethod(ast, indentLevel);
                    break;
                case AST_LIST_STMT:
                    astOutputListStmt(ast, indentLevel);
                    break;
                case AST_LIST_VAR:
                    astOutputListVar(ast, indentLevel);
                    break;
                default: return;
            }
        }
        default: return;
    }
}
