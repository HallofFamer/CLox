#include <stdio.h>
#include <stdlib.h>
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

char* tokenToString(Token token) {
    if (token.length == 0) return "";
    char* buffer = (char*)malloc((size_t)token.length + 1);
    if (buffer != NULL) {
        memcpy(buffer, token.start, token.length);
        buffer[token.length] = '\0';
        return buffer;
    }
    exit(1);
}
