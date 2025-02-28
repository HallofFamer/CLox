#pragma once
#ifndef clox_token_h
#define clox_token_h

#include "../common/common.h"

typedef enum {
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COLON, TOKEN_COMMA,
    TOKEN_MINUS, TOKEN_MODULO,
    TOKEN_PIPE, TOKEN_PLUS,
    TOKEN_QUESTION, TOKEN_SEMICOLON,
    TOKEN_SLASH, TOKEN_STAR,

    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_DOT, TOKEN_DOT_DOT,

    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_INTERPOLATION, TOKEN_NUMBER, TOKEN_INT,

    TOKEN_AND, TOKEN_AS, TOKEN_ASYNC, TOKEN_AWAIT, TOKEN_BREAK,
    TOKEN_CASE, TOKEN_CATCH, TOKEN_CLASS, TOKEN_CONTINUE, TOKEN_DEFAULT, 
    TOKEN_ELSE, TOKEN_EXTENDS, TOKEN_FALSE, TOKEN_FINALLY, TOKEN_FOR,
    TOKEN_FUN, TOKEN_IF, TOKEN_NAMESPACE, TOKEN_NIL, TOKEN_OR,
    TOKEN_REQUIRE, TOKEN_RETURN, TOKEN_SUPER, TOKEN_SWITCH, TOKEN_THIS,
    TOKEN_THROW, TOKEN_TRAIT, TOKEN_TRUE, TOKEN_TRY, TOKEN_USING,
    TOKEN_VAL, TOKEN_VAR, TOKEN_WHILE, TOKEN_WITH, TOKEN_YIELD,

    TOKEN_ERROR, TOKEN_EMPTY, TOKEN_EOF
} TokenSymbol;

typedef struct {
    TokenSymbol type;
    const char* start;
    int length;
    int line;
} Token;

Token syntheticToken(const char* text);
bool tokensEqual(Token* token, Token* token2);
bool tokenIsOperator(Token token);
char* tokenToCString(Token token);
void outputToken(Token token);

static inline Token emptyToken() {
    return syntheticToken("");
}

#endif // !clox_token_h