#include <stdlib.h> 
#include <string.h> 

#include "net.h"
#include "../inc/yuarel.h"
#include "../vm/assert.h"
#include "../vm/dict.h"
#include "../vm/native.h"
#include "../vm/network.h"
#include "../vm/object.h"
#include "../vm/os.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

LOX_METHOD(Domain, __init__) {
    ASSERT_ARG_COUNT("Domain::__init__(name)", 1);
    ASSERT_ARG_TYPE("Domain::__init__(name)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "name", args[0]);
    RETURN_OBJ(self);
}

LOX_METHOD(Domain, getIPAddresses) {
    ASSERT_ARG_COUNT("Domain::getIPAddresses()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* name = AS_STRING(getObjProperty(vm, self, "name"));

    int status = -1;
    struct addrinfo* result = dnsGetDomainInfo(vm, name->chars, &status);
    if (result == NULL) THROW_EXCEPTION(clox.std.net.DomainHostException, "Unable to get domain info due to out of memory.");
    if (status) THROW_EXCEPTION(clox.std.net.IPAddressException, "Failed to get IP address information for domain.");

    ObjArray* ipAddresses = dnsGetIPAddressesFromDomain(vm, result);
    uv_freeaddrinfo(result);
    RETURN_OBJ(ipAddresses);
}

LOX_METHOD(Domain, getIPAddressesAsync) {
    ASSERT_ARG_COUNT("Domain::getIPAddressesAsync()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjPromise* promise = dnsGetDomainInfoAsync(vm, self, dnsOnGetAddrInfo);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.net.DomainHostException, "Failed to get IP Addresses from Domain.");
    RETURN_OBJ(promise);
}

LOX_METHOD(Domain, toString) {
    ASSERT_ARG_COUNT("Domain::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* name = AS_STRING(getObjProperty(vm, self, "name"));
    RETURN_OBJ(name);
}

LOX_METHOD(HTTPClient, __init__) {
    ASSERT_ARG_COUNT("HTTPClient::__init__()", 0);
    curl_global_init(CURL_GLOBAL_ALL);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjRecord* curlM = newRecord(vm, curl_multi_init());
    curlM->shouldFree = false;
    setObjProperty(vm, self, "curlM", OBJ_VAL(curlM));
    RETURN_VAL(receiver);
}

LOX_METHOD(HTTPClient, close) {
    ASSERT_ARG_COUNT("HTTPClient::close()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjRecord* curlM = AS_RECORD(getObjProperty(vm, self, "curlM"));
    curl_multi_cleanup((CURLM*)curlM->data);
    curl_global_cleanup();
    RETURN_NIL;
}

LOX_METHOD(HTTPClient, delete) {
    ASSERT_ARG_COUNT("HTTPClient::delete(url)", 1);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::delete(url)", 0, clox.std.lang.String, clox.std.net.URL);
    ObjString* url = httpRawURL(vm, args[0]);
    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a DELETE request using CURL.");

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_DELETE, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete a DELETE request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, download) {
    ASSERT_ARG_COUNT("HTTPClient::download(src, dest)", 2);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::download(src, dest)", 0, clox.std.lang.String, clox.std.net.URL);
    ASSERT_ARG_TYPE("HttpClient::download(src, dest)", 1, String);

    ObjString* src = httpRawURL(vm, args[0]);
    ObjString* dest = AS_STRING(args[1]);
    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a request to download file using CURL.");

    CURLcode curlCode = httpDownloadFile(vm, src, dest, curl);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to download file from URL.");
    }
    curl_easy_cleanup(curl);
    RETURN_NIL;
}

LOX_METHOD(HTTPClient, get) {
    ASSERT_ARG_COUNT("HTTPClient::get(url)", 1);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::get(url)", 0, clox.std.lang.String, clox.std.net.URL);
    ObjString* url = httpRawURL(vm, args[0]);
    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a GET request using CURL.");

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_GET, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete a GET request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, head) {
    ASSERT_ARG_COUNT("HTTPClient::head(url)", 1);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::head(url)", 0, clox.std.lang.String, clox.std.net.URL);
    ObjString* url = httpRawURL(vm, args[0]);
    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a HEAD request using CURL.");

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_HEAD, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete a HEAD request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, options) {
    ASSERT_ARG_COUNT("HTTPClient::options(url)", 1);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::options(url)", 0, clox.std.lang.String, clox.std.net.URL);
    ObjString* url = httpRawURL(vm, args[0]);
    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate an OPTIONS request using CURL.");

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_OPTIONS, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete an OPTIONS request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, patch) {
    ASSERT_ARG_COUNT("HTTPClient::put(url, data)", 2);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::put(url, data)", 0, clox.std.lang.String, clox.std.net.URL);
    ASSERT_ARG_TYPE("HTTPClient::put(url, data)", 1, Dictionary);
    ObjString* url = httpRawURL(vm, args[0]);
    ObjDictionary* data = AS_DICTIONARY(args[1]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a PATCH request using CURL.");
    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_PATCH, data, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete a PATCH request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, post) {
    ASSERT_ARG_COUNT("HTTPClient::post(url, data)", 2);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::post(url, data)", 0, clox.std.lang.String, clox.std.net.URL);
    ASSERT_ARG_TYPE("HTTPClient::post(url, data)", 1, Dictionary);
    ObjString* url = httpRawURL(vm, args[0]);
    ObjDictionary* data = AS_DICTIONARY(args[1]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a POST request using CURL.");
    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_POST, data, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete a POST request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, put) {
    ASSERT_ARG_COUNT("HTTPClient::put(url, data)", 2);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPClient::put(url, data)", 0, clox.std.lang.String, clox.std.net.URL);
    ASSERT_ARG_TYPE("HTTPClient::put(url, data)", 1, Dictionary);
    ObjString* url = httpRawURL(vm, args[0]);
    ObjDictionary* data = AS_DICTIONARY(args[1]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate a PUT request using CURL.");
    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_PUT, data, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete a PUT request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, send) {
    ASSERT_ARG_COUNT("HTTPClient::send(request)", 1);
    ASSERT_ARG_INSTANCE_OF("HTTPClient::send(request)", 0, clox.std.net.HTTPRequest);
    CURL* curl = curl_easy_init();
    if (curl == NULL) THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to initiate an HTTP request using CURL.");

    ObjInstance* request = AS_INSTANCE(args[0]);
    ObjString* url = AS_STRING(getObjProperty(vm, request, "url"));
    HTTPMethod method = (HTTPMethod)AS_INT(getObjProperty(vm, request, "method"));
    ObjDictionary* headers = AS_DICTIONARY(getObjProperty(vm, request, "headers"));
    ObjDictionary* data = AS_DICTIONARY(getObjProperty(vm, request, "data"));

    struct curl_slist* curlHeaders = httpParseHeaders(vm, headers, curl);
    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, method, data, curl, &curlResponse);
    curl_slist_free_all(curlHeaders);
    if (curlCode != CURLE_OK) {
        curl_easy_cleanup(curl);
        THROW_EXCEPTION(clox.std.net.HTTPException, "Failed to complete an HTTP request from URL.");
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPRequest, __init__) {
    ASSERT_ARG_COUNT("HTTPRequest::__init__(url, method, headers, data)", 4);
    ASSERT_ARG_INSTANCE_OF_ANY("HTTPRequest::__init__(url, method, headers, data)", 0, clox.std.lang.String, clox.std.net.URL);
    ASSERT_ARG_TYPE("HTTPRequest::__init__(url, method, headers, data)", 1, Int);
    ASSERT_ARG_TYPE("HTTPRequest::__init__(url, method, headers, data)", 2, Dictionary);
    ASSERT_ARG_TYPE("HTTPRequest::__init__(url, method, headers, data)", 3, Dictionary);

    ObjInstance* self = AS_INSTANCE(receiver);
    Value rawURL = OBJ_VAL(httpRawURL(vm, args[0]));
    setObjProperty(vm, self, "url", rawURL);
    setObjProperty(vm, self, "method", args[1]);
    setObjProperty(vm, self, "headers", args[2]);
    setObjProperty(vm, self, "data", args[3]);
    RETURN_OBJ(self);
}

LOX_METHOD(HTTPRequest, toString) {
    ASSERT_ARG_COUNT("HTTPRequest::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* url = AS_STRING(getObjProperty(vm, self, "url"));
    HTTPMethod method = (HTTPMethod)AS_INT(getObjProperty(vm, self, "method"));
    ObjDictionary* data = AS_DICTIONARY(getObjProperty(vm, self, "data"));
    RETURN_STRING_FMT("HTTPRequest - URL: %s; Method: %s; Data: %s", url->chars, httpMapMethod(method), httpParsePostData(vm, data)->chars);
}

LOX_METHOD(HTTPResponse, __init__) {
    ASSERT_ARG_COUNT("HTTPResponse::__init__(url, status, headers, contentType)", 4);
    ASSERT_ARG_TYPE("HTTPResponse::__init__(url, status, headers, contentType)", 0, String);
    ASSERT_ARG_TYPE("HTTPResponse::__init__(url, status, headers, contentType)", 1, Int);
    ASSERT_ARG_TYPE("HTTPResponse::__init__(url, status, headers, contentType)", 2, Dictionary);
    ASSERT_ARG_TYPE("HTTPResponse::__init__(url, status, headers, contentType)", 3, String);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "url", args[0]);
    setObjProperty(vm, self, "status", args[1]);
    setObjProperty(vm, self, "headers", args[2]);
    setObjProperty(vm, self, "contentType", args[3]);
    RETURN_OBJ(self);
}

LOX_METHOD(HTTPResponse, toString) {
    ASSERT_ARG_COUNT("HTTPResponse::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* url = AS_STRING(getObjProperty(vm, self, "url"));
    int status = AS_INT(getObjProperty(vm, self, "status"));
    ObjString* contentType = AS_STRING(getObjProperty(vm, self, "contentType"));
    RETURN_STRING_FMT("HTTPResponse - URL: %s; Status: %d; ContentType: %s", url->chars, status, contentType->chars);
}

LOX_METHOD(IPAddress, __init__) {
    ASSERT_ARG_COUNT("IPAddress::__init__(address)", 1);
    ASSERT_ARG_TYPE("IPAddress::__init__(address)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* address = AS_STRING(args[0]);
    int version = -1;

    if (ipIsV4(address)) version = 4;
    else if (ipIsV6(address)) version = 6;
    else THROW_EXCEPTION(clox.std.net.IPAddressException, "Invalid IP address specified.");

    setObjProperty(vm, self, "address", args[0]);
    setObjProperty(vm, self, "version", INT_VAL(version));
    RETURN_OBJ(self);
}

LOX_METHOD(IPAddress, getDomain) {
    ASSERT_ARG_COUNT("IPAddress::getDomain()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* address = AS_STRING(getObjProperty(vm, self, "address"));
    int status = -1;
    ObjString* domain = dnsGetDomainFromIPAddress(vm, address->chars, &status);
    if (status) THROW_EXCEPTION(clox.std.net.DomainHostException, "Failed to get domain information for IP Address.");
    RETURN_OBJ(domain);
}

LOX_METHOD(IPAddress, getDomainAsync) {
    ASSERT_ARG_COUNT("IPAddress::getDomainAsync()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjPromise* promise = dnsGetDomainFromIPAddressAsync(vm, self, dnsOnGetNameInfo);
    if (promise == NULL) RETURN_PROMISE_EX(clox.std.net.IPAddressException, "Failed to get domain name from IP Address.");
    RETURN_OBJ(promise);
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

LOX_METHOD(Socket, __init__) {
    ASSERT_ARG_COUNT("Socket::__init__(addressFamily, socketType, protocolType)", 3);
    ASSERT_ARG_TYPE("Socket::__init__(addressFamily, socketType, protocolType)", 0, Int);
    ASSERT_ARG_TYPE("Socket::__init__(addressFamily, socketType, protocolType)", 1, Int);
    ASSERT_ARG_TYPE("Socket::__init__(addressFamily, socketType, protocolType)", 2, Int);

    SOCKET descriptor = socket(AS_INT(args[0]), AS_INT(args[1]), AS_INT(args[2]));
    if (descriptor == INVALID_SOCKET) THROW_EXCEPTION(clox.std.net.SocketException, "Socket creation failed...");
    ObjInstance* self = AS_INSTANCE(receiver);

    setObjProperty(vm, self, "addressFamily", args[0]);
    setObjProperty(vm, self, "socketType", args[1]);
    setObjProperty(vm, self, "protocolType", args[2]);
    setObjProperty(vm, self, "descriptor", INT_VAL(descriptor));
    RETURN_OBJ(self);
}

LOX_METHOD(Socket, close) {
    ASSERT_ARG_COUNT("Socket::close()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    closesocket(descriptor);
    RETURN_NIL;
}

LOX_METHOD(Socket, receive) {
    ASSERT_ARG_COUNT("Socket::receive()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    char message[UINT8_MAX] = "";
    if (recv(descriptor, message, UINT8_MAX, 0) < 0) THROW_EXCEPTION(clox.std.net.SocketException, "Failed to receive message from socket.");
    RETURN_STRING(message, (int)strlen(message));
}

LOX_METHOD(Socket, send) {
    ASSERT_ARG_COUNT("Socket::send(message)", 1);
    ASSERT_ARG_TYPE("Socket::send(message)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* message = AS_STRING(args[0]);

    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    if (send(descriptor, message->chars, message->length, 0) < 0) {
        THROW_EXCEPTION(clox.std.net.SocketException, "Failed to send message to socket.");
    }
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

LOX_METHOD(SocketAddress, __init__) {
    ASSERT_ARG_COUNT("SocketAddress::__init__(address, family, port)", 3);
    ASSERT_ARG_TYPE("SocketAddress::__init__(address, family, port)", 0, String);
    ASSERT_ARG_TYPE("SocketAddress::__init__(address, family, port)", 1, Int);
    ASSERT_ARG_TYPE("SocketAddress::__init__(address, family, port)", 2, Int);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "address", args[0]);
    setObjProperty(vm, self, "family", args[1]);
    setObjProperty(vm, self, "port", args[2]);
    RETURN_OBJ(self);
}

LOX_METHOD(SocketAddress, ipAddress) {
    ASSERT_ARG_COUNT("SocketAddress::ipAddress()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value address = getObjProperty(vm, self, "address");

    ObjInstance* ipAddress = newInstance(vm, getNativeClass(vm, "clox.std.net.IPAddress"));
    push(vm, OBJ_VAL(ipAddress));
    copyObjProperty(vm, self, ipAddress, "address");
    pop(vm);
    RETURN_OBJ(ipAddress);
}

LOX_METHOD(SocketAddress, toString) {
    ASSERT_ARG_COUNT("SocketAddress::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    char* address = AS_CSTRING(getObjProperty(vm, self, "address"));
    int port = AS_INT(getObjProperty(vm, self, "port"));
    RETURN_STRING_FMT("%s:%d", address, port);
}

LOX_METHOD(SocketClient, connect) {
    ASSERT_ARG_COUNT("SocketClient::connect(socketAddress)", 1);
    ASSERT_ARG_INSTANCE_OF("SocketClient::connect(socketAddress)", 0, clox.std.net.SocketAddress);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* socketAddress = AS_INSTANCE(args[0]);

    struct sockaddr_in sockAddr = { 0 };
    ObjString* ipAddress = AS_STRING(getObjProperty(vm, socketAddress, "address"));
    int addressFamily = AS_INT(getObjProperty(vm, socketAddress, "family"));
    if (inet_pton(addressFamily, ipAddress->chars, &sockAddr.sin_addr) <= 0) {
        THROW_EXCEPTION(clox.std.net.SocketException, "Invalid socket address provided.");
    }

    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    if (connect(descriptor, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) <= 0) {
        THROW_EXCEPTION(clox.std.net.SocketException, "Socket connection failed.");
    }
    RETURN_NIL;
}

LOX_METHOD(SocketServer, accept) {
    ASSERT_ARG_COUNT("SocketServer::accept()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));

    struct sockaddr_in socketAddress = { 0 };
    int clientSize = sizeof(socketAddress);
    if (accept(descriptor, (struct sockaddr*)&socketAddress, &clientSize)) {
        THROW_EXCEPTION(clox.std.net.SocketException, "Failed to accept client connection.");
    }
    char ipAddress[UINT8_MAX];
    inet_ntop(socketAddress.sin_family, (struct sockaddr*)&socketAddress, ipAddress, UINT8_MAX);

    ObjInstance* clientAddress = newInstance(vm, getNativeClass(vm, "clox.std.net.SocketAddress"));
    push(vm, OBJ_VAL(clientAddress));
    setObjProperty(vm, clientAddress, "address", OBJ_VAL(newString(vm, ipAddress)));
    setObjProperty(vm, clientAddress, "family", INT_VAL(socketAddress.sin_family));
    setObjProperty(vm, clientAddress, "port", INT_VAL(socketAddress.sin_port));
    pop(vm);
    RETURN_OBJ(clientAddress);
}

LOX_METHOD(SocketServer, bind) {
    ASSERT_ARG_COUNT("SocketServer::bind(serverAddress)", 1);
    ASSERT_ARG_INSTANCE_OF("socketServer::bind(serverAddress)", 0, clox.std.net.SocketAddress);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* serverAddress = AS_INSTANCE(args[0]);

    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    ObjString* ipAddress = AS_STRING(getObjProperty(vm, serverAddress, "address"));
    int addressFamily = AS_INT(getObjProperty(vm, serverAddress, "family"));
    int port = AS_INT(getObjProperty(vm, serverAddress, "port"));

    struct sockaddr_in socketAddress = {
        .sin_family = addressFamily,
        .sin_port = htons(port)
    };

    if (inet_pton(addressFamily, ipAddress->chars, &socketAddress.sin_addr) <= 0) {
        THROW_EXCEPTION(clox.std.net.SocketException, "Invalid socket address provided.");
    }
    else if (bind(descriptor, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
        THROW_EXCEPTION(clox.std.net.SocketException, "Failed to bind to port on socket address.");
    }
    RETURN_NIL;
}

LOX_METHOD(SocketServer, listen) {
    ASSERT_ARG_COUNT("SocketServer::listen()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    if (listen(descriptor, 1) < 0) THROW_EXCEPTION(clox.std.net.SocketException, "Failed to listen for incoming connections.");
    RETURN_NIL;
}

LOX_METHOD(URL, __init__) {
    ASSERT_ARG_COUNT("URL::__init__(scheme, host, port, path, query, fragment)", 6);
    ASSERT_ARG_TYPE("URL::__init__(scheme, host, port, path, query, fragment)", 0, String);
    ASSERT_ARG_TYPE("URL::__init__(scheme, host, port, path, query, fragment)", 1, String);
    ASSERT_ARG_TYPE("URL::__init__(scheme, host, port, path, query, fragment)", 2, Int);
    ASSERT_ARG_TYPE("URL::__init__(scheme, host, port, path, query, fragment)", 3, String);
    ASSERT_ARG_TYPE("URL::__init__(scheme, host, port, path, query, fragment)", 4, String);
    ASSERT_ARG_TYPE("URL::__init__(scheme, host, port, path, query, fragment)", 5, String);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "scheme", args[0]);
    setObjProperty(vm, self, "host", args[1]);
    setObjProperty(vm, self, "port", args[2]);
    setObjProperty(vm, self, "path", args[3]);
    setObjProperty(vm, self, "query", args[4]);
    setObjProperty(vm, self, "fragment", args[5]);
    setObjProperty(vm, self, "raw", OBJ_VAL(urlToString(vm, self)));
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
        if (length == -1) THROW_EXCEPTION(clox.std.net.URLException, "Failed to parse path from URL.");
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
        if (length == -1) THROW_EXCEPTION(clox.std.net.URLException, "Failed to parse query parameters from URL.");
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
    ASSERT_ARG_INSTANCE_OF("URL::relativize(url)", 0, clox.std.net.URL);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* url = AS_INSTANCE(args[0]);
    if (urlIsAbsolute(vm, self) || urlIsAbsolute(vm, url)) RETURN_OBJ(url);
    ObjString* urlString = AS_STRING(getObjProperty(vm, self, "raw"));
    ObjString* urlString2 = urlToString(vm, url);
    int index = searchString(vm, urlString, urlString2, 0);

    if (index == 0) {
        ObjInstance* relativized = newInstance(vm, self->obj.klass);
        ObjString* relativizedURL = subString(vm, urlString, urlString2->length, urlString->length); 
        struct yuarel component;
        char fullURL[UINT8_MAX];
        int length = sprintf_s(fullURL, UINT8_MAX, "%s%s", "https://example.com/", relativizedURL->chars);
        yuarel_parse(&component, fullURL);

        setObjProperty(vm, relativized, "scheme", OBJ_VAL(emptyString(vm)));
        setObjProperty(vm, relativized, "host", OBJ_VAL(emptyString(vm)));
        setObjProperty(vm, relativized, "port", INT_VAL(0));
        setObjProperty(vm, relativized, "path", OBJ_VAL(newString(vm, component.path != NULL ? component.path : "")));
        setObjProperty(vm, relativized, "query", OBJ_VAL(newString(vm, component.query != NULL ? component.query : "")));
        setObjProperty(vm, relativized, "fragment", OBJ_VAL(newString(vm, component.fragment != NULL ? component.fragment : "")));
        setObjProperty(vm, relativized, "raw", OBJ_VAL(relativizedURL));
        RETURN_OBJ(relativized);
    }
    RETURN_OBJ(url);
}

LOX_METHOD(URL, toString) {
    ASSERT_ARG_COUNT("URL::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjString* raw = AS_STRING(getObjProperty(vm, self, "raw"));
    RETURN_OBJ(raw);
}

LOX_METHOD(URLClass, parse) {
    ASSERT_ARG_COUNT("URL class::parse(url)", 1);
    ASSERT_ARG_TYPE("URL class::parse(url)", 0, String);
    ObjInstance* instance = newInstance(vm, AS_CLASS(receiver));
    push(vm, OBJ_VAL(instance));
    ObjString* url = AS_STRING(args[0]);
    struct yuarel component;
    if (yuarel_parse(&component, url->chars) == -1) THROW_EXCEPTION(clox.std.net.URLException, "Failed to parse the supplied url.");

    setObjProperty(vm, instance, "scheme", OBJ_VAL(newString(vm, component.scheme != NULL ? component.scheme : "")));
    setObjProperty(vm, instance, "host", OBJ_VAL(newString(vm, component.host != NULL ? component.host : "")));
    setObjProperty(vm, instance, "port", INT_VAL(component.port));
    setObjProperty(vm, instance, "path", OBJ_VAL(newString(vm, component.path != NULL ? component.path : "")));
    setObjProperty(vm, instance, "query", OBJ_VAL(newString(vm, component.query != NULL ? component.query : "")));
    setObjProperty(vm, instance, "fragment", OBJ_VAL(newString(vm, component.fragment != NULL ? component.fragment : "")));
    setObjProperty(vm, instance, "raw", OBJ_VAL(urlToString(vm, instance)));
    pop(vm);
    RETURN_OBJ(instance);
}

void registerNetPackage(VM* vm) {
    ObjNamespace* netNamespace = defineNativeNamespace(vm, "net", vm->stdNamespace);
    vm->currentNamespace = netNamespace;

    ObjClass* urlClass = defineNativeClass(vm, "URL");
    bindSuperclass(vm, urlClass, vm->objectClass);
    DEF_INTERCEPTOR(urlClass, URL, INTERCEPTOR_INIT, __init__, 6);
    DEF_METHOD(urlClass, URL, isAbsolute, 0);
    DEF_METHOD(urlClass, URL, isRelative, 0);
    DEF_METHOD(urlClass, URL, pathArray, 0);
    DEF_METHOD(urlClass, URL, queryDict, 0);
    DEF_METHOD(urlClass, URL, relativize, 1);
    DEF_METHOD(urlClass, URL, toString, 0);

    ObjClass* urlMetaclass = urlClass->obj.klass;
    DEF_METHOD(urlMetaclass, URLClass, parse, 1);

    ObjClass* domainClass = defineNativeClass(vm, "Domain");
    bindSuperclass(vm, domainClass, vm->objectClass);
    DEF_INTERCEPTOR(domainClass, Domain, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(domainClass, Domain, getIPAddresses, 0);
    DEF_METHOD_ASYNC(domainClass, Domain, getIPAddressesAsync, 0);
    DEF_METHOD(domainClass, Domain, toString, 0);

    ObjClass* ipAddressClass = defineNativeClass(vm, "IPAddress");
    bindSuperclass(vm, ipAddressClass, vm->objectClass);
    DEF_INTERCEPTOR(ipAddressClass, IPAddress, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(ipAddressClass, IPAddress, getDomain, 0);
    DEF_METHOD_ASYNC(ipAddressClass, IPAddress, getDomainAsync, 0);
    DEF_METHOD(ipAddressClass, IPAddress, isIPV4, 0);
    DEF_METHOD(ipAddressClass, IPAddress, isIPV6, 0);
    DEF_METHOD(ipAddressClass, IPAddress, toArray, 0);
    DEF_METHOD(ipAddressClass, IPAddress, toString, 0);

    ObjClass* socketAddressClass = defineNativeClass(vm, "SocketAddress");
    bindSuperclass(vm, socketAddressClass, vm->objectClass);
    DEF_INTERCEPTOR(socketAddressClass, SocketAddress, INTERCEPTOR_INIT, __init__, 3);
    DEF_METHOD(socketAddressClass, SocketAddress, ipAddress, 0);
    DEF_METHOD(socketAddressClass, SocketAddress, toString, 0);

    ObjClass* closableTrait = getNativeClass(vm, "clox.std.io.TClosable");
    ObjClass* socketClass = defineNativeClass(vm, "Socket");
    bindSuperclass(vm, socketClass, vm->objectClass);
    bindTrait(vm, socketClass, closableTrait);
    DEF_INTERCEPTOR(socketClass, Socket, INTERCEPTOR_INIT, __init__, 3);
    DEF_METHOD(socketClass, Socket, close, 0);
    DEF_METHOD(socketClass, Socket, receive, 0);
    DEF_METHOD(socketClass, Socket, send, 1);
    DEF_METHOD(socketClass, Socket, toString, 0);

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

    ObjClass* socketClientClass = defineNativeClass(vm, "SocketClient");
    bindSuperclass(vm, socketClientClass, socketClass);
    DEF_METHOD(socketClientClass, SocketClient, connect, 1);

    ObjClass* socketServerClass = defineNativeClass(vm, "SocketServer");
    bindSuperclass(vm, socketServerClass, socketClass);
    DEF_METHOD(socketServerClass, SocketServer, accept, 1);
    DEF_METHOD(socketServerClass, SocketServer, bind, 1);
    DEF_METHOD(socketServerClass, SocketServer, listen, 0);

    ObjClass* httpClientClass = defineNativeClass(vm, "HTTPClient");
    bindSuperclass(vm, httpClientClass, vm->objectClass);
    bindTrait(vm, httpClientClass, closableTrait);
    DEF_INTERCEPTOR(httpClientClass, HTTPClient, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(httpClientClass, HTTPClient, close, 0);
    DEF_METHOD(httpClientClass, HTTPClient, delete, 1);
    DEF_METHOD(httpClientClass, HTTPClient, download, 2);
    DEF_METHOD(httpClientClass, HTTPClient, get, 1);
    DEF_METHOD(httpClientClass, HTTPClient, head, 1);
    DEF_METHOD(httpClientClass, HTTPClient, options, 1);
    DEF_METHOD(httpClientClass, HTTPClient, patch, 2);
    DEF_METHOD(httpClientClass, HTTPClient, post, 2);
    DEF_METHOD(httpClientClass, HTTPClient, put, 2);
    DEF_METHOD(httpClientClass, HTTPClient, send, 1);

    ObjClass* httpRequestClass = defineNativeClass(vm, "HTTPRequest");
    bindSuperclass(vm, httpRequestClass, vm->objectClass);
    DEF_INTERCEPTOR(httpRequestClass, HTTPRequest, INTERCEPTOR_INIT, __init__, 4);
    DEF_METHOD(httpRequestClass, HTTPRequest, toString, 0);

    setClassProperty(vm, httpRequestClass, "httpHEAD", INT_VAL(HTTP_HEAD));
    setClassProperty(vm, httpRequestClass, "httpGET", INT_VAL(HTTP_GET));
    setClassProperty(vm, httpRequestClass, "httpPOST", INT_VAL(HTTP_POST));
    setClassProperty(vm, httpRequestClass, "httpPUT", INT_VAL(HTTP_PUT));
    setClassProperty(vm, httpRequestClass, "httpDELETE", INT_VAL(HTTP_DELETE));
    setClassProperty(vm, httpRequestClass, "httpPATCH", INT_VAL(HTTP_PATCH));
    setClassProperty(vm, httpRequestClass, "httpOPTIONS", INT_VAL(HTTP_OPTIONS));
    setClassProperty(vm, httpRequestClass, "httpTRACE", INT_VAL(HTTP_TRACE));
    setClassProperty(vm, httpRequestClass, "httpCONNECT", INT_VAL(HTTP_CONNECT));
    setClassProperty(vm, httpRequestClass, "httpQUERY", INT_VAL(HTTP_QUERY));

    ObjClass* httpResponseClass = defineNativeClass(vm, "HTTPResponse");
    bindSuperclass(vm, httpResponseClass, vm->objectClass);
    DEF_INTERCEPTOR(httpResponseClass, HTTPResponse, INTERCEPTOR_INIT, __init__, 4);
    DEF_METHOD(httpResponseClass, HTTPResponse, toString, 0);

    setClassProperty(vm, httpResponseClass, "statusOK", INT_VAL(200));
    setClassProperty(vm, httpResponseClass, "statusFound", INT_VAL(302));
    setClassProperty(vm, httpResponseClass, "statusBadRequest", INT_VAL(400));
    setClassProperty(vm, httpResponseClass, "statusUnauthorized", INT_VAL(401));
    setClassProperty(vm, httpResponseClass, "statusForbidden", INT_VAL(403));
    setClassProperty(vm, httpResponseClass, "statusNotFound", INT_VAL(404));
    setClassProperty(vm, httpResponseClass, "statusMethodNotAllowed", INT_VAL(405));
    setClassProperty(vm, httpResponseClass, "statusInternalServerError", INT_VAL(500));
    setClassProperty(vm, httpResponseClass, "statusBadGateway", INT_VAL(502));
    setClassProperty(vm, httpResponseClass, "statusServiceUnavailable", INT_VAL(503));

    ObjClass* networkExceptionClass = defineNativeException(vm, "NetworkException", vm->exceptionClass);
    defineNativeException(vm, "DomainHostException", networkExceptionClass);
    defineNativeException(vm, "HTTPException", networkExceptionClass);
    defineNativeException(vm, "IPAddressException", networkExceptionClass);
    defineNativeException(vm, "SocketException", networkExceptionClass);
    defineNativeException(vm, "URLException", networkExceptionClass);
    vm->currentNamespace = vm->rootNamespace;
}