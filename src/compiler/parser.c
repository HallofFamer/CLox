#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"

void initParser(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->rootClass = syntheticToken("Object");
    parser->hadError = false;
    parser->panicMode = false;
}