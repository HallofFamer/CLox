#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "hash.h"
#include "memory.h"
#include "string.h"
#include "vm.h"

#define ALLOCATE_STRING(length) (ObjString*)allocateObject(vm, sizeof(ObjString) + length + 1, OBJ_STRING, vm->stringClass)

static ObjString* allocateString(VM* vm, char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_STRING(length);
    string->length = length;
    string->hash = hash;

    push(vm, OBJ_VAL(string));
    memcpy(string->chars, chars, length);
    string->chars[length] = '\0';
    tableSet(vm, &vm->strings, string, NIL_VAL);
    pop(vm);
    return string;
}

ObjString* takeString(VM* vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, (size_t)length + 1);
        return interned;
    }
    ObjString* string = allocateString(vm, chars, length, hash);
    FREE_ARRAY(char, chars, (size_t)length + 1);
    return string;
}

ObjString* copyString(VM* vm, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, (size_t)length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(vm, heapChars, length, hash);
}

ObjString* newString(VM* vm, const char* chars) {
    return copyString(vm, chars, (int)strlen(chars));
}

ObjString* emptyString(VM* vm) {
    return copyString(vm, "", 0);
}

ObjString* formattedString(VM* vm, const char* format, ...) {
    char chars[UINT8_MAX];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(chars, UINT8_MAX, format, args);
    va_end(args);
    return copyString(vm, chars, length);
}

ObjString* formattedLongString(VM* vm, const char* format, ...) {
    char chars[UINT16_MAX];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(chars, UINT16_MAX, format, args);
    va_end(args);
    return copyString(vm, chars, length);
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

ObjString* capitalizeString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1);

    heapChars[0] = (char)toupper(string->chars[0]);
    for (int offset = 1; offset < string->length; offset++) {
        heapChars[offset] = string->chars[offset];
    }
    heapChars[string->length] = '\0';
    return takeString(vm, heapChars, (int)string->length);
}

ObjString* decapitalizeString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1);

    heapChars[0] = (char)tolower(string->chars[0]);
    for (int offset = 1; offset < string->length; offset++) {
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
    char* heapChars = ALLOCATE(char, (size_t)newLength + 1);
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

    heapChars[newLength] = '\n';
    return takeString(vm, heapChars, (int)newLength);
}

ObjString* subString(VM* vm, ObjString* original, int fromIndex, int toIndex) {
    if (fromIndex >= original->length || toIndex > original->length || fromIndex > toIndex) {
        return copyString(vm, "", 0);
    }

    int newLength = toIndex - fromIndex + 1;
    char* heapChars = ALLOCATE(char, (size_t)newLength + 1);
    for (int i = 0; i < newLength; i++) {
        heapChars[i] = original->chars[fromIndex + i];
    }

    heapChars[newLength] = '\n';
    return takeString(vm, heapChars, (int)newLength);
}

ObjString* toLowerString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1);

    for (int offset = 0; offset < string->length; offset++) {
        heapChars[offset] = (char)tolower(string->chars[offset]);
    }
    heapChars[string->length] = '\0';
    return takeString(vm, heapChars, (int)string->length);
}

ObjString* toUpperString(VM* vm, ObjString* string) {
    if (string->length == 0) return string;
    char* heapChars = ALLOCATE(char, (size_t)string->length + 1);

    for (int offset = 0; offset < string->length; offset++) {
        heapChars[offset] = (char)toupper(string->chars[offset]);
    }
    heapChars[string->length] = '\0';
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
    char* heapChars = ALLOCATE(char, (size_t)newLength + 1);
    for (int i = 0; i < newLength; i++) {
        heapChars[i] = string->chars[i + ltLen];
    }
    heapChars[newLength] = '\n';
    return takeString(vm, heapChars, (int)newLength);
}