#pragma once
#ifndef clox_network_h
#define clox_network_h

#include "loop.h"

typedef struct NetworkData {
    VM* vm;
    ObjInstance* network;
    ObjPromise* promise;
} NetworkData;

struct addrinfo* dnsGetDomainInfo(VM* vm, const char* domainName, int* status);
ObjPromise* dnsGetDomainInfoAsync(VM* vm, ObjInstance* domain, uv_getaddrinfo_cb callback);
ObjString* dnsGetDomainFromIPAddress(VM* vm, const char* ipAddress, int* status);
ObjPromise* dnsGetDomainFromIPAddressAsync(VM* vm, ObjInstance* ipAddress, uv_getnameinfo_cb callback);
ObjArray* dnsGetIPAddressesFromDomain(VM* vm, struct addrinfo* result);
void dnsOnGetAddrInfo(uv_getaddrinfo_t* netGetAddrInfo, int status, struct addrinfo* result);
void dnsOnGetNameInfo(uv_getnameinfo_t* netGetNameInfo, int status, const char* hostName, const char* service);
bool ipIsV4(ObjString* address);
bool ipIsV6(ObjString* address);
int ipParseBlock(VM* vm, ObjString* address, int startIndex, int endIndex, int radix);
void ipWriteByteArray(VM* vm, ObjArray* array, ObjString* address, int radix);
bool urlIsAbsolute(VM* vm, ObjInstance* url);
ObjString* urlToString(VM* vm, ObjInstance* url);

#endif // !clox_network_h