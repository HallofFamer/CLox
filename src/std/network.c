#include <stdlib.h>
#include <string.h>

#include "network.h"
#include "../inc/yuarel.h"
#include "../vm/assert.h"
#include "../vm/dict.h"
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

LOX_METHOD(URL, pathArray) {
    ASSERT_ARG_COUNT("URL::pathArray()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* path = AS_STRING(getObjProperty(vm, self, "path"));
    if (path->length == 0) RETURN_NIL;
    else {
        char* paths[UINT4_MAX];
        int length = yuarel_split_path(path->chars, paths, 3);
        if (length == -1) {
            raiseError(vm, "Failed to parse path from URL.");
            RETURN_NIL;
        }

        ObjArray* pathArray = newArray(vm);
        push(vm, OBJ_VAL(pathArray));
        for (int i = 0; i < length; i++) {
            ObjString* subPath = newString(vm, paths[i]);
            valueArrayWrite(vm, &pathArray->elements, OBJ_VAL(subPath));
        }
        pop(vm);
        RETURN_OBJ(pathArray);
    }
}

LOX_METHOD(URL, queryDict) {
    ASSERT_ARG_COUNT("URL::queryDict()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* query = AS_STRING(getObjProperty(vm, self, "query"));
    if (query->length == 0) RETURN_NIL;
    else {
        struct yuarel_param params[UINT4_MAX];
        int length = yuarel_parse_query(query->chars, '&', params, UINT4_MAX);
        if (length == -1) {
            raiseError(vm, "Failed to parse query parameters from URL.");
            RETURN_NIL;
        }

        ObjDictionary* queryDict = newDictionary(vm);
        push(vm, OBJ_VAL(queryDict));
        for (int i = 0; i < length; i++) {
            ObjString* key = newString(vm, params[i].key);
            ObjString* value = newString(vm, params[i].val);
            dictSet(vm, queryDict, OBJ_VAL(key), OBJ_VAL(value));
        }     
        pop(vm);
        RETURN_OBJ(queryDict);
    }
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

LOX_METHOD(URLClass, parse) {
    ASSERT_ARG_COUNT("URL class::parse(url)", 1);
    ASSERT_ARG_TYPE("URL class::parse(url)", 0, String);
    ObjInstance* instance = newInstance(vm, AS_CLASS(receiver));
    ObjString* url = AS_STRING(args[0]);
    struct yuarel component;
    if (yuarel_parse(&component, url->chars) == -1) {
        raiseError(vm, "Failed to parse url.");
        RETURN_NIL;
    }

    setObjProperty(vm, instance, "scheme", OBJ_VAL(newString(vm, component.scheme != NULL ? component.scheme : "")));
    setObjProperty(vm, instance, "host", OBJ_VAL(newString(vm, component.host != NULL ? component.host : "")));
    setObjProperty(vm, instance, "port", INT_VAL(component.port));
    setObjProperty(vm, instance, "path", OBJ_VAL(newString(vm, component.path != NULL ? component.path : "")));
    setObjProperty(vm, instance, "query", OBJ_VAL(newString(vm, component.query != NULL ? component.query : "")));
    setObjProperty(vm, instance, "fragment", OBJ_VAL(newString(vm, component.fragment != NULL ? component.fragment : "")));
    RETURN_OBJ(instance);
}

void registerNetworkPackage(VM* vm) {
    ObjNamespace* networkNamespace = defineNativeNamespace(vm, "network", vm->stdNamespace);
    vm->currentNamespace = networkNamespace;

    ObjClass* urlClass = defineNativeClass(vm, "URL");
    bindSuperclass(vm, urlClass, vm->objectClass);
    DEF_METHOD(urlClass, URL, init, 6);
    DEF_METHOD(urlClass, URL, isAbsolute, 0);
    DEF_METHOD(urlClass, URL, isRelative, 0);
    DEF_METHOD(urlClass, URL, pathArray, 0);
    DEF_METHOD(urlClass, URL, queryDict, 0);
    DEF_METHOD(urlClass, URL, toString, 0);

    ObjClass* urlMetaclass = urlClass->obj.klass;
    DEF_METHOD(urlMetaclass, URLClass, parse, 1);

    vm->currentNamespace = vm->rootNamespace;
}