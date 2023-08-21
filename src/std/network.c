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

static bool urlIsAbsolute(VM* vm, ObjInstance* url) {
    ObjString* host = AS_STRING(getObjProperty(vm, url, "host"));
    return host->length > 0;
}

static ObjString* urlToString(VM* vm, ObjInstance* url) {
    ObjString* scheme = AS_STRING(getObjProperty(vm, url, "scheme"));
    ObjString* host = AS_STRING(getObjProperty(vm, url, "host"));
    int port = AS_INT(getObjProperty(vm, url, "port"));
    ObjString* path = AS_STRING(getObjProperty(vm, url, "path"));
    ObjString* query = AS_STRING(getObjProperty(vm, url, "query"));
    ObjString* fragment = AS_STRING(getObjProperty(vm, url, "fragment"));

    ObjString* urlString = newString(vm, "");
    if (host->length > 0) {
        urlString = (scheme->length > 0) ? formattedString(vm, "%s://%s", scheme->chars, host->chars) : host;
        if (port > 0 && port < 65536) urlString = formattedString(vm, "%s:%d", urlString->chars, port);
    }
    if (path->length > 0) urlString = formattedString(vm, "%s/%s", urlString->chars, path->chars);
    if (query->length > 0) urlString = formattedString(vm, "%s&%s", urlString->chars, query->chars);
    if (fragment->length > 0) urlString = formattedString(vm, "%s#%s", urlString->chars, fragment->chars);
    return urlString;
}

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
    RETURN_BOOL(urlIsAbsolute(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(URL, isRelative) {
    ASSERT_ARG_COUNT("URL::isRelative()", 0);
    RETURN_BOOL(!urlIsAbsolute(vm, AS_INSTANCE(receiver)));
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

LOX_METHOD(URL, relativize) {
    ASSERT_ARG_COUNT("URL::relativize(url)", 1);
    ASSERT_ARG_INSTANCE_OF("URL::relativize(url)", 0, clox.std.network, URL);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* url = AS_INSTANCE(args[0]);
    if (urlIsAbsolute(vm, self) || urlIsAbsolute(vm, url)) RETURN_OBJ(url);

    ObjString* urlString = urlToString(vm, self);
    ObjString* urlString2 = urlToString(vm, url);
    int index = searchString(vm, urlString, urlString2, 0);
    if (index == 0) {
        ObjInstance* relativized = newInstance(vm, self->obj.klass);
        ObjString* relativizedURL = subString(vm, urlString, urlString2->length, urlString->length); 
        struct yuarel component;
        char fullURL[UINT8_MAX];
        sprintf_s(fullURL, UINT8_MAX, "%s%s", "https://example.com/", relativizedURL->chars);
        yuarel_parse(&component, fullURL);

        setObjProperty(vm, relativized, "scheme", OBJ_VAL(newString(vm, "")));
        setObjProperty(vm, relativized, "host", OBJ_VAL(newString(vm, "")));
        setObjProperty(vm, relativized, "port", INT_VAL(0));
        setObjProperty(vm, relativized, "path", OBJ_VAL(newString(vm, component.path != NULL ? component.path : "")));
        setObjProperty(vm, relativized, "query", OBJ_VAL(newString(vm, component.query != NULL ? component.query : "")));
        setObjProperty(vm, relativized, "fragment", OBJ_VAL(newString(vm, component.fragment != NULL ? component.fragment : "")));
        RETURN_OBJ(relativized);
    }
    RETURN_OBJ(url);
}

LOX_METHOD(URL, toString) {
    ASSERT_ARG_COUNT("URL::toString()", 0);
    RETURN_OBJ(urlToString(vm, AS_INSTANCE(receiver)));
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
    DEF_METHOD(urlClass, URL, relativize, 1);
    DEF_METHOD(urlClass, URL, toString, 0);

    ObjClass* urlMetaclass = urlClass->obj.klass;
    DEF_METHOD(urlMetaclass, URLClass, parse, 1);

    vm->currentNamespace = vm->rootNamespace;
}