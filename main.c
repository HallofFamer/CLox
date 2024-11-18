#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/compiler/chunk.h"
#include "src/common/common.h"
#include "src/common/os.h"
#include "src/inc/ini.h"
#include "src/vm/debug.h"
#include "src/vm/vm.h"

static void repl(VM* vm) {
    printf("REPL for CLox version %s\n", vm->config.version);
    vm->currentModule = newModule(vm, emptyString(vm));
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

static void runFile(VM* vm, const char* filePath) {
    ObjString* path = newString(vm, filePath);
    vm->currentModule = newModule(vm, path);

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
    runAtStartup();
    atexit(runAtExit);

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
  
    uv_run(vm.eventLoop, UV_RUN_DEFAULT);
    freeVM(&vm);
    return 0;
}
