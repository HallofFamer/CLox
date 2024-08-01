#pragma once
#ifndef clox_ast_h
#define clox_ast_h

typedef enum {
    Expr_Assign,
    Expr_Await,
    Expr_Binary,
    Expr_Call,
    Expr_Collection,
    Expr_Function,
    Expr_Grouping,
    Expr_Literal,
    Expr_Logical,
    Expr_Question,
    Expr_Super,
    Expr_This,
    Expr_Unary,
    Expr_Variable,
    Expr_Yield,
    Stmt_Await,
    Stmt_Block,
    Stmt_Break,
    Stmt_Continue,
    Stmt_Expression,
    Stmt_For,
    Stmt_Function,
    Stmt_If,
    Stmt_Namespace,
    Stmt_Require,
    Stmt_Return,
    Stmt_Trait,
    Stmt_Using,
    Stmt_Var,
    Stmt_While,
    Stmt_Yield
} AstNodeType;

typedef enum {
    Literal_Unknown,
    Literal_Nil,
    Literal_Bool,
    Literal_Int,
    Literal_Float,
    Literal_String
} AstLiteralType;

typedef struct {
    AstNodeType type;
    uint8_t depth;
} AstNode;

#define AST_IS_ROOT(astNode) (node->depth == 0)

#endif // !clox_ast_h