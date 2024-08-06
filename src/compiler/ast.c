#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

DEFINE_BUFFER(AstArray, Ast*)

static void freeAstChildren(AstArray* children, bool shouldFreeChildren) {
    for (int i = 0; i < children->count; i++) {
        freeAst(children->elements[i], shouldFreeChildren);
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

Ast* newAst(AstNodeType type, Token token, AstArray* children) {
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

Ast* newAst1(AstNodeType type, Token token, Ast* child) {
    Ast* ast = newAst(type, token, NULL);
    astAppendChild(ast, child);
    return ast;
}

Ast* newAst2(AstNodeType type, Token token, Ast* left, Ast* right) {
    Ast* ast = newAst(type, token, NULL);
    astAppendChild(ast, left);
    astAppendChild(ast, right);
    return ast;
}

void freeAst(Ast* ast, bool shouldFreeChildren) {
    if (ast->children != NULL) {
        if (shouldFreeChildren) freeAstChildren(ast->children, shouldFreeChildren);
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

static char* astPrintExprLiteral(Ast* ast, int indentLevel) {
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

char* astPrint(Ast* ast, int indentLevel) {
    switch (ast->category) {
        case AST_CATEGORY_EXPR: {
            switch (ast->type) {
                case AST_EXPR_LITERAL:
                    return astPrintExprLiteral(ast, indentLevel);
            }
        }
            
    }
    return NULL;
}
