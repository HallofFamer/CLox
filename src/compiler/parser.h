#pragma once
#ifndef clox_parser_h
#define clox_parser_h

#include <setjmp.h>

#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer* lexer;
    Token previous;
    Token current;
    Token next;
    Token rootClass;
    bool debugAst;
    bool previousNewLine;
    bool currentNewLine;
    bool hadError;
    bool panicMode;
    jmp_buf jumpBuffer;
} Parser;

void initParser(Parser* parser, Lexer* lexer, bool debugAst);
Ast* parse(Parser* parser);

#endif // !clox_parser_h
