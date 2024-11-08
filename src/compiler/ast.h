#pragma once
#ifndef clox_ast_h
#define clox_ast_h

#include "../common/buffer.h"
#include "../common/common.h"
#include "symbol.h"
#include "token.h"

typedef struct Ast Ast;
DECLARE_BUFFER(AstArray, Ast*)

typedef enum {
    AST_CATEGORY_SCRIPT,
    AST_CATEGORY_EXPR,
    AST_CATEGORY_STMT,
    AST_CATEGORY_DECL,
    AST_CATEGORY_OTHER
} AstNodeCategory;

typedef enum {
    AST_TYPE_NONE,
    AST_EXPR_AND,
    AST_EXPR_ARRAY,
    AST_EXPR_ASSIGN,
    AST_EXPR_AWAIT,
    AST_EXPR_BINARY,
    AST_EXPR_CALL,
    AST_EXPR_CLASS,
    AST_EXPR_DICTIONARY,
    AST_EXPR_FUNCTION,
    AST_EXPR_GROUPING,
    AST_EXPR_INVOKE,
    AST_EXPR_INTERPOLATION,
    AST_EXPR_LITERAL,
    AST_EXPR_NIL,
    AST_EXPR_OR,
    AST_EXPR_PARAM,
    AST_EXPR_PROPERTY_GET,
    AST_EXPR_PROPERTY_SET,
    AST_EXPR_SUBSCRIPT_GET,
    AST_EXPR_SUBSCRIPT_SET,
    AST_EXPR_SUPER_GET,
    AST_EXPR_SUPER_INVOKE,
    AST_EXPR_THIS,
    AST_EXPR_TRAIT,
    AST_EXPR_UNARY,
    AST_EXPR_VARIABLE,
    AST_EXPR_YIELD,
    AST_STMT_AWAIT,
    AST_STMT_BLOCK,
    AST_STMT_BREAK,
    AST_STMT_CASE,
    AST_STMT_CATCH,
    AST_STMT_CONTINUE,
    AST_STMT_DEFAULT,
    AST_STMT_EXPRESSION,
    AST_STMT_FINALLY,
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

typedef struct {
    bool isAsync;
    bool isClass;
    bool isInitializer;
    bool isLambda;
    bool isMutable;
    bool isOptional;
    bool isVariadic;
    bool isWith;
} AstModifier;

struct Ast {
    AstNodeCategory category;
    AstNodeType type;
    AstModifier modifier;
    Token token;
    Ast* parent;
    Ast* sibling;
    AstArray* children;
    SymbolTable* symtab;
};

#define AST_IS_ROOT(ast) (ast->category == AST_CATEGORY_SCRIPT)

static inline AstModifier astInitModifier() {
    AstModifier modifier = {
        .isAsync = false,
        .isClass = false,
        .isLambda = false,
        .isMutable = false,
        .isOptional = false,
        .isVariadic = false,
        .isWith = false
    };
    return modifier;
}

Ast* emptyAst(AstNodeType type, Token token);
Ast* newAst(AstNodeType type, Token token, int numChildren, ...);
void freeAst(Ast* node, bool freeChildren);
void astAppendChild(Ast* ast, Ast* child);
Ast* astFirstChild(Ast* ast);
Ast* astGetChild(Ast* ast, int index);
bool astHasChild(Ast* ast);
Ast* astLastChild(Ast* ast);
int astNumChild(Ast* ast);
void astOutput(Ast* ast, int indentLevel);

static inline AstNodeCategory astNodeCategory(AstNodeType type) {
    if (type == AST_TYPE_NONE) return AST_CATEGORY_SCRIPT;
    else if (type >= AST_EXPR_AND && type <= AST_EXPR_YIELD) return AST_CATEGORY_EXPR;
    else if (type >= AST_STMT_AWAIT && type <= AST_STMT_YIELD) return AST_CATEGORY_STMT;
    else if (type >= AST_DECL_CLASS && type <= AST_DECL_VAR) return AST_CATEGORY_DECL;
    else return AST_CATEGORY_OTHER;
}

#endif // !clox_ast_h