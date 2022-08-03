#include <stdio.h>

#include "parser.h"

void initParser(Parser* parser, VM* vm, Scanner* scanner) {
    parser->vm = vm;
    parser->scanner = scanner;
    parser->rootClass = syntheticToken("Object");
    parser->hadError = false;
    parser->panicMode = false;
}

static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) return;
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR) {

    }
    else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}

void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current = parser->next;

    for (;;) {
        parser->next = scanToken(parser->scanner);
        if (parser->next.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->next.start);
    }
}

void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    errorAtCurrent(parser, message);
}

bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

bool checkNext(Parser* parser, TokenType type) {
    return parser->next.type == type;
}

bool match(Parser* parser, TokenType type) {
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

void synchronize(Parser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON) return;
        switch (parser->current.type) {
        case TOKEN_CLASS:
        case TOKEN_FUN:
        case TOKEN_VAR:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_SWITCH:
        case TOKEN_WHILE:
        case TOKEN_RETURN:
            return;

        default:
            ;
        }
        advance(parser);
    }
}