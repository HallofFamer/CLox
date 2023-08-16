#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

LOX_METHOD(URI, init) {
    ASSERT_ARG_COUNT("URI::init(scheme, host, port, path, query, fragment)", 6);
    ASSERT_ARG_TYPE("URI::init(scheme, host, port, path, query, fragment)", 0, String);
    ASSERT_ARG_TYPE("URI::init(scheme, host, port, path, query, fragment)", 1, String);
    ASSERT_ARG_TYPE("URI::init(scheme, host, port, path, query, fragment)", 2, Int);
    ASSERT_ARG_TYPE("URI::init(scheme, host, port, path, query, fragment)", 3, String);
    ASSERT_ARG_TYPE("URI::init(scheme, host, port, path, query, fragment)", 4, String);
    ASSERT_ARG_TYPE("URI::init(scheme, host, port, path, query, fragment)", 5, String);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "scheme", args[0]);
    setObjProperty(vm, self, "host", args[1]);
    setObjProperty(vm, self, "port", args[2]);
    setObjProperty(vm, self, "path", args[3]);
    setObjProperty(vm, self, "query", args[4]);
    setObjProperty(vm, self, "fragment", args[5]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(URI, toString) {
    ASSERT_ARG_COUNT("URI::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* scheme = AS_STRING(getObjProperty(vm, self, "scheme"));
    ObjString* host = AS_STRING(getObjProperty(vm, self, "host"));
    int port = AS_INT(getObjProperty(vm, self, "port"));
    ObjString* path = AS_STRING(getObjProperty(vm, self, "path"));
    ObjString* query = AS_STRING(getObjProperty(vm, self, "query"));
    ObjString* fragment = AS_STRING(getObjProperty(vm, self, "fragment"));

    ObjString* uriString = formattedString(vm, "%s://%s", scheme->chars, host->chars);
    if (port > 0) uriString = formattedString(vm, "%s:%d", uriString->chars, port);
    if (path->length > 0) uriString = formattedString(vm, "%s/%s", uriString->chars, path->chars);
    if (query->length > 0) uriString = formattedString(vm, "%s&%s", uriString->chars, query->chars);
    if (fragment->length > 0) uriString = formattedString(vm, "%s#%s", uriString->chars, fragment->chars);
    RETURN_OBJ(uriString);
}

void registerNetworkPackage(VM* vm) {
    ObjNamespace* networkNamespace = defineNativeNamespace(vm, "network", vm->stdNamespace);
    vm->currentNamespace = networkNamespace;

    ObjClass* uriClass = defineNativeClass(vm, "URI");
    bindSuperclass(vm, uriClass, vm->objectClass);
    DEF_METHOD(uriClass, URI, init, 6);
    DEF_METHOD(uriClass, URI, toString, 0);

    vm->currentNamespace = vm->rootNamespace;
}