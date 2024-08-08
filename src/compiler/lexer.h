#pragma once
#ifndef clox_lexer_h
#define clox_lexer_h

#include "../common/common.h"
#include "token.h"

typedef struct {
    const char* start;
    const char* current;
    int line;
    int interpolationDepth;
} Lexer;

void initLexer(Lexer* lexer, const char* source);
Token scanToken(Lexer* lexer);

#endif // !clox_lexer_h