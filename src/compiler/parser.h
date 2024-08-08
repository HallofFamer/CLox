#pragma once
#ifndef clox_parser_h
#define clox_parser_h

#include "../common/common.h"
#include "ast.h"
#include "lexer.h"

typedef struct {
    Lexer* lexer;
    Token previous;
    Token current;
    Token next;
    Token rootClass;
    bool hadError;
    bool panicMode;
} Parser;

void initParser(Parser* parser, Lexer* lexer);
Ast* parse(Parser* parser);

#endif // !clox_parser_h
