#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "native.h"
#include "network.h"
#include "object.h"
#include "vm.h"

static NetworkData* networkLoadData(VM* vm, ObjInstance* network, ObjPromise* promise) {
    NetworkData* data = ALLOCATE_STRUCT(NetworkData);
    if (data != NULL) {
        data->vm = vm;
        data->network = network;
        data->promise = promise;
    }
    return data;
}

struct addrinfo* dnsGetDomainInfo(VM* vm, const char* domainName, int* status) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags |= AI_CANONNAME;

    uv_getaddrinfo_t netGetAddrInfo;
    *status = uv_getaddrinfo(vm->eventLoop, &netGetAddrInfo, NULL, domainName, "80", &hints);
    return netGetAddrInfo.addrinfo;
}

ObjPromise* dnsGetDomainInfoAsync(VM* vm, ObjInstance* domain, uv_getaddrinfo_cb callback) {
    uv_getaddrinfo_t* netGetAddrInfo = ALLOCATE_STRUCT(uv_getaddrinfo_t);
    ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
    if (netGetAddrInfo == NULL) return NULL;
    else {
        netGetAddrInfo->data = networkLoadData(vm, domain, promise);
        char* domainName = AS_CSTRING(getObjProperty(vm, domain, "name"));
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));

        hints.ai_family = PF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags |= AI_CANONNAME;

        uv_getaddrinfo(vm->eventLoop, netGetAddrInfo, callback, domainName, "80", &hints);
        return promise;
    }
}

ObjString* dnsGetDomainFromIPAddress(VM* vm, const char* ipAddress, int* status) {
    struct sockaddr_in socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    inet_pton(AF_INET, ipAddress, &socketAddress.sin_addr);
    uv_getnameinfo_t netGetNameInfo;
    *status = uv_getnameinfo(vm->eventLoop, &netGetNameInfo, NULL, (struct sockaddr*)&socketAddress, 0);
    return newString(vm, netGetNameInfo.host);
}

ObjPromise* dnsGetDomainFromIPAddressAsync(VM* vm, ObjInstance* ipAddress, uv_getnameinfo_cb callback) {
    uv_getnameinfo_t* netGetNameInfo = ALLOCATE_STRUCT(uv_getnameinfo_t);
    ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
    if (netGetNameInfo == NULL) return NULL;
    else {
        netGetNameInfo->data = networkLoadData(vm, ipAddress, promise);
        char* address = AS_CSTRING(getObjProperty(vm, ipAddress, "address"));
        struct sockaddr_in socketAddress;
        memset(&socketAddress, 0, sizeof(socketAddress));
        socketAddress.sin_family = AF_INET;
        inet_pton(AF_INET, address, &socketAddress.sin_addr);
        uv_getnameinfo(vm->eventLoop, netGetNameInfo, callback, (struct sockaddr*)&socketAddress, 0);
        return promise;
    }
    return newPromise(vm, PROMISE_FULFILLED, NIL_VAL, NIL_VAL);
}

ObjArray* dnsGetIPAddressesFromDomain(VM* vm, struct addrinfo* result) {
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

void dnsOnGetAddrInfo(uv_getaddrinfo_t* netGetAddrInfo, int status, struct addrinfo* result) {
    NetworkData* data = netGetAddrInfo->data;
    LOOP_PUSH_DATA(data);

    if (status < 0) {
        ObjException* exception = createNativeException(data->vm, "clox.std.net.DomainHostException", "Failed to resolve IP addresses for domain.");
        promiseReject(data->vm, data->promise, OBJ_VAL(exception));
    }
    else {
        ObjArray* ipAddresses = dnsGetIPAddressesFromDomain(data->vm, result);
        promiseFulfill(data->vm, data->promise, OBJ_VAL(ipAddresses));
    }

    uv_freeaddrinfo(result);
    free(netGetAddrInfo);
    LOOP_POP_DATA(data);
}

void dnsOnGetNameInfo(uv_getnameinfo_t* netGetNameInfo, int status, const char* hostName, const char* service) {
    NetworkData* data = netGetNameInfo->data;
    LOOP_PUSH_DATA(data);

    if (status < 0) {
        ObjException* exception = createNativeException(data->vm, "clox.std.net.IPAddressException", "Failed to get domain name for IP Address.");
        promiseReject(data->vm, data->promise, OBJ_VAL(exception));
    }
    else {
        ObjString* domain = newString(data->vm, netGetNameInfo->host);
        promiseFulfill(data->vm, data->promise, OBJ_VAL(domain));
    }

    free(netGetNameInfo);
    LOOP_POP_DATA(data);
}

bool ipIsV4(ObjString* address) {
    unsigned char b1, b2, b3, b4;
    if (sscanf_s(address->chars, "%hhu.%hhu.%hhu.%hhu", &b1, &b2, &b3, &b4) != 4) return false;
    char buffer[16];
    snprintf(buffer, 16, "%hhu.%hhu.%hhu.%hhu", b1, b2, b3, b4);
    return !strcmp(address->chars, buffer);
}

bool ipIsV6(ObjString* address) {
    unsigned short b1, b2, b3, b4, b5, b6, b7, b8;
    if (sscanf_s(address->chars, "%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx", &b1, &b2, &b3, &b4, &b5, &b6, &b7, &b8) != 8) return false;
    char buffer[40];
    snprintf(buffer, 40, "%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx:%04hx", b1, b2, b3, b4, b5, b6, b7, b8);
    return !strcmp(address->chars, buffer);
}

int ipParseBlock(VM* vm, ObjString* address, int startIndex, int endIndex, int radix) {
    ObjString* bString = subString(vm, address, startIndex, endIndex);
    return strtol(bString->chars, NULL, radix);
}

void ipWriteByteArray(VM* vm, ObjArray* array, ObjString* address, int radix) {
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

bool urlIsAbsolute(VM* vm, ObjInstance* url) {
    ObjString* host = AS_STRING(getObjProperty(vm, url, "host"));
    return (host->length > 0);
}

ObjString* urlToString(VM* vm, ObjInstance* url) {
    ObjString* scheme = AS_STRING(getObjProperty(vm, url, "scheme"));
    ObjString* host = AS_STRING(getObjProperty(vm, url, "host"));
    int port = AS_INT(getObjProperty(vm, url, "port"));
    ObjString* path = AS_STRING(getObjProperty(vm, url, "path"));
    ObjString* query = AS_STRING(getObjProperty(vm, url, "query"));
    ObjString* fragment = AS_STRING(getObjProperty(vm, url, "fragment"));

    ObjString* urlString = emptyString(vm);
    if (host->length > 0) {
        urlString = (scheme->length > 0) ? formattedString(vm, "%s://%s", scheme->chars, host->chars) : host;
        if (port > 0 && port < 65536) urlString = formattedString(vm, "%s:%d", urlString->chars, port);
    }
    if (path->length > 0) urlString = formattedString(vm, "%s/%s", urlString->chars, path->chars);
    if (query->length > 0) urlString = formattedString(vm, "%s&%s", urlString->chars, query->chars);
    if (fragment->length > 0) urlString = formattedString(vm, "%s#%s", urlString->chars, fragment->chars);
    return urlString;
}