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

AstNodeCategory astNodeCategory(AstNodeType type) {
    if (type == AST_TYPE_NONE) return AST_CATEGORY_PROGRAM;
    else if (type >= AST_EXPR_ASSIGN && type <= AST_EXPR_YIELD) return AST_CATEGORY_EXPR;
    else if (type >= AST_STMT_AWAIT && type <= AST_STMT_YIELD) return AST_CATEGORY_STMT;
    else if (type >= AST_DECL_CLASS && type <= AST_DECL_VAR) return AST_CATEGORY_DECL;
    else return AST_CATEGORY_OTHER;
}

static char* astConcat(char* source, char* dest) {
    size_t srcLength = strlen(source);
    size_t destLendth = strlen(dest);
    char* result = (char*)malloc(srcLength + destLendth + 1);
    if (result != NULL) {
        memcpy(result, source, srcLength);
        memcpy(result + srcLength, dest, destLendth);
        result[srcLength + destLendth] = '\0';
    }
    return result;
}

static char* astIndent(int indentLevel) {
    char* buffer = "";
    for (int i = 0; i < indentLevel; i++) {
        buffer = astConcat(buffer, "  ");
    }
    return buffer;
}

static char* astOutputChildExpr(Ast* ast, int indentLevel, int index) {
    if (ast->children == NULL || ast->children->count < index) {
        fprintf(stderr, "Ast has no children or invalid child index specified.");
        exit(1);
    }
    Ast* expr = ast->children->elements[index];
    return astToString(expr, indentLevel);
}

static char* astExprArrayToString(Ast* ast, int indentLevel) {
    size_t length = 8;
    char* buffer = (char*)malloc(length + 1);

    if (buffer != NULL) {
        Ast* argList = ast->children->elements[1];
        sprintf_s(buffer, length, "(array [");
        for (int i = 0; i < argList->children->count; i++) {
            char* argOutput = astOutputChildExpr(ast, indentLevel, i);
            buffer = astConcat(buffer, argOutput);
            buffer = astConcat(buffer, " ");
        }
        buffer = astConcat(buffer, "]");
    }
    return buffer;
}

static char* astExprAssignToString(Ast* ast, int indentLevel) {
    char* left = astOutputChildExpr(ast, indentLevel, 0);
    char* right = astOutputChildExpr(ast, indentLevel, 1);
    size_t length = strlen(left) + strlen(right) + 10;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(assign %s %s)", left, right);
    }
    return buffer;
}

static char* astExprAwaitToString(Ast* ast, int indentLevel) {
    char* exprOutput = astOutputChildExpr(ast, indentLevel, 0);;
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(await %s)", exprOutput);
    }
    return buffer;
}

static char* astExprBinaryToString(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* left = astOutputChildExpr(ast, indentLevel, 0);
    char* right = astOutputChildExpr(ast, indentLevel, 1);
    size_t length = strlen(op) + strlen(left) + strlen(right) + 4;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(%s %s %s)", left, op, right);
    }
    return buffer;
}

static char* astExprCallToString(Ast* ast, int indentLevel) {
    char* exprOutput = astOutputChildExpr(ast, indentLevel, 0);
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = (char*)malloc(length + 1);

    if (buffer != NULL) {
        Ast* argList = ast->children->elements[1];
        sprintf_s(buffer, length, "(call %s (", exprOutput);
        for (int i = 0; i < argList->children->count; i++) {
            char* argOutput = astOutputChildExpr(ast, indentLevel, i);
            buffer = astConcat(buffer, argOutput);
            buffer = astConcat(buffer, " ");
        }
        buffer = astConcat(buffer, ")");
    }
    return buffer;
}

static char* astExprDictionaryToString(Ast* ast, int indentLevel) {
    size_t length = 13;
    char* buffer = (char*)malloc(length + 1);

    if (buffer != NULL) {
        Ast* argList = ast->children->elements[1];
        sprintf_s(buffer, length, "(dictionary [");
        for (int i = 0; i < argList->children->count; i += 2) {
            char* keyOutput = astOutputChildExpr(ast, indentLevel, i);
            char* valueOutput = astOutputChildExpr(ast, indentLevel, i + 1);
            buffer = astConcat(buffer, keyOutput);
            buffer = astConcat(buffer, ": ");
            buffer = astConcat(buffer, valueOutput);
            buffer = astConcat(buffer, " ");
        }
        buffer = astConcat(buffer, "]");
    }
    return buffer;
}

static char* astExprGetToString(Ast* ast, int indentLevel) {
    char* exprOutput = astOutputChildExpr(ast, indentLevel, 0);
    char* propOutput = tokenToString(ast->token);
    size_t length = strlen(exprOutput) + strlen(propOutput) + 15;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(getProperty %s.%s)", exprOutput, propOutput);
    }
    return buffer;
}

static char* astExprGroupingToString(Ast* ast, int indentLevel) {
    char* exprOutput = astOutputChildExpr(ast, indentLevel, 0);
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(group %s)", exprOutput);
    }
    return buffer;
}

static char* astExprInvokeToString(Ast* ast, int indentLevel) {
    char* receiverOutput = astOutputChildExpr(ast, indentLevel, 0);
    char* methodOutput = astOutputChildExpr(ast, indentLevel, 1);
    size_t length = strlen(receiverOutput) + strlen(methodOutput) + 10;
    char* buffer = (char*)malloc(length + 1);

    if (buffer != NULL) {
        Ast* argList = ast->children->elements[2];
        sprintf_s(buffer, length, "(invoke %s.%s(", receiverOutput, methodOutput);
        for (int i = 0; i < argList->children->count; i++) {
            char* argOutput = astOutputChildExpr(ast, indentLevel, i);
            buffer = astConcat(buffer, argOutput);
            buffer = astConcat(buffer, " ");
        }
        buffer = astConcat(buffer, ")");
    }
    return buffer;
}

static char* astExprLiteralToString(Ast* ast, int indentLevel) {
    switch (ast->token.type) {
        case TOKEN_NIL:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_INT:
        case TOKEN_NUMBER: {
            char* buffer = (char*)malloc((size_t)ast->token.length + 1);
            if (buffer != NULL) {
                snprintf(buffer, ast->token.length, "%s", ast->token.start);
                return buffer;
            }
        }
        case TOKEN_STRING: {
            char* buffer = (char*)malloc((size_t)ast->token.length + 3);
            if (buffer != NULL) {
                snprintf(buffer, ast->token.length, "\"%s\"", ast->token.start);
                return buffer;
            }
        }
    }
    return NULL;
}

static char* astExprThisToString(Ast* ast, int indentLevel) {
    return "(this)";
}

static char* astExprUnaryToString(Ast* ast, int indentLevel) {
    char* op = tokenToString(ast->token);
    char* child = astOutputChildExpr(ast, indentLevel, 0);
    size_t length = strlen(op) + strlen(child) + 3;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(%s %s)", op, child);
    }
    return buffer;
}

static char* astExprVariableToString(Ast* ast, int indentLevel) {
    const char* name = tokenToString(ast->token);
    size_t length = strlen(name) + 6;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(var %s)", name);
    }
    return buffer;
}

static char* astExprYieldToString(Ast* ast, int indentLevel) {
    char* exprOutput = astOutputChildExpr(ast, indentLevel, 0);;
    size_t length = (size_t)ast->token.length + 8;
    char* buffer = (char*)malloc(length + 1);
    if (buffer != NULL) {
        sprintf_s(buffer, length, "(yield %s)", exprOutput);
    }
    return buffer;
}

char* astToString(Ast* ast, int indentLevel) {
    switch (ast->category) {
        case AST_CATEGORY_EXPR: {
            switch (ast->type) {
                case AST_EXPR_ARRAY:
                    return astExprArrayToString(ast, indentLevel);
                case AST_EXPR_ASSIGN:
                    return astExprAssignToString(ast, indentLevel);
                case AST_EXPR_AWAIT:
                    return astExprAwaitToString(ast, indentLevel);
                case AST_EXPR_BINARY: 
                    return astExprBinaryToString(ast, indentLevel);
                case AST_EXPR_CALL:
                    return astExprCallToString(ast, indentLevel);
                case AST_EXPR_DICTIONARY:
                    return astExprDictionaryToString(ast, indentLevel);
                case AST_EXPR_GET:
                    return astExprGetToString(ast, indentLevel);
                case AST_EXPR_GROUPING:
                    return astExprGroupingToString(ast, indentLevel);
                case AST_EXPR_INVOKE:
                    return astExprInvokeToString(ast, indentLevel);
                case AST_EXPR_LITERAL:
                    return astExprLiteralToString(ast, indentLevel);
                case AST_EXPR_THIS: 
                    return astExprThisToString(ast, indentLevel);
                case AST_EXPR_UNARY:
                    return astExprUnaryToString(ast, indentLevel);
                case AST_EXPR_VARIABLE:
                    return astExprVariableToString(ast, indentLevel);
                case AST_EXPR_YIELD:
                    return astExprYieldToString(ast, indentLevel);
                default:
                    return NULL;
            }
        }
            
    }
    return NULL;
}
