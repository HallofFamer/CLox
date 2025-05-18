#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "memory.h"
#include "string.h"
#include "vm.h"

static ObjString* allocateString(VM* vm, char* chars, int length, uint32_t hash, GCGenerationType generation) {
    ObjString* string = ALLOCATE_STRING_GEN(length, vm->stringClass, generation);
    string->length = length;
    string->hash = hash;

    push(vm, OBJ_VAL(string));
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    tableSet(vm, &vm->strings, string, NIL_VAL);
    pop(vm);
    return string;
}

ObjString* createString(VM* vm, char* chars, int length, uint32_t hash, ObjClass* klass) {
    ObjString* string = ALLOCATE_STRING(length, klass);
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    string->length = length;
    string->hash = hash;
    return string;
}

ObjString* takeString(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, (size_t)length + 1, interned->obj.generation);
        return interned;
    }
    ObjString* string = allocateString(vm, chars, length, hash, GC_GENERATION_TYPE_EDEN);
    FREE_ARRAY(char, chars, (size_t)length + 1, string->obj.generation);
    return string;
}

ObjString* takeStringPerma(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, (size_t)length + 1, GC_GENERATION_TYPE_PERMANENT);
        return interned;
    }
    ObjString* string = allocateString(vm, chars, length, hash, GC_GENERATION_TYPE_PERMANENT);
    FREE_ARRAY(char, chars, (size_t)length + 1, string->obj.generation);
    return string;
}

ObjString* copyString(VM* vm, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, (size_t)length + 1, GC_GENERATION_TYPE_EDEN);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(vm, heapChars, length, hash, GC_GENERATION_TYPE_EDEN);
}

ObjString* copyStringPerma(VM* vm, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, (size_t)length + 1, GC_GENERATION_TYPE_PERMANENT);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(vm, heapChars, length, hash, GC_GENERATION_TYPE_PERMANENT);
}

ObjString* newString(VM* vm, const char* chars) {
    return copyString(vm, chars, (int)strlen(chars));
}

ObjString* newStringPerma(VM* vm, const char* chars) {
    return copyStringPerma(vm, chars, (int)strlen(chars));
}

ObjString* emptyString(VM* vm) {
    return copyStringPerma(vm, "", 0);
}

ObjString* formattedString(VM* vm, const char* format, ...) {
    char chars[UINT8_MAX];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(chars, UINT8_MAX, format, args);
    va_end(args);
    return copyString(vm, chars, length);
}

ObjString* formattedStringPerma(VM* vm, const char* format, ...) {
    char chars[UINT8_MAX];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(chars, UINT8_MAX, format, args);
    va_end(args);
    return copyStringPerma(vm, chars, length);
}

static void capitalizeFirstIndex(VM* vm, char* source, char* target, int length) {
    if (length == 1) target[0] = (char)toupper(source[0]);
    else {
        int codePoint = utf8Decode(source, length);
        char* utfChar = utf8Encode(utf8uprcodepoint(codePoint));
        if (utfChar == NULL) return;

        for (int i = 0; i < length; i++) {
            target[i] = utfChar[i];
        }
        free(utfChar);
    }
}

ObjString* capitalizeString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1, GC_GENERATION_TYPE_EDEN);
    int offset = utf8CodePointOffset(vm, string->chars, 0);
    capitalizeFirstIndex(vm, string->chars, heapChars, offset);

    for (; offset < string->length; offset++) {
        heapChars[offset] = string->chars[offset];
    }
    heapChars[string->length] = '\0';
    return takeString(vm, heapChars, (int)string->length);
}

ObjString* concatenateString(VM* vm, ObjString* string, ObjString* string2, const char* separator) {
    size_t separatorLength = strlen(separator);
    if (separatorLength > 1) {
        runtimeError(vm, "Separator must be empty or single character.");
    }

    size_t totalLength = (size_t)string->length + (size_t)string2->length + separatorLength;
    char* chars = bufferNewCString(totalLength);
    memcpy(chars, string->chars, string->length);
    if (separatorLength > 0) chars[string->length] = separator[0];
    memcpy(chars + string->length + separatorLength, string2->chars, string2->length);
    chars[totalLength] = '\0';
    return takeString(vm, chars, (int)totalLength);
}

static void decapitalizeFirstIndex(VM* vm, char* source, char* target, int length) {
    if (length == 1) target[0] = (char)tolower(source[0]);
    else {
        int codePoint = utf8Decode(source, length);
        char* utfChar = utf8Encode(utf8lwrcodepoint(codePoint));
        if (utfChar == NULL) return;

        for (int i = 0; i < length; i++) {
            target[i] = utfChar[i];
        }
        free(utfChar);
    }
}

ObjString* decapitalizeString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1, GC_GENERATION_TYPE_EDEN);
    int offset = utf8CodePointOffset(vm, string->chars, 0);
    decapitalizeFirstIndex(vm, string->chars, heapChars, offset);

    for (; offset < string->length; offset++) {
        heapChars[offset] = string->chars[offset];
    }
    heapChars[string->length] = '\0';
    return takeString(vm, heapChars, (int)string->length);
}

ObjString* replaceString(VM* vm, ObjString* original, ObjString* target, ObjString* replace) {
    if (original->length == 0 || target->length == 0 || original->length < target->length) return original;
    int startIndex = searchString(vm, original, target, 0);
    if (startIndex == -1) return original;

    int newLength = original->length - target->length + replace->length;
    push(vm, OBJ_VAL(target));
    char* heapChars = ALLOCATE(char, (size_t)newLength + 1, GC_GENERATION_TYPE_EDEN);
    pop(vm);

    int offset = 0;
    while (offset < startIndex) {
        heapChars[offset] = original->chars[offset];
        offset++;
    }

    while (offset < startIndex + replace->length) {
        heapChars[offset] = replace->chars[offset - startIndex];
        offset++;
    }

    while (offset < newLength) {
        heapChars[offset] = original->chars[offset - replace->length + target->length];
        offset++;
    }

    heapChars[newLength] = '\0';
    return takeString(vm, heapChars, (int)newLength);
}

ObjString* reverseString(VM* vm, ObjString* original) {
    char* heapChars = ALLOCATE(char, (size_t)original->length + 1, GC_GENERATION_TYPE_EDEN);
    int i = 0;
    while (i < original->length) {
        int offset = utf8CodePointOffset(vm, original->chars, i);
        for (int j = 0; j < offset; j++) {
            heapChars[original->length - i - (offset - j)] = original->chars[i + j];
        }
        i += offset;
    }
    return takeString(vm, heapChars, original->length);
}

int searchString(VM* vm, ObjString* haystack, ObjString* needle, uint32_t start) {
    if (needle->length == 0) return start;
    if (start + needle->length > (uint32_t)haystack->length || start >= (uint32_t)haystack->length) return -1;
    uint32_t shift[UINT8_MAX];
    uint32_t needleEnd = needle->length - 1;

    for (uint32_t index = 0; index < UINT8_MAX; index++) {
        shift[index] = needle->length;
    }

    for (uint32_t index = 0; index < needleEnd; index++) {
        char c = needle->chars[index];
        shift[(uint8_t)c] = needleEnd - index;
    }

    char lastChar = needle->chars[needleEnd];
    uint32_t range = haystack->length - needle->length;

    for (uint32_t index = start; index <= range; ) {
        char c = haystack->chars[index + needleEnd];
        if (lastChar == c && memcmp(haystack->chars + index, needle->chars, needleEnd) == 0) {
            return index;
        }

        index += shift[(uint8_t)c];
    }
    return -1;
}

ObjString* subString(VM* vm, ObjString* original, int fromIndex, int toIndex) {
    if (fromIndex >= original->length || toIndex > original->length || fromIndex > toIndex) {
        return copyString(vm, "", 0);
    }

    int newLength = toIndex - fromIndex + 1;
    char* heapChars = ALLOCATE(char, (size_t)newLength + 1, GC_GENERATION_TYPE_EDEN);
    for (int i = 0; i < newLength; i++) {
        heapChars[i] = original->chars[fromIndex + i];
    }

    heapChars[newLength] = '\0';
    return takeString(vm, heapChars, (int)newLength);
}

ObjString* toLowerString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1, GC_GENERATION_TYPE_EDEN);
    memcpy(heapChars, string->chars, (size_t)string->length + 1);
    utf8lwr(heapChars);
    return takeString(vm, heapChars, (int)string->length);
}

ObjString* toUpperString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1, GC_GENERATION_TYPE_EDEN);
    memcpy(heapChars, string->chars, (size_t)string->length + 1);
    utf8upr(heapChars);
    return takeString(vm, heapChars, (int)string->length);
}

ObjString* trimString(VM* vm, ObjString* string) {
    int ltLen = 0, rtLen = 0;

    for (int i = 0; i < string->length; i++) {
        char c = string->chars[i];
        if (c != ' ' && c != '\t' && c != '\n') break;
        ltLen++;
    }

    for (int i = string->length - 1; i >= 0; i--) {
        char c = string->chars[i];
        if (c != ' ' && c != '\t' && c != '\n') break;
        rtLen++;
    }

    int newLength = string->length - ltLen - rtLen + 1;
    char* heapChars = ALLOCATE(char, (size_t)newLength + 1, GC_GENERATION_TYPE_EDEN);
    for (int i = 0; i < newLength; i++) {
        heapChars[i] = string->chars[i + ltLen];
    }
    heapChars[newLength] = '\0';
    return takeString(vm, heapChars, (int)newLength);
}

int utf8NumBytes(int value) {
    if (value < 0) return -1;
    if (value <= 0x7f) return 1;
    if (value <= 0x7ff) return 2;
    if (value <= 0xffff) return 3;
    if (value <= 0x10ffff) return 4;
    return 0;
}

char* utf8Encode(int value) {
    int length = utf8NumBytes(value);
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

int utf8Decode(const uint8_t* bytes, uint32_t length) {
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

ObjString* utf8StringFromByte(VM* vm, uint8_t byte) {
    char chars[2] = { byte, '\0' };
    return copyString(vm, chars, 1);
}

ObjString* utf8StringFromCodePoint(VM* vm, int codePoint) {
    int length = utf8NumBytes(codePoint);
    if (length <= 0) return NULL;
    char* utfChars = utf8Encode(codePoint);
    return takeString(vm, utfChars, length);
}

int utf8CodePointOffset(VM* vm, const char* string, int index) {
    int offset = 0;
    do {
        offset++;
    } while ((string[index + offset] & 0xc0) == 0x80);
    return offset;
}

ObjString* utf8CodePointAtIndex(VM* vm, const char* string, int index) {
    int length = utf8CodePointOffset(vm, string, index);
    switch (length) {
        case 1: 
            return copyString(vm, (char[]) { string[index], '\0' }, 1);
        case 2: 
            return copyString(vm, (char[]) { string[index], string[index + 1], '\0' }, 2);
        case 3: 
            return copyString(vm, (char[]) { string[index], string[index + 1], string[index + 2], '\0' }, 3);
        case 4: 
            return copyString(vm, (char[]) { string[index], string[index + 1], string[index + 2], string[index + 3], '\0' }, 4);
        default: 
            return emptyString(vm);
    }
}
