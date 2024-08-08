#include <stdio.h>
#include <string.h>

#include "token.h"

Token emptyToken() {
    return syntheticToken("");
}

Token syntheticToken(const char* text) {
    Token token = {
        .start = text,
        .length = (int)strlen(text)
    };
    return token;
}