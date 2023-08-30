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
    char* headers;
    char* content;
    size_t hSize;
    size_t cSize;
} CURLResponse;

typedef enum HTTPMethod {
    HTTP_HEAD, 
    HTTP_GET, 
    HTTP_POST, 
    HTTP_PUT, 
    HTTP_DELETE, 
    HTTP_PATCH, 
    HTTP_OPTIONS, 
    HTTP_TRACE,
    HTTP_CONNECT,
    HTTP_QUERY
} HTTPMethod;

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

static ObjArray* httpCreateCookies(VM* vm, CURL* curl) {
    struct curl_slist* cookies = NULL;
    CURLcode curlCode = curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies);
    ObjArray* cookieArray = newArray(vm);

    if (curlCode == CURLE_OK) {
        push(vm, OBJ_VAL(cookieArray));
        struct curl_slist* cookieNode = cookies;

        while (cookieNode) {
            valueArrayWrite(vm, &cookieArray->elements, OBJ_VAL(newString(vm, cookieNode->data)));
            cookieNode = cookieNode->next;
        }

        curl_slist_free_all(cookies);
        pop(vm);
    }
    return cookieArray;
}

static ObjArray* httpCreateHeaders(VM* vm, CURLResponse curlResponse) {
    ObjString* headerString = copyString(vm, curlResponse.headers, (int)curlResponse.hSize);
    ObjArray* headers = newArray(vm);
    int startIndex = 0;
    push(vm, OBJ_VAL(headers));

    for (int i = 0; i < curlResponse.hSize - 1; i++) {
        if (curlResponse.headers[i] == '\n') {
            valueArrayWrite(vm, &headers->elements, OBJ_VAL(subString(vm, headerString, startIndex, i - 1)));
            startIndex = i + 1;
        }
    }
    pop(vm);
    return headers;
}

static ObjInstance* httpCreateResponse(VM* vm, ObjString* url, CURL* curl, CURLResponse curlResponse) {
    long statusCode;
    char* contentType;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);

    ObjInstance* httpResponse = newInstance(vm, getNativeClass(vm, "clox.std.network", "HTTPResponse"));
    push(vm, OBJ_VAL(httpResponse));
    setObjProperty(vm, httpResponse, "content", OBJ_VAL(copyString(vm, curlResponse.content, (int)curlResponse.cSize)));
    setObjProperty(vm, httpResponse, "contentType", OBJ_VAL(newString(vm, contentType)));
    setObjProperty(vm, httpResponse, "cookies", OBJ_VAL(httpCreateCookies(vm, curl)));
    setObjProperty(vm, httpResponse, "headers", OBJ_VAL(httpCreateHeaders(vm, curlResponse)));
    setObjProperty(vm, httpResponse, "status", INT_VAL(statusCode));
    setObjProperty(vm, httpResponse, "url", OBJ_VAL(url));
    pop(vm);
    return httpResponse;
}

static size_t httpCURLHeaders(void* headers, size_t size, size_t nitems, void* userdata) {
    size_t realsize = size * nitems;
    if (nitems != 2) {
        CURLResponse* curlResponse = (CURLResponse*)userdata;
        char* ptr = realloc(curlResponse->headers, curlResponse->hSize + realsize + 1);
        if (!ptr) return 0;

        curlResponse->headers = ptr;
        memcpy(&(curlResponse->headers[curlResponse->hSize]), headers, realsize);
        curlResponse->hSize += realsize;
        curlResponse->headers[curlResponse->hSize] = 0;
    }
    return realsize;
}

static size_t httpCURLResponse(void* contents, size_t size, size_t nmemb, void* userdata) {
    size_t realsize = size * nmemb;
    CURLResponse* curlResponse = (CURLResponse*)userdata;
    char* ptr = realloc(curlResponse->content, curlResponse->cSize + realsize + 1);
    if (!ptr) return 0;

    curlResponse->content = ptr;
    memcpy(&(curlResponse->content[curlResponse->cSize]), contents, realsize);
    curlResponse->cSize += realsize;
    curlResponse->content[curlResponse->cSize] = 0;
    return realsize;
}

static char* httpMapMethod(HTTPMethod method) {
    switch (method) {
        case HTTP_GET: return "GET";
        case HTTP_POST: return "POST";
        case HTTP_PUT: return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH: return "PATCH";
        case HTTP_OPTIONS: return "OPTIONS";
        case HTTP_TRACE: return "TRACE";
        case HTTP_CONNECT: return "CONNECT";
        case HTTP_QUERY: return "QUERY";
        default: return "HEAD";
    }
}

static struct curl_slist* httpParseHeaders(VM* vm, ObjDictionary* headers, CURL* curl) {
    struct curl_slist* headerList = NULL;
    for (int i = 0; i < headers->capacity; i++) {
        ObjEntry* entry = &headers->entries[i];
        if (!IS_STRING(entry->key) || !IS_STRING(entry->value)) continue;

        char header[UINT8_MAX] = "";
        sprintf_s(header, UINT8_MAX, "%s:%s", AS_STRING(entry->key)->chars, AS_STRING(entry->value)->chars);
        headerList = curl_slist_append(headerList, header);
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    return headerList;
}

static ObjString* httpParsePostData(VM* vm, ObjDictionary* postData) {
    if (postData->count == 0) return emptyString(vm);
    else {
        char string[UINT8_MAX] = "";
        size_t offset = 0;
        int startIndex = 0;

        for (int i = 0; i < postData->capacity; i++) {
            ObjEntry* entry = &postData->entries[i];
            if (IS_UNDEFINED(entry->key)) continue;
            Value key = entry->key;
            char* keyChars = valueToString(vm, key);
            size_t keyLength = strlen(keyChars);
            Value value = entry->value;
            char* valueChars = valueToString(vm, value);
            size_t valueLength = strlen(valueChars);

            memcpy(string + offset, keyChars, keyLength);
            offset += keyLength;
            memcpy(string + offset, "=", 1);
            offset += 1;
            memcpy(string + offset, valueChars, valueLength);
            offset += valueLength;
            startIndex = i + 1;
            break;
        }

        for (int i = startIndex; i < postData->capacity; i++) {
            ObjEntry* entry = &postData->entries[i];
            if (IS_UNDEFINED(entry->key)) continue;
            Value key = entry->key;
            char* keyChars = valueToString(vm, key);
            size_t keyLength = strlen(keyChars);
            Value value = entry->value;
            char* valueChars = valueToString(vm, value);
            size_t valueLength = strlen(valueChars);

            memcpy(string + offset, "&", 1);
            offset += 1;
            memcpy(string + offset, keyChars, keyLength);
            offset += keyLength;
            memcpy(string + offset, "=", 1);
            offset += 1;
            memcpy(string + offset, valueChars, valueLength);
            offset += valueLength;
        }

        string[offset] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
}

static ObjString* httpRawURL(VM* vm, Value value) {
    if (IS_INSTANCE(value)) {
        ObjInstance* url = AS_INSTANCE(value);
        return AS_STRING(getObjProperty(vm, url, "raw"));
    }
    else return AS_STRING(value);
}

static CURLcode httpSendRequest(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* data, CURL* curl, CURLResponse* curlResponse) {
    curlResponse->headers = malloc(0);
    curlResponse->content = malloc(0);
    curlResponse->hSize = 0;
    curlResponse->cSize = 0;

    char* urlChars = url->chars;
    curl_easy_setopt(curl, CURLOPT_URL, urlChars);
    if (method > HTTP_POST) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, httpMapMethod(method));
    }
 
    if (method == HTTP_HEAD) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    else if (method == HTTP_POST || method == HTTP_PUT || method == HTTP_PATCH) {
        char* dataChars = httpParsePostData(vm, data)->chars;
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dataChars);
    }
    else if (method == HTTP_OPTIONS) {
        curl_easy_setopt(curl, CURLOPT_REQUEST_TARGET, "*");
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpCURLResponse);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)curlResponse);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, httpCURLHeaders);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void*)curlResponse);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    return curl_easy_perform(curl);
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

LOX_METHOD(HTTPClient, delete) {
    ASSERT_ARG_COUNT("HTTPClient::delete(url)", 1);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::delete(url)", 0, clox.std.lang, String, clox.std.network, URL);
    ObjString* url = httpRawURL(vm, args[0]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a DELETE request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_DELETE, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a DELETE request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, get) {
    ASSERT_ARG_COUNT("HTTPClient::get(url)", 1);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::get(url)", 0, clox.std.lang, String, clox.std.network, URL);
    ObjString* url = httpRawURL(vm, args[0]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a GET request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_GET, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a GET request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, head) {
    ASSERT_ARG_COUNT("HTTPClient::head(url)", 1);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::head(url)", 0, clox.std.lang, String, clox.std.network, URL);
    ObjString* url = httpRawURL(vm, args[0]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a HEAD request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_HEAD, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a HEAD request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, init) {
    ASSERT_ARG_COUNT("HTTPClient::init()", 0);
    curl_global_init(CURL_GLOBAL_ALL);
    RETURN_VAL(receiver);
}

LOX_METHOD(HTTPClient, options) {
    ASSERT_ARG_COUNT("HTTPClient::options(url)", 1);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::options(url)", 0, clox.std.lang, String, clox.std.network, URL);
    ObjString* url = httpRawURL(vm, args[0]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate an OPTIONS request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_OPTIONS, NULL, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete an OPTIONS request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, patch) {
    ASSERT_ARG_COUNT("HTTPClient::put(url, data)", 2);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::put(url, data)", 0, clox.std.lang, String, clox.std.network, URL);
    ASSERT_ARG_TYPE("HTTPClient::put(url, data)", 1, Dictionary);
    ObjString* url = httpRawURL(vm, args[0]);
    ObjDictionary* data = AS_DICTIONARY(args[1]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a PATCH request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_PATCH, data, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a PATCH request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, post) {
    ASSERT_ARG_COUNT("HTTPClient::post(url, data)", 2);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::post(url, data)", 0, clox.std.lang, String, clox.std.network, URL);
    ASSERT_ARG_TYPE("HTTPClient::post(url, data)", 1, Dictionary);
    ObjString* url = httpRawURL(vm, args[0]);
    ObjDictionary* data = AS_DICTIONARY(args[1]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a POST request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_POST, data, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a POST request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, put) {
    ASSERT_ARG_COUNT("HTTPClient::put(url, data)", 2);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPClient::put(url, data)", 0, clox.std.lang, String, clox.std.network, URL);
    ASSERT_ARG_TYPE("HTTPClient::put(url, data)", 1, Dictionary);
    ObjString* url = httpRawURL(vm, args[0]);
    ObjDictionary* data = AS_DICTIONARY(args[1]);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a PUT request using CURL.");
        RETURN_NIL;
    }

    CURLResponse curlResponse;
    CURLcode curlCode = httpSendRequest(vm, url, HTTP_PUT, data, curl, &curlResponse);
    if (curlCode != CURLE_OK) {
        raiseError(vm, "Failed to complete a PUT request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPClient, send) {
    ASSERT_ARG_COUNT("HTTPClient::send(request)", 1);
    ASSERT_ARG_INSTANCE_OF("HTTPClient::send(request)", 0, clox.std.network, HTTPRequest);

    CURL* curl = curl_easy_init();
    if (curl == NULL) {
        raiseError(vm, "Failed to initiate a PUT request using CURL.");
        RETURN_NIL;
    }

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
        raiseError(vm, "Failed to complete a PUT request from URL.");
        curl_easy_cleanup(curl);
        RETURN_NIL;
    }

    ObjInstance* httpResponse = httpCreateResponse(vm, url, curl, curlResponse);
    curl_easy_cleanup(curl);
    RETURN_OBJ(httpResponse);
}

LOX_METHOD(HTTPRequest, init) {
    ASSERT_ARG_COUNT("HTTPRequest::init(url, method, headers, data)", 4);
    ASSERT_ARG_INSTANCE_OF_EITHER("HTTPRequest::init(url, method, headers, data)", 0, clox.std.lang, String, clox.std.network, URL);
    ASSERT_ARG_TYPE("HTTPRequest::init(url, method, headers, data)", 1, Int);
    ASSERT_ARG_TYPE("HTTPRequest::init(url, method, headers, data)", 2, Dictionary);
    ASSERT_ARG_TYPE("HTTPRequest::init(url, method, headers, data)", 3, Dictionary);

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

LOX_METHOD(HTTPResponse, init) {
    ASSERT_ARG_COUNT("HTTPResponse::init(url, status, headers, contentType)", 4);
    ASSERT_ARG_TYPE("HTTPResponse::init(url, status, headers, contentType)", 0, String);
    ASSERT_ARG_TYPE("HTTPResponse::init(url, status, headers, contentType)", 1, Int);
    ASSERT_ARG_TYPE("HTTPResponse::init(url, status, headers, contentType)", 2, Dictionary);
    ASSERT_ARG_TYPE("HTTPResponse::init(url, status, headers, contentType)", 3, String);

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

LOX_METHOD(SocketAddress, init) {
    ASSERT_ARG_COUNT("SocketAddress::init(address, family, port)", 3);
    ASSERT_ARG_TYPE("SocketAddress::init(address, family, port)", 0, String);
    ASSERT_ARG_TYPE("SocketAddress::init(address, family, port)", 1, Int);
    ASSERT_ARG_TYPE("SocketAddress::init(address, family, port)", 2, Int);

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

    ObjInstance* ipAddress = newInstance(vm, getNativeClass(vm, "clox.std.network", "IPAddress"));
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
    ASSERT_ARG_INSTANCE_OF("SocketClient::connect(socketAddress)", 0, clox.std.network, SocketAddress);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* socketAddress = AS_INSTANCE(args[0]);

    struct sockaddr_in sockAddr = { 0 };
    ObjString* ipAddress = AS_STRING(getObjProperty(vm, socketAddress, "address"));
    int addressFamily = AS_INT(getObjProperty(vm, socketAddress, "family"));
    if (inet_pton(addressFamily, ipAddress->chars, &sockAddr.sin_addr) <= 0) {
        raiseError(vm, "Invalid socket address provided.");
        RETURN_NIL;
    }

    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));
    if (connect(descriptor, (struct sockaddr*)&sockAddr, sizeof(sockAddr)) <= 0) {
        raiseError(vm, "Socket connection failed.");
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
        raiseError(vm, "Failed to accept client connection.");
        RETURN_NIL;
    }
    char ipAddress[UINT8_MAX];
    inet_ntop(socketAddress.sin_family, (struct sockaddr*)&socketAddress, ipAddress, UINT8_MAX);

    ObjInstance* clientAddress = newInstance(vm, getNativeClass(vm, "clox.std.network", "SocketAddress"));
    push(vm, OBJ_VAL(clientAddress));
    setObjProperty(vm, clientAddress, "address", OBJ_VAL(newString(vm, ipAddress)));
    setObjProperty(vm, clientAddress, "family", INT_VAL(socketAddress.sin_family));
    setObjProperty(vm, clientAddress, "port", INT_VAL(socketAddress.sin_port));
    pop(vm);
    RETURN_OBJ(clientAddress);
}

LOX_METHOD(SocketServer, bind) {
    ASSERT_ARG_COUNT("SocketServer::bind(serverAddress)", 1);
    ASSERT_ARG_INSTANCE_OF("socketServer::bind(serverAddress)", 0, clox.std.network, SocketAddress);
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
        raiseError(vm, "Invalid socket address provided.");
    }
    else if (bind(descriptor, (struct sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
        raiseError(vm, "Failed to bind to port on socket address.");
    }
    RETURN_NIL;
}

LOX_METHOD(SocketServer, listen) {
    ASSERT_ARG_COUNT("SocketServer::listen()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int descriptor = AS_INT(getObjProperty(vm, self, "descriptor"));

    if (listen(descriptor, 1) < 0) {
        raiseError(vm, "Failed to listen for incoming connections.");
    }
    RETURN_NIL;
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
    setObjProperty(vm, instance, "raw", OBJ_VAL(urlToString(vm, instance)));
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

    ObjClass* domainClass = defineNativeClass(vm, "Domain");
    bindSuperclass(vm, domainClass, vm->objectClass);
    DEF_METHOD(domainClass, Domain, init, 1);
    DEF_METHOD(domainClass, Domain, ipAddresses, 0);
    DEF_METHOD(domainClass, Domain, toString, 0);

    ObjClass* ipAddressClass = defineNativeClass(vm, "IPAddress");
    bindSuperclass(vm, ipAddressClass, vm->objectClass);
    DEF_METHOD(ipAddressClass, IPAddress, domain, 0);
    DEF_METHOD(ipAddressClass, IPAddress, init, 1);
    DEF_METHOD(ipAddressClass, IPAddress, isIPV4, 0);
    DEF_METHOD(ipAddressClass, IPAddress, isIPV6, 0);
    DEF_METHOD(ipAddressClass, IPAddress, toArray, 0);
    DEF_METHOD(ipAddressClass, IPAddress, toString, 0);

    ObjClass* socketAddressClass = defineNativeClass(vm, "SocketAddress");
    bindSuperclass(vm, socketAddressClass, vm->objectClass);
    DEF_METHOD(socketAddressClass, SocketAddress, init, 3);
    DEF_METHOD(socketAddressClass, SocketAddress, ipAddress, 0);
    DEF_METHOD(socketAddressClass, SocketAddress, toString, 0);

    ObjClass* closableTrait = getNativeClass(vm, "clox.std.io", "TClosable");
    ObjClass* socketClass = defineNativeClass(vm, "Socket");
    bindSuperclass(vm, socketClass, vm->objectClass);
    bindTrait(vm, socketClass, closableTrait);
    DEF_METHOD(socketClass, Socket, close, 0);
    DEF_METHOD(socketClass, Socket, init, 3);
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
    DEF_METHOD(httpClientClass, HTTPClient, close, 0);
    DEF_METHOD(httpClientClass, HTTPClient, delete, 1);
    DEF_METHOD(httpClientClass, HTTPClient, get, 1);
    DEF_METHOD(httpClientClass, HTTPClient, head, 1);
    DEF_METHOD(httpClientClass, HTTPClient, init, 0);
    DEF_METHOD(httpClientClass, HTTPClient, options, 1);
    DEF_METHOD(httpClientClass, HTTPClient, patch, 2);
    DEF_METHOD(httpClientClass, HTTPClient, post, 2);
    DEF_METHOD(httpClientClass, HTTPClient, put, 2);
    DEF_METHOD(httpClientClass, HTTPClient, send, 1);

    ObjClass* httpRequestClass = defineNativeClass(vm, "HTTPRequest");
    bindSuperclass(vm, httpRequestClass, vm->objectClass);
    DEF_METHOD(httpRequestClass, HTTPRequest, init, 4);
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
    DEF_METHOD(httpResponseClass, HTTPResponse, init, 4);
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

    vm->currentNamespace = vm->rootNamespace;
}