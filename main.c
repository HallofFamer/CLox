#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/inc/ini.h"
#include "src/vm/common.h"
#include "src/vm/chunk.h"
#include "src/vm/debug.h"
#include "src/vm/vm.h"

static void repl(VM* vm) {
    printf("REPL for CLox version %s\n", vm->config.version);
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        interpret(vm, line);
    }
}

static char* readFile(const char* path) {
    FILE* file;
    fopen_s(&file, path, "rb");

    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(VM* vm, const char* filePath) {
    char* source = readFile(filePath);
    InterpretResult result = interpret(vm, source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

static void runScript(VM* vm, const char* path, const char* script) {
    char scriptPath[UINT8_MAX];
    if (strlen(path) + strlen(script) > UINT8_MAX) {
        printf("file path/name too long...");
        exit(74);
    }
    int length = sprintf_s(scriptPath, UINT8_MAX, "%s%s", path, script);
    runFile(vm, scriptPath);
}

int main(int argc, char* argv[]) {
    VM vm;
    initVM(&vm);

    if (strlen(vm.config.script) > 0) {
        runScript(&vm, vm.config.path, vm.config.script);
    }
    else if (argc == 1) {
        repl(&vm);
    } 
    else if (argc == 2) {
        runFile(&vm, argv[1]);
    } 
    else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);
    }
  
    freeVM(&vm);
    return 0;
}
