#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"

DEFINE_BUFFER(AstArray, Ast*)

static void freeAstChildren(AstArray* children, bool shouldFreeChildren) {
    for (int i = 0; i < children->count; i++) {
        freeAstNode(children->elements[i], shouldFreeChildren);
    }
}

Ast* newAst(AstNodeType type, Token token, AstArray* children) {
    Ast* node = (Ast*)malloc(sizeof(Ast));
    if (node != NULL) {
        node->category = astNodeCategory(type);
        node->type = type;
        node->token = token;
        node->parent = NULL;
        if (children == NULL) {
            node->children = (AstArray*)malloc(sizeof(AstArray));
            if (node->children != NULL) AstArrayInit(node->children);
        }
        else node->children = children;
    }
    return node;
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

AstNodeCategory astCategory(AstNodeType type) {
    if (type == AST_SCRIPT) return AST_CATEGORY_PROGRAM;
    else if (type >= EXPR_ASSIGN && type <= EXPR_YIELD) return AST_CATEGORY_EXPR;
    else if (type >= STMT_AWAIT && type <= STMT_YIELD) return AST_CATEGORY_STMT;
    else if (type >= DECL_CLASS && type <= DECL_VAR) return AST_CATEGORY_DECL;
    else return AST_CATEGORY_OTHER;
}
