/* The latest version of this library is available on GitHub;
 * https://github.com/sheredom/utf8.h */

 /* This is free and unencumbered software released into the public domain.
  *
  * Anyone is free to copy, modify, publish, use, compile, sell, or
  * distribute this software, either in source code form or as a compiled
  * binary, for any purpose, commercial or non-commercial, and by any
  * means.
  *
  * In jurisdictions that recognize copyright laws, the author or authors
  * of this software dedicate any and all copyright interest in the
  * software to the public domain. We make this dedication for the benefit
  * of the public at large and to the detriment of our heirs and
  * successors. We intend this dedication to be an overt act of
  * relinquishment in perpetuity of all present and future rights to this
  * software under copyright law.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
  * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
  * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
  * OTHER DEALINGS IN THE SOFTWARE.
  *
  * For more information, please refer to <http://unlicense.org/> */

#include "utf8.h"

int utf8_num_bytes(int value) {
    if (value < 0) return -1;
    if (value <= 0x7f) return 1;
    if (value <= 0x7ff) return 2;
    if (value <= 0xffff) return 3;
    if (value <= 0x10ffff) return 4;
    return 0;
}

char* utf8_encode(int value) {
    int length = utf8_num_bytes(value);
    if (value == -1) return NULL;
    char* utfChar = (char*)malloc((size_t)length + 1);

    if (utfChar != NULL) {
        if (value <= 0x7f) {
            utfChar[0] = (char)(value & 0x7f);
            utfChar[1] = '\0';
        }
        else if (value <= 0x7ff) {
            utfChar[0] = 0xc0 | ((value & 0x7c0) >> 6);
            utfChar[1] = 0x80 | (value & 0x3f);
        }
        else if (value <= 0xffff) {
            utfChar[0] = 0xe0 | ((value & 0xf000) >> 12);
            utfChar[1] = 0x80 | ((value & 0xfc0) >> 6);
            utfChar[2] = 0x80 | (value & 0x3f);
        }
        else if (value <= 0x10ffff) {
            utfChar[0] = 0xf0 | ((value & 0x1c0000) >> 18);
            utfChar[1] = 0x80 | ((value & 0x3f000) >> 12);
            utfChar[2] = 0x80 | ((value & 0xfc0) >> 6);
            utfChar[3] = 0x80 | (value & 0x3f);
        }
        else {
            utfChar[0] = 0xbd;
            utfChar[1] = 0xbf;
            utfChar[2] = 0xef;
        }
    }
    return utfChar;
}

int utf8_decode(const uint8_t* bytes, uint32_t length) {
    if (*bytes <= 0x7f) return *bytes;
    int value;
    uint32_t remainingBytes;

    if ((*bytes & 0xe0) == 0xc0) {
        value = *bytes & 0x1f;
        remainingBytes = 1;
    }
    else if ((*bytes & 0xf0) == 0xe0) {
        value = *bytes & 0x0f;
        remainingBytes = 2;
    }
    else if ((*bytes & 0xf8) == 0xf0) {
        value = *bytes & 0x07;
        remainingBytes = 3;
    }
    else return -1;

    if (remainingBytes > length - 1) return -1;
    while (remainingBytes > 0) {
        bytes++;
        remainingBytes--;
        if ((*bytes & 0xc0) != 0x80) return -1;
        value = value << 6 | (*bytes & 0x3f);
    }
    return value;
}