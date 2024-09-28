#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

Token syntheticToken(const char* text) {
    return (Token) {
        .type = TOKEN_EMPTY, 
        .start = text, 
        .length = (int)strlen(text)
    };
}

char* tokenToCString(Token token) {
    char* buffer = (char*)malloc((size_t)token.length + 1);
    if (buffer != NULL) {
        if (token.length > 0) memcpy(buffer, token.start, token.length);
        buffer[token.length] = '\0';
        return buffer;
    }
    else {
        fprintf(stderr, "Not enough memory to convert token to string.");
        exit(1);
    }
}
