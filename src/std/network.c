#include <stdlib.h>
#include <string.h> 

#include <curl/curl.h>

#include "network.h"
#include "../inc/yuarel.h"
#include "../vm/assert.h"
#include "../vm/dict.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/os.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

typedef struct CURLResponse {
    char* content;
    size_t size;
} CURLResponse;

static struct addrinfo* dnsGetDomainInfo(VM* vm, const char* domainName, int* status) {
    struct addrinfo hints, *result;
    void* ptr = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags |= AI_CANONNAME;

    *status = getaddrinfo(domainName, NULL, &hints, &result);
    return result;
}

static ObjString* dnsGetDomainFromIPAddress(VM* vm, const char* ipAddress, int* status) {
    struct sockaddr_in socketAddress;
    char domainString[NI_MAXHOST];
    memset(&socketAddress, 0, sizeof socketAddress);
    socketAddress.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddress, &socketAddress.sin_addr);

    *status = getnameinfo((struct sockaddr*)&socketAddress, sizeof(socketAddress), domainString, NI_MAXHOST, NULL, 0, 0);
    return newString(vm, domainString);
}

static ObjArray* dnsGetIPAddressesFromDomain(VM* vm, struct addrinfo* result) {
    char ipString[100];
    void* source = NULL;
    ObjArray* ipAddresses = newArray(vm);
    push(vm, OBJ_VAL(ipAddresses));

    while (result) {
        inet_ntop(result->ai_family, result->ai_addr->sa_data, ipString, 100);
        switch (result->ai_family) {
        case AF_INET:
            source = &((struct sockaddr_in*)result->ai_addr)->sin_addr;
            break;
        case AF_INET6:
            source = &((struct sockaddr_in6*)result->ai_addr)->sin6_addr;
            break;
        }

        if (source != NULL) {
            inet_ntop(result->ai_family, source, ipString, 100);
            valueArrayWrite(vm, &ipAddresses->elements, OBJ_VAL(newString(vm, ipString)));
        }
        result = result->ai_next;
    }

    pop(vm);
    return ipAddresses;
}

static size_t httpWriteResponse(void* contents, size_t size, size_t nmemb, void* userdata) {
    size_t realsize = size * nmemb;
    CURLResponse* mem = (CURLResponse*)userdata;

    char* ptr = realloc(mem->content, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->content = ptr;
    memcpy(&(mem->content[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->content[mem->size] = 0;
    return realsize;
}

static bool ipIsV4(ObjString* address) {
    unsigned char b1, b2, b3, b4;
    if (sscanf_s(address->chars, "%hhu.%hhu.%hhu.%hhu", &b1, &b2, &b3, &b4) != 4) return false;
    char buffer[16];
    snprintf(buffer, 16, "%hhu.%hhu.%hhu.%hhu", b1, b2, b3, b4);
    return !strcmp(address->chars, buffer);
}

static bool ipIsV6(ObjString* address) {
    unsigned short b1, b2, b3, b4, b5, b6, b7, b8;
    if (sscanf_s(address->chars, "%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx", &b1, &b2, &b3, &b4, &b5, &b6, &b7, &b8) != 8) return false;
    char buffer[40];
    snprintf(buffer, 40, "%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx", b1, b2, b3, b4, b5, b6, b7, b8);
    return !strcmp(address->chars, buffer);
}

static int ipParseBlock(VM* vm, ObjString* address, int startIndex, int endIndex, int radix) {
    ObjString* bString = subString(vm, address, startIndex, endIndex);
    return strtol(bString->chars, NULL, radix);
}

static void ipWriteByteArray(VM* vm, ObjArray* array, ObjString* address, int radix) {
    push(vm, OBJ_VAL(array));
    int d = 0;
    for (int i = 0; i < address->length; i++) {
        if (address->chars[i] == '.' || address->chars[i] == ':') {
            valueArrayWrite(vm, &array->elements, INT_VAL(ipParseBlock(vm, address, d, i, radix)));
            d = i + 1;
        }
    }
    valueArrayWrite(vm, &array->elements, INT_VAL(ipParseBlock(vm, address, d, address->length - 1, radix)));
    pop(vm);
}

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

LOX_METHOD(Domain, init) {
    ASSERT_ARG_COUNT("Domain::init(name)", 1);
    ASSERT_ARG_TYPE("Domain::init(name)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "name", args[0]);
    RETURN_OBJ(self);
}

LOX_METHOD(Domain, ipAddresses) {
    ASSERT_ARG_COUNT("Domain::ipAddresses()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* name = AS_STRING(getObjProperty(vm, self, "name"));

    int status = -1;
    struct addrinfo* result = dnsGetDomainInfo(vm, name->chars, &status);
    if (status) {
        raiseError(vm, "Failed to get IP address information for domain.");
        RETURN_NIL;
    }

    ObjArray* ipAddresses = dnsGetIPAddressesFromDomain(vm, result);
    freeaddrinfo(result);
    RETURN_OBJ(ipAddresses);
}

LOX_METHOD(Domain, toString) {
    ASSERT_ARG_COUNT("Domain::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* name = AS_STRING(getObjProperty(vm, self, "name"));
    RETURN_OBJ(name);
}

LOX_METHOD(HTTPClient, close) {
    ASSERT_ARG_COUNT("HTTPClient::close()", 0);
    curl_global_cleanup();
    RETURN_NIL;
}

LOX_METHOD(HTTPClient, get) {
    ASSERT_ARG_COUNT("HTTPClient::get(url)", 1);
    ASSERT_ARG_TYPE("HTTPClient::get(url)", 0, String);
    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a GET request using CURL.");
        RETURN_NIL;
    }

    ObjString* url = AS_STRING(args[0]);
    CURLResponse curlResponse = { .content = malloc(0), .size = 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url->chars);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpWriteResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&curlResponse);
    CURLcode curlCode = curl_easy_perform(curl);

    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a GET request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }
    curl_easy_cleanup(curl);
    ObjString* response = copyString(vm, curlResponse.content, curlResponse.size);
    RETURN_OBJ(response);
}

LOX_METHOD(HTTPClient, init) {
    ASSERT_ARG_COUNT("HTTPClient::init()", 0);
    curl_global_init(CURL_GLOBAL_ALL);
    RETURN_VAL(receiver);
}

LOX_METHOD(IPAddress, domain) {
    ASSERT_ARG_COUNT("IPAddress::domain", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* address = AS_STRING(getObjProperty(vm, self, "address"));

    int status = -1;
    ObjString* domain = dnsGetDomainFromIPAddress(vm, address->chars, &status);
    if (status) {
        raiseError(vm, "Failed to get domain information for IP Address.");
        RETURN_NIL;
    }
    RETURN_OBJ(domain);
}

LOX_METHOD(IPAddress, init) {
    ASSERT_ARG_COUNT("IPAddress::init(address)", 1);
    ASSERT_ARG_TYPE("IPAddress::init(address)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* address = AS_STRING(args[0]);
    int version = -1;
    if (ipIsV4(address)) version = 4;
    else if (ipIsV6(address)) version = 6;
    else {
        raiseError(vm, "Invalid IP address specified.");
        RETURN_NIL;
    }
    setObjProperty(vm, self, "address", args[0]);
    setObjProperty(vm, self, "version", INT_VAL(version));
    RETURN_OBJ(self);
}

LOX_METHOD(IPAddress, isIPV4) {
    ASSERT_ARG_COUNT("IPAddress::isIPV4()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int version = AS_INT(getObjProperty(vm, self, "version"));
    RETURN_BOOL(version == 4);
}

LOX_METHOD(IPAddress, isIPV6) {
    ASSERT_ARG_COUNT("IPAddress::isIPV6()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int version = AS_INT(getObjProperty(vm, self, "version"));
    RETURN_BOOL(version == 6);
}

LOX_METHOD(IPAddress, toArray) {
    ASSERT_ARG_COUNT("IPAddress::toArray()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* address = AS_STRING(getObjProperty(vm, self, "address"));
    int version = AS_INT(getObjProperty(vm, self, "version"));
    ObjArray* array = newArray(vm);
    ipWriteByteArray(vm, array, address, version == 6 ? 16 : 10);
    RETURN_OBJ(array);
}

LOX_METHOD(IPAddress, toString) {
    ASSERT_ARG_COUNT("IPAddress::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value address = getObjProperty(vm, self, "address");
    RETURN_OBJ(AS_STRING(address));
}

LOX_METHOD(Socket, close) {
    ASSERT_ARG_COUNT("Socket::close()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    closesocket(descriptor);
    RETURN_NIL;
}

LOX_METHOD(Socket, connect) { 
    ASSERT_ARG_COUNT("Socket::connect(ipAddress)", 1);
    ASSERT_ARG_INSTANCE_OF("Socket::connect(IPAddress)", 0, clox.std.network, IPAddress);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* ipAddress = AS_INSTANCE(args[0]);

    struct sockaddr_in socketAddress = { 0 };
    ObjString* ipString = AS_STRING(getObjProperty(vm, ipAddress, "address"));
    if (inet_pton(AF_INET, ipString->chars, &socketAddress.sin_addr) <= 0) {
        raiseError(vm, "Invalid socket address provided.");
        RETURN_NIL;
    }

    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor")); 
    if (connect(descriptor, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) <= 0) {
        raiseError(vm, "Socket connection failed.");
        RETURN_NIL;
    }
    RETURN_NIL;
}

LOX_METHOD(Socket, init) {
    ASSERT_ARG_COUNT("Socket::init(addressFamily, socketType, protocolType)", 3);
    ASSERT_ARG_TYPE("Socket::init(addressFamily, socketType, protocolType)", 0, Int);
    ASSERT_ARG_TYPE("Socket::init(addressFamily, socketType, protocolType)", 1, Int);
    ASSERT_ARG_TYPE("Socket::init(addressFamily, socketType, protocolType)", 2, Int);

    SOCKET descriptor = socket(AS_INT(args[0]), AS_INT(args[1]), AS_INT(args[2]));
    if (descriptor == INVALID_SOCKET) {
        raiseError(vm, "Socket creation failed...");
        RETURN_NIL;
    }
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "addressFamily", args[0]);
    setObjProperty(vm, self, "socketType", args[1]);
    setObjProperty(vm, self, "protocolType", args[2]);
    setObjProperty(vm, self, "descriptor", INT_VAL(descriptor));
    RETURN_OBJ(self);
}

LOX_METHOD(Socket, receive) {
    ASSERT_ARG_COUNT("Socket::receive()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    char message[UINT8_MAX] = "";
    if (recv(descriptor, message, UINT8_MAX, 0) < 0) {
        raiseError(vm, "Failed to receive message from socket.");
        RETURN_NIL;
    }
    RETURN_STRING(message, (int)strlen(message));
}

LOX_METHOD(Socket, send) {
    ASSERT_ARG_COUNT("Socket::send(message)", 1);
    ASSERT_ARG_TYPE("Socket::send(message)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* message = AS_STRING(args[0]);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    if (send(descriptor, message->chars, message->length, 0) < 0) raiseError(vm, "Failed to send message to socket.");
    RETURN_NIL;
}

LOX_METHOD(Socket, toString) {
    ASSERT_ARG_COUNT("Socket::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value addressFamily = getObjProperty(vm, self, "addressFamily");
    Value socketType = getObjProperty(vm, self, "socketType");
    Value protocolType = getObjProperty(vm, self, "protocolType");
    RETURN_STRING_FMT("Socket - AddressFamily: %d, SocketType: %d, ProtocolType: %d", AS_INT(addressFamily), AS_INT(socketType), AS_INT(protocolType));
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
    RETURN_OBJ(self);
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

    ObjClass* ipAddressClass = defineNativeClass(vm, "IPAddress");
    bindSuperclass(vm, ipAddressClass, vm->objectClass);
    DEF_METHOD(ipAddressClass, IPAddress, domain, 0);
    DEF_METHOD(ipAddressClass, IPAddress, init, 1);
    DEF_METHOD(ipAddressClass, IPAddress, isIPV4, 0);
    DEF_METHOD(ipAddressClass, IPAddress, isIPV6, 0);
    DEF_METHOD(ipAddressClass, IPAddress, toArray, 0);
    DEF_METHOD(ipAddressClass, IPAddress, toString, 0);

    ObjClass* domainClass = defineNativeClass(vm, "Domain");
    bindSuperclass(vm, domainClass, vm->objectClass);
    DEF_METHOD(domainClass, Domain, init, 1);
    DEF_METHOD(domainClass, Domain, ipAddresses, 0);
    DEF_METHOD(domainClass, Domain, toString, 0);

    ObjClass* closableTrait = getNativeClass(vm, "clox.std.io", "TClosable");
    ObjClass* socketClass = defineNativeClass(vm, "Socket");
    bindSuperclass(vm, socketClass, vm->objectClass);
    bindTrait(vm, socketClass, closableTrait);
    DEF_METHOD(socketClass, Socket, close, 0);
    DEF_METHOD(socketClass, Socket, connect, 1);
    DEF_METHOD(socketClass, Socket, init, 3);
    DEF_METHOD(socketClass, Socket, receive, 0);
    DEF_METHOD(socketClass, Socket, send, 1);
    DEF_METHOD(socketClass, Socket, toString, 0);

    ObjClass* socketMetaclass = socketClass->obj.klass;
    setClassProperty(vm, socketClass, "afUNSPEC", INT_VAL(AF_UNSPEC));
    setClassProperty(vm, socketClass, "afUNIX", INT_VAL(AF_UNIX));
    setClassProperty(vm, socketClass, "afINET", INT_VAL(AF_INET));
    setClassProperty(vm, socketClass, "afIPX", INT_VAL(AF_IPX));
    setClassProperty(vm, socketClass, "afDECnet", INT_VAL(AF_DECnet));
    setClassProperty(vm, socketClass, "afAPPLETALK", INT_VAL(AF_APPLETALK));
    setClassProperty(vm, socketClass, "afINET6", INT_VAL(AF_INET6));
    setClassProperty(vm, socketClass, "sockSTREAM", INT_VAL(SOCK_STREAM));
    setClassProperty(vm, socketClass, "sockDGRAM", INT_VAL(SOCK_DGRAM));
    setClassProperty(vm, socketClass, "sockRAW", INT_VAL(SOCK_RAW));
    setClassProperty(vm, socketClass, "sockRDM", INT_VAL(SOCK_RDM));
    setClassProperty(vm, socketClass, "sockSEQPACKET", INT_VAL(SOCK_SEQPACKET));
    setClassProperty(vm, socketClass, "protoIP", INT_VAL(IPPROTO_IP));
    setClassProperty(vm, socketClass, "protoICMP", INT_VAL(IPPROTO_ICMP));
    setClassProperty(vm, socketClass, "protoTCP", INT_VAL(IPPROTO_TCP));
    setClassProperty(vm, socketClass, "protoUDP", INT_VAL(IPPROTO_UDP));
    setClassProperty(vm, socketClass, "protoICMPV6", INT_VAL(IPPROTO_ICMPV6));
    setClassProperty(vm, socketClass, "protoRAW", INT_VAL(IPPROTO_RAW));

    ObjClass* httpClientClass = defineNativeClass(vm, "HTTPClient");
    bindSuperclass(vm, httpClientClass, vm->objectClass);
    bindTrait(vm, httpClientClass, closableTrait);
    DEF_METHOD(httpClientClass, HTTPClient, close, 0);
    DEF_METHOD(httpClientClass, HTTPClient, get, 1);
    DEF_METHOD(httpClientClass, HTTPClient, init, 0);

    vm->currentNamespace = vm->rootNamespace;
}