#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

void registerNetworkPackage(VM* vm) {
    ObjNamespace* networkNamespace = defineNativeNamespace(vm, "network", vm->stdNamespace);
    vm->currentNamespace = networkNamespace;

    ObjClass* uriClass = defineNativeClass(vm, "URI");
    bindSuperclass(vm, uriClass, vm->objectClass);

    vm->currentNamespace = vm->rootNamespace;
}