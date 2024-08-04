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
    AST_TOP_SCRIPT,
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
    STMT_CONTINUE,
    STMT_EXPRESSION,
    STMT_FOR,
    STMT_IF,
    STMT_REQUIRE,
    STMT_RETURN,
    STMT_SWITCH,
    STMT_THROW,
    STMT_TRY,
    STMT_USING,
    STMT_WHILE,
    STMT_YIELD,
    DECL_CLASS,
    DECL_FUN,
    DECL_METHOD,
    DECL_NAMESPACE,
    DECL_TRAIT,
    DECL_VAR,
    AST_PARAM_LIST,
    AST_STMT_LIST
} AstNodeType;

typedef enum {
    LITERAL_UNKNOWN,
    LITERAL_NIL,
    LITERAL_BOOL,
    LITERA_INT,
    LITERAL_FLOAT,
    LITERAL_STRING
} AstLiteralType;

struct Ast {
    AstNodeCategory category;
    AstNodeType type;
    Token token;
    Ast* parent;
    AstArray* children;
    uint8_t depth;
};

#define ASTNODE_IS_ROOT(astNode) (ast->type == AST_SCRIPT)

Ast* newAst(AstNodeType type, Token token, AstArray* children);
void freeAst(Ast* node, bool shouldFreeChildren);
void astAppendChild(Ast* ast, Ast* child);
AstNodeCategory astCategory(AstNodeType type);
char* astOutputString(Ast* ast, int indentLevel);

#endif // !clox_ast_h