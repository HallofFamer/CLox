#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "../common/buffer.h"

const char* tokenNames[] = {
    [TOKEN_LEFT_PAREN]     = "TOKEN_LEFT_PAREN",
    [TOKEN_RIGHT_PAREN]    = "TOKEN_RIGHT_PAREN",
    [TOKEN_LEFT_BRACKET]   = "TOKEN_LEFT_BRACKET",
    [TOKEN_RIGHT_BRACKET]  = "TOKEN_RIGHT_BRACKET",
    [TOKEN_LEFT_BRACE]     = "TOKEN_LEFT_BRACE",
    [TOKEN_RIGHT_BRACE]    = "TOKEN_RIGHT_BRACE",
    [TOKEN_COLON]          = "TOKEN_COLON",
    [TOKEN_COMMA]          = "TOKEN_COMMA",
    [TOKEN_MINUS]          = "TOKEN_MINUS",
    [TOKEN_MODULO]         = "TOKEN_MODULO",
    [TOKEN_PIPE]           = "TOKEN_PIPE",
    [TOKEN_PLUS]           = "TOKEN_PLUS",
    [TOKEN_QUESTION]       = "TOKEN_QUESTION",
    [TOKEN_SEMICOLON]      = "TOKEN_SEMICOLON",
    [TOKEN_SLASH]          = "TOKEN_SLASH",
    [TOKEN_STAR]           = "TOKEN_STAR", 
    [TOKEN_BANG]           = "TOKEN_BANG",
    [TOKEN_BANG_EQUAL]     = "TOKEN_BANG_EQUAL",
    [TOKEN_EQUAL]          = "TOKEN_EQUAL",
    [TOKEN_EQUAL_EQUAL]    = "TOKEN_EQUAL_EQUAL",
    [TOKEN_GREATER]        = "TOKEN_GREATER",
    [TOKEN_GREATER_EQUAL]  = "TOKEN_GREATER_EQUAL",
    [TOKEN_LESS]           = "TOKEN_LESS",
    [TOKEN_LESS_EQUAL]     = "TOKEN_LESS_EQUAL",
    [TOKEN_DOT]            = "TOKEN_DOT",
    [TOKEN_DOT_DOT]        = "TOKEN_DOT_DOT",
    [TOKEN_IDENTIFIER]     = "TOKEN_IDENTIFIER",
    [TOKEN_STRING]         = "TOKEN_STRING",
    [TOKEN_INTERPOLATION]  = "TOKEN_INTERPOLATION",
    [TOKEN_NUMBER]         = "TOKEN_NUMBER",
    [TOKEN_INT]            = "TOKEN_INT",
    [TOKEN_AND]            = "TOKEN_AND",
    [TOKEN_AS]             = "TOKEN_AS",
    [TOKEN_ASYNC]          = "TOKEN_ASYNC",
    [TOKEN_AWAIT]          = "TOKEN_AWAIT",
    [TOKEN_BREAK]          = "TOKEN_BREAK",
    [TOKEN_CASE]           = "TOKEN_CASE",
    [TOKEN_CATCH]          = "TOKEN_CATCH",
    [TOKEN_CLASS]          = "TOKEN_CLASS",
    [TOKEN_CONTINUE]       = "TOKEN_CONTINUE",
    [TOKEN_DEFAULT]        = "TOKEN_DEFAULT",
    [TOKEN_ELSE]           = "TOKEN_ELSE",
    [TOKEN_EXTENDS]        = "TOKEN_EXTENDS",
    [TOKEN_FALSE]          = "TOKEN_FALSE",
    [TOKEN_FINALLY]        = "TOKEN_FINALLY",
    [TOKEN_FOR]            = "TOKEN_FOR",
    [TOKEN_FUN]            = "TOKEN_FUN",
    [TOKEN_IF]             = "TOKEN_IF",
    [TOKEN_NAMESPACE]      = "TOKEN_NAMESPACE",
    [TOKEN_NIL]            = "TOKEN_NIL",
    [TOKEN_OR]             = "TOKEN_OR",
    [TOKEN_REQUIRE]        = "TOKEN_REQUIRE",
    [TOKEN_RETURN]         = "TOKEN_RETURN",
    [TOKEN_SUPER]          = "TOKEN_SUPER",
    [TOKEN_SWITCH]         = "TOKEN_SWITCH",
    [TOKEN_THIS]           = "TOKEN_THIS",
    [TOKEN_THROW]          = "TOKEN_THROW",
    [TOKEN_TRAIT]          = "TOKEN_TRAIT",
    [TOKEN_TRUE]           = "TOKEN_TRUE",
    [TOKEN_TRY]            = "TOKEN_TRY",
    [TOKEN_USING]          = "TOKEN_USING",
    [TOKEN_VAL]            = "TOKEN_VAL",
    [TOKEN_VAR]            = "TOKEN_VAR", 
    [TOKEN_VOID]           = "TOKEN_VOID",
    [TOKEN_WHILE]          = "TOKEN_WHILE",
    [TOKEN_WITH]           = "TOKEN_WITH",
    [TOKEN_YIELD]          = "TOKEN_YIELD",
    [TOKEN_ERROR]          = "TOKEN_ERROR", 
    [TOKEN_EMPTY]          = "TOKEN_EMPTY",
    [TOKEN_NEW_LINE]       = "TOKEN_NEW_LINE",
    [TOKEN_EOF]            = "TOKEN_EOF"
};

Token syntheticToken(const char* text) {
    return (Token) {
        .type = TOKEN_EMPTY, 
        .start = text, 
        .length = (int)strlen(text)
    };
}

bool tokensEqual(Token* token, Token* token2) {
    if (token->length != token2->length) return false;
    return memcmp(token->start, token2->start, token->length) == 0;
}

bool tokenIsLiteral(Token token) {
    switch (token.type) {
        case TOKEN_NIL:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
        case TOKEN_NUMBER:
        case TOKEN_INT:
        case TOKEN_STRING:
            return true;
        default:
            return false;
    }
}

bool tokenIsOperator(Token token) {
    switch (token.type) {
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_GREATER:
        case TOKEN_LESS:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_STAR:
        case TOKEN_SLASH:
        case TOKEN_MODULO:
        case TOKEN_DOT_DOT:
        case TOKEN_LEFT_BRACKET:
            return true;
        default:
            return false;
    }
}

char* tokenToCString(Token token) {
    char* buffer = bufferNewCString((size_t)token.length);
    if (buffer != NULL) {
        if (token.length > 0) memcpy(buffer, token.start, token.length);
        buffer[token.length] = '\0';
        return buffer;
    }
    else {
        fprintf(stderr, "Not enough memory to convert token to string.");
        return NULL;
    }
}

void outputToken(Token token) {
    printf("Scanning Token type %s at line %d\n", tokenNames[token.type], token.line);
}