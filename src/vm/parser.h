#pragma once
#ifndef clox_parser_h

#include "common.h"
#include "scanner.h"
#include "vm.h"

typedef struct {
    VM* vm;
    Scanner* scanner;
    Token next;
    Token current;
    Token previous;
    Token rootClass;
    bool hadError;
    bool panicMode;
} Parser;

void initParser(Parser* parser, VM* vm, Scanner* scanner);
void error(Parser* parser, const char* message);
void errorAtCurrent(Parser* parser, const char* message);
void advance(Parser* parser);
void consume(Parser* parser, TokenType type, const char* message);
bool check(Parser* parser, TokenType type);
bool checkNext(Parser* parser, TokenType type);
bool match(Parser* parser, TokenType type);
void synchronize(Parser* parser);

#endif // !clox_parser_h