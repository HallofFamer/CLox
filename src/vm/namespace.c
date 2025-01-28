#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "memory.h"
#include "namespace.h"
#include "native.h"
#include "vm.h"

ObjNamespace* declareNamespace(VM* vm, uint8_t namespaceDepth) {
    ObjNamespace* enclosingNamespace = vm->rootNamespace;
    for (int i = namespaceDepth - 1; i >= 0; i--) {
        ObjString* name = AS_STRING(peek(vm, i));
        Value value;
        if (!tableGet(&enclosingNamespace->values, name, &value)) {
            enclosingNamespace = defineNativeNamespace(vm, name->chars, enclosingNamespace);
        }
        else enclosingNamespace = AS_NAMESPACE(value);
    }

    while (namespaceDepth > 0) {
        pop(vm);
        namespaceDepth--;
    }
    return enclosingNamespace;
}

Value usingNamespace(VM* vm, uint8_t namespaceDepth) {
    ObjNamespace* enclosingNamespace = vm->rootNamespace;
    Value value;
    for (int i = namespaceDepth - 1; i >= 1; i--) {
        ObjString* name = AS_STRING(peek(vm, i));
        if (!tableGet(&enclosingNamespace->values, name, &value)) {
            enclosingNamespace = defineNativeNamespace(vm, name->chars, enclosingNamespace);
        }
        else enclosingNamespace = AS_NAMESPACE(value);
    }

    ObjString* shortName = AS_STRING(peek(vm, 0));
    bool valueExists = tableGet(&enclosingNamespace->values, shortName, &value);
    while (namespaceDepth > 0) {
        pop(vm);
        namespaceDepth--;
    }

    push(vm, OBJ_VAL(shortName));
    push(vm, OBJ_VAL(enclosingNamespace));
    return valueExists ? value : NIL_VAL;
}

bool sourceFileExists(ObjString* filePath) {
    struct stat fileStat;
    return stat(filePath->chars, &fileStat) == 0;
}

ObjString* locateSourceFile(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace) {
    int length = enclosingNamespace->fullName->length + shortName->length + 5;
    char* heapChars = ALLOCATE(char, length + 1);
    int offset = 0;
    while (offset < enclosingNamespace->fullName->length) {
        char currentChar = enclosingNamespace->fullName->chars[offset];
        heapChars[offset] = (currentChar == '.') ? '/' : currentChar;
        offset++;
    }
    heapChars[offset++] = '/';

    int startIndex = offset;
    while (offset < startIndex + shortName->length) {
        heapChars[offset] = shortName->chars[offset - startIndex];
        offset++;
    }

    heapChars[offset++] = '.';
    heapChars[length - 3] = 'l';
    heapChars[length - 2] = 'o';
    heapChars[length - 1] = 'x';
    heapChars[length] = '\n';
    return takeString(vm, heapChars, length);
}

bool sourceDirectoryExists(ObjString* directoryPath) {
    struct stat directoryStat;
    if (stat(directoryPath->chars, &directoryStat) == 0) return (directoryStat.st_mode & S_IFMT) == S_IFDIR;
    return false;
}

ObjString* locateSourceDirectory(VM* vm, ObjString* shortName, ObjNamespace* enclosingNamespace) {
    int length = enclosingNamespace->fullName->length + shortName->length + 1;
    char* heapChars = ALLOCATE(char, length + 1);
    int offset = 0;
    while (offset < enclosingNamespace->fullName->length) {
        char currentChar = enclosingNamespace->fullName->chars[offset];
        heapChars[offset] = (currentChar == '.') ? '/' : currentChar;
        offset++;
    }
    heapChars[offset++] = '/';

    int startIndex = offset;
    while (offset < startIndex + shortName->length) {
        heapChars[offset] = shortName->chars[offset - startIndex];
        offset++;
    }

    heapChars[length] = '\n';
    return takeString(vm, heapChars, length);
}

InterpretResult runModule(VM* vm, ObjModule* module, bool isRootModule) {
    if (module->closure->function->isAsync) {
        Value result = runGeneratorAsync(vm, OBJ_VAL(module->closure), newArray(vm));
        return result ? INTERPRET_OK : INTERPRET_RUNTIME_ERROR;
    }
    else {
        push(vm, OBJ_VAL(module->closure));
        callClosure(vm, module->closure, 0);
        if (!isRootModule) vm->apiStackDepth++;
        InterpretResult result = run(vm);
        if (!isRootModule) vm->apiStackDepth--;
        return result;
    }
}

bool loadModule(VM* vm, ObjString* path) {
    ObjModule* lastModule = vm->currentModule;
    vm->currentModule = newModule(vm, path);

    char* source = readFile(path->chars);
    ObjFunction* function = compile(vm, source);
    free(source);
    if (function == NULL) return false;
    push(vm, OBJ_VAL(function));

    vm->currentModule->closure = newClosure(vm, function);
    pop(vm);
    InterpretResult result = runModule(vm, vm->currentModule, false);
    vm->currentModule = lastModule;
    return true;
}