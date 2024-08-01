#pragma once
#ifndef clox_ast_h
#define clox_ast_h

#include "../common/buffer.h"
#include "../common/common.h"

typedef struct AstNode AstNode;
DECLARE_BUFFER(AstNodeArray, AstNode*)

typedef enum {
    EXPR_ASSIGN,
    EXPR_AWAIT,
    EXPR_BINARY,
    EXPR_CALL,
    EXPR_COLLECTION,
    EXPR_FUNCTION,
    EXPR_GROUPING,
    EXPR_LAMBDA,
    EXPR_LITERAL,
    EXPR_LOGICAL,
    EXPR_QUESTION,
    EXPR_SUPER,
    EXPR_THIS,
    EXPR_UNARY,
    EXPR_VARIABLE,
    EXPR_YIELD,
    STMT_AWAIT,
    STMT_BLOCK,
    STMT_BREAK,
    STMT_CLASS,
    STMT_CONTINUE,
    STMT_EXPRESSION,
    STMT_FOR,
    STMT_FUNCTION,
    STMT_IF,
    STMT_NAMESPACE,
    STMT_REQUIRE,
    STMT_RETURN,
    STMT_TRAIT,
    STMT_USING,
    STMT_VAR,
    STMT_WHILE,
    STMT_YIELD
} AstNodeType;

typedef enum {
    LITERAL_UNKNOWN,
    LITERAL_NIL,
    LITERAL_BOOL,
    LITERA_INT,
    LITERAL_FLOAT,
    LITERAL_STRING
} AstLiteralType;

struct AstNode {
    AstNodeType type;
    Token token;
    AstNode* parent;
    AstNodeArray children;
    uint8_t depth;
};

#define AST_IS_ROOT(astNode) (node->depth == 0)

#endif // !clox_ast_h