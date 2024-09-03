#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

DEFINE_BUFFER(AstArray, Ast*)

static void freeAstChildren(AstArray* children, bool freeChildren) {
    for (int i = 0; i < children->count; i++) {
        freeAst(children->elements[i], freeChildren);
    }
}

Ast* emptyAst(AstNodeType type, Token token) {
    Ast* ast = (Ast*)malloc(sizeof(Ast));
    if (ast != NULL) {
        ast->category = astNodeCategory(type);
        ast->type = type;
        ast->modifier = astInitModifier();
        ast->token = token;
        ast->parent = NULL;
        ast->children = (AstArray*)malloc(sizeof(AstArray));
        if (ast->children != NULL) AstArrayInit(ast->children);
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
        if (children == NULL) {
            ast->children = (AstArray*)malloc(sizeof(AstArray));
            if (ast->children != NULL) AstArrayInit(ast->children);
        }
        else ast->children = children;
    }
    return ast;
}

void freeAst(Ast* ast, bool freeChildren) {
    if (ast->children != NULL) {
        if (freeChildren) freeAstChildren(ast->children, freeChildren);
        AstArrayFree(ast->children);
        free(ast->children);
    }
    free(ast);
}

void astAppendChild(Ast* ast, Ast* child) {
    if (ast->children == NULL) {
        fprintf(stderr, "Not enough memory to add child AST node to parent.");
        exit(1);
    }

    AstArrayAdd(ast->children, child);
    if (child != NULL) child->parent = ast;
}

static Ast* astGetChild(Ast* ast, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    return ast->children->elements[index];
}

static bool astHasChild(Ast* ast) {
    return (ast->children != NULL && ast->children->count > 0);
}

static char* astIndent(int indentLevel) {
    if (indentLevel) return "";
    size_t length = indentLevel * 2;
    char* buffer = bufferNewCharArray(length);
    for (int i = 0; i < length; i++) {
        buffer[i] = ' ';
    }
    buffer[length] = '\0';
    return buffer;
}

static void astOutputChild(Ast* ast, int indentLevel, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    Ast* expr = ast->children->elements[index];
    astOutput(expr, indentLevel);
}

static void astOutputExprArray(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sarray\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputExprAssign(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sassign\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputExprAwait(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sawait\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);;
    free(indent);
}

static void astOutputExprBinary(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* op = tokenToString(ast->token);
    printf("%sbinary %s\n", indent, op);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
    free(op);
}

static void astOutputExprCall(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%scall\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputExprClass(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sclass\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
    free(indent);
}

static void astOutputExprDictionary(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sdictionary\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputExprFunction(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sfunction\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputExprGrouping(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sgrouping\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputExprInvoke(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sinvoke\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
    free(indent);
}

static void astOutputExprInterpolation(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sinterpolation\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputExprLiteral(Ast* ast, int indentLevel) {
    char* token = tokenToString(ast->token);
    switch (ast->token.type) {
        case TOKEN_NIL:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_INT:
        case TOKEN_NUMBER: {
            printf("%s", token);
            break;
        }
        case TOKEN_STRING: {
            printf("\"%s\"", token);
            break;
        }
        default: break;
    }
    free(token);
}

static void astOutputExprLogical(Ast* ast, int indentLevel) {
    astOutputExprBinary(ast, indentLevel);
}

static void astOutputExprNil(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* op = tokenToString(ast->token);
    printf("%sbinary ?%s\n", indent, op);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
    free(op);
}

static void astOutputExprPropertyGet(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* prop = tokenToString(ast->token);
    printf("%spropertyGet %s\n", indent, prop);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
    free(prop);
}

static void astOutputExprPropertySet(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* token = tokenToString(ast->token);
    printf("%spropertySet %s\n", indent, token);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
    free(token);
}

static void astOutputExprSubscriptGet(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%subscriptGet\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputExprSubscriptSet(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%subscriptSet\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
    free(indent);
}

static void astOutputExprSuperGet(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%superGet\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputExprSuperInvoke(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* method = tokenToString(ast->token);
    printf("%superInvoke %s\n", indent, method);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
    free(method);
}

static void astOutputExprThis(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sthis\n", indent);
    free(indent);
}

static void astOutputExprTrait(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%strait\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputExprUnary(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* op = tokenToString(ast->token);
    printf("%sunary %s\n", indent, op);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
    free(op);
}

static void astOutputExprVariable(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* modifier = ast->modifier.isMutable ? "var" : "val";
    char* name = tokenToString(ast->token);
    printf("%s%s %s\n", indent, modifier, name);
    free(indent);
    free(name);
}

static void astOutputExprYield(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%syield\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputStmtAwait(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sawaitStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputStmtBlock(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sblock\n", indent);
    Ast* stmtList = astGetChild(ast, 0);
    astOutput(stmtList, indentLevel + 1);
    free(indent);
}

static void astOutputStmtBreak(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sbreak\n", indent);
    free(indent);
}

static void astOutputStmtCase(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%scaseStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputStmtCatch(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* type = tokenToString(ast->token);
    printf("%scatchStmt %s\n", indent, type);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
    free(type);
}

static void astOutputStmtContinue(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%scontinue\n", indent);
    free(indent);
}

static void astOutputStmtExpression(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sexprStmt\n", indent);
    Ast* expr = astGetChild(ast, 0);
    astOutput(expr, indentLevel + 1);
    free(indent);
}

static void astOutputStmtFor(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sforStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
    free(indent);
}

static void astOutputStmtIf(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sifStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    if (ast->children->count > 2) {
        astOutputChild(ast, indentLevel + 1, 2);
    }
    free(indent);
}

static void astOutputStmtRequire(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%srequireStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputStmtReturn(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sreturnStmt\n", indent);
    if (ast->children != NULL && ast->children->count > 0) {
        astOutputChild(ast, indentLevel + 1, 1);
    }
    free(indent);
}

static void astOutputStmtSwitch(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sswitchStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    if (ast->children->count > 2) {
        astOutputChild(ast, indentLevel + 1, 2);
    }
    free(indent);
}

static void astOutputStmtThrow(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%sthrowStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputStmtTry(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%stryStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    if (ast->children->count > 2) {
        astOutputChild(ast, indentLevel + 1, 2);
    }
    free(indent);
}

static void astOutputStmtUsing(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%susingStmt ", indent);
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    char* name = tokenToString(identifier->token);
    printf("%s", name);
    free(name);

    for (int i = 0; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        name = tokenToString(identifier->token);
        printf(".%s", name);
        free(name);
    }
    printf("\n");
    free(indent);
}

static void astOutputStmtWhile(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%swhileStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
}

static void astOutputStmtYield(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%syieldStmt\n", indent);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
}

static void astOutputDeclClass(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* className = tokenToString(ast->token);
    printf("%sclassDecl %s\n", indent, className);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    astOutputChild(ast, indentLevel + 1, 2);
    free(indent);
    free(className);
}

static void astOutputDeclFun(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* funName = tokenToString(ast->token);
    printf("%sfunDecl %s%s\n", indent, async, funName);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
    free(funName);
}

static void astOutputDeclMethod(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* async = ast->modifier.isAsync ? "async " : "";
    char* _class = ast->modifier.isClass ? "class " : "";
    char* methodName = tokenToString(ast->token);
    printf("%sfunDecl %s%s%s\n", indent, async, _class, methodName);
    astOutputChild(ast, indentLevel + 1, 0);
    free(indent);
    free(methodName);
}

static void astOutputDeclNamespace(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%snamespaceDecl ", indent);
    Ast* identifiers = astGetChild(ast, 0);
    Ast* identifier = astGetChild(identifiers, 0);
    char* name = tokenToString(identifier->token);
    printf("%s", name);
    free(name);

    for (int i = 0; i < identifiers->children->count; i++) {
        identifier = astGetChild(identifiers, i);
        name = tokenToString(identifier->token);
        printf(".%s", name);
        free(name);
    }
    printf("\n");
    free(indent);
}

static void astOutputDeclTrait(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* traitName = tokenToString(ast->token);
    printf("%straitDecl %s\n", indent, traitName);
    astOutputChild(ast, indentLevel + 1, 0);
    astOutputChild(ast, indentLevel + 1, 1);
    free(indent);
    free(traitName);
}

static void astOutputDeclVar(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    char* modifier = ast->modifier.isMutable ? "var" : "val";
    char* varName = tokenToString(ast->token);
    printf("%svarDecl %s %s\n", indent, modifier, varName);

    if (ast->children != NULL && ast->children->count > 0) {
        astOutputChild(ast, indentLevel + 1, 0);
    }
    free(indent);
    free(varName);
}

static void astOutputListExpr(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%slistExpr\n", indent);
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(astGetChild(ast, i), indentLevel + 1);
    }
    free(indent);
}

static void astOutputListMethod(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%slistMethod\n", indent);
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(astGetChild(ast, i), indentLevel + 1);
    }
    free(indent);
}

static void astOutputListStmt(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%slistStmt\n", indent);
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(ast->children->elements[i], indentLevel + 1);
    }
    free(indent);
}

static void astOutputListVar(Ast* ast, int indentLevel) {
    char* indent = astIndent(indentLevel);
    printf("%slistVar\n", indent);
    for (int i = 0; i < ast->children->count; i++) {
        astOutput(ast->children->elements[i], indentLevel + 1);
    }
    free(indent);
}

static void astOutputScript(Ast* ast, int indentLevel) {
    printf("script\n");
    if (ast->children != NULL && ast->children->count > 0) {
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
                case AST_EXPR_LOGICAL:
                    astOutputExprLogical(ast, indentLevel);
                    break;
                case AST_EXPR_NIL:
                    astOutputExprNil(ast, indentLevel);
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
                    astOutputStmtUsing(ast, indentLevel);
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
