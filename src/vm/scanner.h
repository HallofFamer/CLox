#pragma once
#ifndef clox_scanner_h
#define clox_scanner_h

#include "common.h"

typedef enum {
    
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COLON, TOKEN_COMMA, 
    TOKEN_MINUS, TOKEN_PIPE,
    TOKEN_PLUS, TOKEN_SEMICOLON, 
    TOKEN_SLASH, TOKEN_STAR,
    
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_DOT, TOKEN_DOT_DOT,
    
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER, TOKEN_INT,
    
    TOKEN_AND, TOKEN_AS, TOKEN_BREAK, TOKEN_CASE, TOKEN_CLASS, 
    TOKEN_DEFAULT, TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN, TOKEN_IF,
    TOKEN_NAMESPACE, TOKEN_NIL, TOKEN_OR, TOKEN_REQUIRE, TOKEN_RETURN, 
    TOKEN_SUPER, TOKEN_SWITCH, TOKEN_THIS, TOKEN_TRAIT, TOKEN_TRUE, 
    TOKEN_USING, TOKEN_VAL, TOKEN_VAR, TOKEN_WHILE, TOKEN_WITH,

    TOKEN_ERROR, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

void initScanner(Scanner* scanner, const char* source);
Token syntheticToken(const char* text);
Token scanToken(Scanner* scanner);

#endif // !clox_scanner_h