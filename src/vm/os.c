#include <stdlib.h>
#include <string.h>

#include "os.h"

#ifdef _WIN32
    // define windows only functions.
    #pragma comment(lib,"WS2_32")
#else
    // define non-windows functions.
void strrev(char str[]) {
    int len = strlen(str);
    int start = 0;
    int end = len - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        end--;
        start++;
    }
}

char* _strrev(char* str) {
    int len = strlen(str);
    char* ret = malloc(len + 1);
    if (ret != NULL)
    {
        for (int i = 0; i < len; ++i)
        {
            ret[len - 1 - i] = str[i];
        }
        ret[len] = '\0';
    }
    return ret;
}

void _itoa_s(int value, char buffer[], size_t bufsz, int radix) {
    int i = 0;
    bool isNegative = false;

    if (value == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }

    if (value < 0 && radix == 10) {
        isNegative = true;
        value = -value;
    }

    while (value != 0) {
        int rem = value % radix;
        buffer[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value = value / radix;
    }

    if (isNegative) buffer[i++] = '-';
    buffer[i] = '\0';
    strrev(buffer);
}
#endif

void runAtStartup() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != NO_ERROR) {
        exit(60);
    }
#endif
}

void runAtExit(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}