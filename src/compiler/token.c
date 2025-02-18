#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "../common/buffer.h"

typedef struct {
    char* lexeme;
    bool canStart;
    bool canEnd;
} TokenRule;

TokenRule tokenRules[] = {
    [TOKEN_LEFT_PAREN]     = {"TOKEN_LEFT_PAREN",     true,    false},
    [TOKEN_RIGHT_PAREN]    = {"TOKEN_RIGHT_PAREN",    false,   true},
    [TOKEN_LEFT_BRACKET]   = {"TOKEN_LEFT_BRACKET",   true,    false},
    [TOKEN_RIGHT_BRACKET]  = {"TOKEN_RIGHT_BRACKET",  false,   true},
    [TOKEN_LEFT_BRACE]     = {"TOKEN_LEFT_BRACE",     true,    false},
    [TOKEN_RIGHT_BRACE]    = {"TOKEN_RIGHT_BRACE",    false,   true},
    [TOKEN_COLON]          = {"TOKEN_COLON",          false,   false},
    [TOKEN_COMMA]          = {"TOKEN_COMMA",          false,   false},
    [TOKEN_MINUS]          = {"TOKEN_MINUS",          true,    false},
    [TOKEN_MODULO]         = {"TOKEN_MODULO",         false,   false},
    [TOKEN_PIPE]           = {"TOKEN_PIPE",           false,   false},
    [TOKEN_PLUS]           = {"TOKEN_PLUS",           false,   false},
    [TOKEN_QUESTION]       = {"TOKEN_QUESTION",       false,   false},
    [TOKEN_SEMICOLON]      = {"TOKEN_SEMICOLON",      true,    true},
    [TOKEN_SLASH]          = {"TOKEN_SLASH",          false,   false},
    [TOKEN_STAR]           = {"TOKEN_STAR",           false,   false},
    [TOKEN_BANG]           = {"TOKEN_BANG",           true,    false},
    [TOKEN_BANG_EQUAL]     = {"TOKEN_BANG_EQUAL",     false,   false},
    [TOKEN_EQUAL]          = {"TOKEN_EQUAL",          false,   false},
    [TOKEN_EQUAL_EQUAL]    = {"TOKEN_EQUAL_EQUAL",    false,   false},
    [TOKEN_GREATER]        = {"TOKEN_GREATER",        false,   false},
    [TOKEN_GREATER_EQUAL]  = {"TOKEN_GREATER_EQUAL",  false,   false},
    [TOKEN_LESS]           = {"TOKEN_LESS",           false,   false},
    [TOKEN_LESS_EQUAL]     = {"TOKEN_LESS_EQUAL",     false,   false},
    [TOKEN_DOT]            = {"TOKEN_DOT",            false,   false},
    [TOKEN_DOT_DOT]        = {"TOKEN_DOT_DOT",        false,   false},
    [TOKEN_IDENTIFIER]     = {"TOKEN_IDENTIFIER",     true,    true},
    [TOKEN_STRING]         = {"TOKEN_STRING",         true,    true},
    [TOKEN_INTERPOLATION]  = {"TOKEN_INTERPOLATION",  true,    true},
    [TOKEN_NUMBER]         = {"TOKEN_NUMBER",         true,    true},
    [TOKEN_INT]            = {"TOKEN_INT",            true,    true},
    [TOKEN_AND]            = {"TOKEN_AND",            false,   false},
    [TOKEN_AS]             = {"TOKEN_AS",             false,   false},
    [TOKEN_ASYNC]          = {"TOKEN_ASYNC",          true,    false},
    [TOKEN_AWAIT]          = {"TOKEN_AWAIT",          true,    false},
    [TOKEN_BREAK]          = {"TOKEN_BREAK",          true,    true},
    [TOKEN_CASE]           = {"TOKEN_CASE",           true,    false},
    [TOKEN_CATCH]          = {"TOKEN_CATCH",          true,    false},
    [TOKEN_CLASS]          = {"TOKEN_CLASS",          true,    false},
    [TOKEN_CONTINUE]       = {"TOKEN_CONTINUE",       true,    true},
    [TOKEN_DEFAULT]        = {"TOKEN_DEFAULT",        true,    false},
    [TOKEN_ELSE]           = {"TOKEN_ELSE",           true,    false},
    [TOKEN_EXTENDS]        = {"TOKEN_EXTENDS",        false,   false},
    [TOKEN_FALSE]          = {"TOKEN_FALSE",          true,    true},
    [TOKEN_FINALLY]        = {"TOKEN_FINALLY",        true,    false},
    [TOKEN_FOR]            = {"TOKEN_FOR",            true,    false},
    [TOKEN_FUN]            = {"TOKEN_FUN",            true,    true},
    [TOKEN_IF]             = {"TOKEN_IF",             true,    false},
    [TOKEN_NAMESPACE]      = {"TOKEN_NAMESPACE",      true,    false},
    [TOKEN_NIL]            = {"TOKEN_NIL",            true,    true},
    [TOKEN_OR]             = {"TOKEN_OR",             false,   false},
    [TOKEN_REQUIRE]        = {"TOKEN_REQUIRE",        true,    false},
    [TOKEN_RETURN]         = {"TOKEN_RETURN",         true,    true},
    [TOKEN_SUPER]          = {"TOKEN_SUPER",          true,    true},
    [TOKEN_SWITCH]         = {"TOKEN_SWITCH",         true,    false},
    [TOKEN_THIS]           = {"TOKEN_THIS",           true,    true},
    [TOKEN_THROW]          = {"TOKEN_THROW",          true,    false},
    [TOKEN_TRAIT]          = {"TOKEN_TRAIT",          true,    false},
    [TOKEN_TRUE]           = {"TOKEN_TRUE",           true,    true},
    [TOKEN_TRY]            = {"TOKEN_TRY",            true,    false},
    [TOKEN_USING]          = {"TOKEN_USING",          true,    false},
    [TOKEN_VAL]            = {"TOKEN_VAL",            true,    false},
    [TOKEN_VAR]            = {"TOKEN_VAR",            true,    false},
    [TOKEN_WHILE]          = {"TOKEN_WHILE",          true,    false},
    [TOKEN_WITH]           = {"TOKEN_WITH",           false,   false},
    [TOKEN_YIELD]          = {"TOKEN_YIELD",          true,    true},
    [TOKEN_ERROR]          = {"TOKEN_ERROR",          false,   false},
    [TOKEN_EMPTY]          = {"TOKEN_EMPTY",          true,    true},
    [TOKEN_EOF]            = {"TOKEN_EOF",            false,   true},
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
    printf("Scanning Token type %s at line %d\n", tokenRules[token.type].lexeme, token.line);
}