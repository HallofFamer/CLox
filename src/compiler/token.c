#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

Token syntheticToken(const char* text) {
    Token token = { 
        .start = text, 
        .length = (int)strlen(text) 
    };
    return token;
}

char* tokenToString(Token token) {
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
