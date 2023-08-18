#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

LOX_METHOD(URL, init) {
    ASSERT_ARG_COUNT("URL::init(scheme, host, port, path, query, fragment)", 6);
    ASSERT_ARG_TYPE("URL::init(scheme, host, port, path, query, fragment)", 0, String);
    ASSERT_ARG_TYPE("URL::init(scheme, host, port, path, query, fragment)", 1, String);
    ASSERT_ARG_TYPE("URL::init(scheme, host, port, path, query, fragment)", 2, Int);
    ASSERT_ARG_TYPE("URL::init(scheme, host, port, path, query, fragment)", 3, String);
    ASSERT_ARG_TYPE("URL::init(scheme, host, port, path, query, fragment)", 4, String);
    ASSERT_ARG_TYPE("URL::init(scheme, host, port, path, query, fragment)", 5, String);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "scheme", args[0]);
    setObjProperty(vm, self, "host", args[1]);
    setObjProperty(vm, self, "port", args[2]);
    setObjProperty(vm, self, "path", args[3]);
    setObjProperty(vm, self, "query", args[4]);
    setObjProperty(vm, self, "fragment", args[5]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(URL, isAbsolute) {
    ASSERT_ARG_COUNT("URL::isAbsolute()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* host = AS_STRING(getObjProperty(vm, self, "host"));
    RETURN_BOOL(host->length > 0);
}

LOX_METHOD(URL, isRelative) {
    ASSERT_ARG_COUNT("URL::isRelative()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* host = AS_STRING(getObjProperty(vm, self, "host"));
    RETURN_BOOL(host->length == 0);
}

LOX_METHOD(URL, toString) {
    ASSERT_ARG_COUNT("URL::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* scheme = AS_STRING(getObjProperty(vm, self, "scheme"));
    ObjString* host = AS_STRING(getObjProperty(vm, self, "host"));
    int port = AS_INT(getObjProperty(vm, self, "port"));
    ObjString* path = AS_STRING(getObjProperty(vm, self, "path"));
    ObjString* query = AS_STRING(getObjProperty(vm, self, "query"));
    ObjString* fragment = AS_STRING(getObjProperty(vm, self, "fragment"));

    ObjString* uriString = newString(vm, "");
    if (host->length > 0) {
        uriString = (scheme->length > 0) ? formattedString(vm, "%s://%s", scheme->chars, host->chars) : host;
        if (port > 0 && port < 65536) uriString = formattedString(vm, "%s:%d", uriString->chars, port);
    }
    if (path->length > 0) uriString = formattedString(vm, "%s/%s", uriString->chars, path->chars);
    if (query->length > 0) uriString = formattedString(vm, "%s&%s", uriString->chars, query->chars);
    if (fragment->length > 0) uriString = formattedString(vm, "%s#%s", uriString->chars, fragment->chars);
    RETURN_OBJ(uriString);
}

void registerNetworkPackage(VM* vm) {
    ObjNamespace* networkNamespace = defineNativeNamespace(vm, "network", vm->stdNamespace);
    vm->currentNamespace = networkNamespace;

    ObjClass* urlClass = defineNativeClass(vm, "URL");
    bindSuperclass(vm, urlClass, vm->objectClass);
    DEF_METHOD(urlClass, URL, init, 6);
    DEF_METHOD(urlClass, URL, isAbsolute, 0);
    DEF_METHOD(urlClass, URL, isRelative, 0);
    DEF_METHOD(urlClass, URL, toString, 0);

    vm->currentNamespace = vm->rootNamespace;
}