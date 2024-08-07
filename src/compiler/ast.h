#pragma once
#ifndef clox_ast_h
#define clox_ast_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "lexer.h"

typedef struct Ast Ast;
DECLARE_BUFFER(AstArray, Ast*)

typedef enum {
    AST_CATEGORY_PROGRAM,
    AST_CATEGORY_EXPR,
    AST_CATEGORY_STMT,
    AST_CATEGORY_DECL,
    AST_CATEGORY_OTHER
} AstNodeCategory;

typedef enum {
    AST_TYPE_NONE,
    AST_EXPR_ASSIGN,
    AST_EXPR_AWAIT,
    AST_EXPR_BINARY,
    AST_EXPR_CALL,
    AST_EXPR_COLLECTION,
    AST_EXPR_FUNCTION,
    AST_EXPR_GET,
    AST_EXPR_GROUPING,
    AST_EXPR_INVOKE,
    AST_EXPR_LAMBDA,
    AST_EXPR_LITERAL,
    AST_EXPR_LOGICAL,
    AST_EXPR_QUESTION,
    AST_EXPR_SET,
    AST_EXPR_SUPER,
    AST_EXPR_THIS,
    AST_EXPR_UNARY,
    AST_EXPR_VARIABLE,
    AST_EXPR_YIELD,
    AST_STMT_AWAIT,
    AST_STMT_BLOCK,
    AST_STMT_BREAK,
    AST_STMT_CONTINUE,
    AST_STMT_EXPRESSION,
    AST_STMT_FOR,
    AST_STMT_IF,
    AST_STMT_REQUIRE,
    AST_STMT_RETURN,
    AST_STMT_SWITCH,
    AST_STMT_THROW,
    AST_STMT_TRY,
    AST_STMT_USING,
    AST_STMT_WHILE,
    AST_STMT_YIELD,
    AST_DECL_CLASS,
    AST_DECL_FUN,
    AST_DECL_METHOD,
    AST_DECL_NAMESPACE,
    AST_DECL_TRAIT,
    AST_DECL_VAR,
    AST_LIST_EXPR,
    AST_LIST_METHOD,
    AST_LIST_STMT,
    AST_LIST_VAR,
    AST_TYPE_ERROR
} AstNodeType;

struct Ast {
    AstNodeCategory category;
    AstNodeType type;
    Token token;
    Ast* parent;
    AstArray* children;
    uint8_t depth;
};

#define ASTNODE_IS_ROOT(astNode) (ast->category == AST_CATEGORY_PROGRAM)

Ast* emptyAst(AstNodeType type, Token token);
Ast* newAst(AstNodeType type, Token token, int numChildren, ...);
Ast* newAstWithChildren(AstNodeType type, Token token, AstArray* children);
void freeAst(Ast* node, bool shouldFreeChildren);
void astAppendChild(Ast* ast, Ast* child);
AstNodeCategory astNodeCategory(AstNodeType type);
char* astPrint(Ast* ast, int indentLevel);

#endif // !clox_ast_h