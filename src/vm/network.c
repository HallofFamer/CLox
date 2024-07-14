#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "native.h"
#include "network.h"
#include "object.h"
#include "os.h"
#include "vm.h"

static void httpCURLInfo(CURLM* curlM) {
    char* doneURL;
    CURLMsg* message;
    int pending;
    CURL* curl;
    FILE* file;

    while (message = curl_multi_info_read(curlM, &pending)) {
        switch (message->msg) {
            case CURLMSG_DONE:
                curl = message->easy_handle;
                curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &doneURL);
                curl_easy_getinfo(curl, CURLINFO_PRIVATE, &file);
                curl_multi_remove_handle(curlM, curl);
                curl_easy_cleanup(curl);
                if (file) fclose(file);
               break;
            default:
                break;
        }
    }
}

static void httpCURLPerform(uv_poll_t* poll, int status, int events) {
    int running;
    int flags = 0;
    CURLContext* context;

    if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
    if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;
    context = (CURLContext*)poll->data;
    curl_multi_socket_action(context->curlM, context->socket, flags, &running);
    httpCURLInfo(context->curlM);
}

static size_t httpCURLWriteFile(void* contents, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(contents, size, nmemb, stream);
}

static void httpCURLOnClose(uv_handle_t* handle) {
    CURLContext* context = (CURLContext*)handle->data;
    free(context);
}

static void httpCURLOnTimeout(uv_timer_t* timer) {
    int running;
    CURLContext* context = (CURLContext*)timer->data;
    curl_multi_socket_action(context->curlM, CURL_SOCKET_TIMEOUT, 0, &running);
    httpCURLInfo(context->curlM);
}

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

ObjArray* httpCreateCookies(VM* vm, CURL* curl) {
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

CURLContext* httpCreateContext(VM* vm, curl_socket_t socket) {
    CURLContext* context;
    context = ALLOCATE_STRUCT(CURLContext);
    if (context != NULL) {
        context->vm = vm;
        context->socket = socket;
        uv_poll_init_socket(vm->eventLoop, &context->poll, socket);
        context->poll.data = context;
    }
    return context;
}

ObjArray* httpCreateHeaders(VM* vm, CURLResponse curlResponse) {
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

ObjInstance* httpCreateResponse(VM* vm, ObjString* url, CURL* curl, CURLResponse curlResponse) {
    long statusCode;
    char* contentType;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
    ObjInstance* httpResponse = newInstance(vm, getNativeClass(vm, "clox.std.net.HTTPResponse"));
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

void httpCURLClose(CURLContext* context) {
    uv_close((uv_handle_t*)&context->poll, httpCURLOnClose);
}

size_t httpCURLHeaders(void* headers, size_t size, size_t nitems, void* userData) {
    size_t realsize = size * nitems;
    if (nitems != 2) {
        CURLResponse* curlResponse = (CURLResponse*)userData;
        char* ptr = realloc(curlResponse->headers, curlResponse->hSize + realsize + 1);
        if (!ptr) return 0;

        curlResponse->headers = ptr;
        memcpy(&(curlResponse->headers[curlResponse->hSize]), headers, realsize);
        curlResponse->hSize += realsize;
        curlResponse->headers[curlResponse->hSize] = 0;
    }
    return realsize;
}

int httpCURLPollSocket(CURL* curl, curl_socket_t socket, int action, void* userData, void* socketData) {
    CURLContext* context = (CURLContext*)socketData;
    int events = 0;

    switch (action) {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT:
            curl_multi_assign(context->curlM, socket, (void*)context);
            if (action != CURL_POLL_IN) events |= UV_WRITABLE;
            if (action != CURL_POLL_OUT) events |= UV_READABLE;
            uv_poll_start(&context->poll, events, httpCURLPerform);
            break;
        case CURL_POLL_REMOVE:
            if (socketData) {
                context = (CURLContext*)socketData;
                uv_poll_stop(&context->poll);
                httpCURLClose(context);
                curl_multi_assign(context->curlM, socket, NULL);
            }
            break;
        default:
            abort();
    }

    return 0;
}

size_t httpCURLResponse(void* contents, size_t size, size_t nmemb, void* userData) {
    size_t realsize = size * nmemb;
    CURLResponse* curlResponse = (CURLResponse*)userData;
    char* ptr = realloc(curlResponse->content, curlResponse->cSize + realsize + 1);
    if (!ptr) return 0;

    curlResponse->content = ptr;
    memcpy(&(curlResponse->content[curlResponse->cSize]), contents, realsize);
    curlResponse->cSize += realsize;
    curlResponse->content[curlResponse->cSize] = 0;
    return realsize;
}

int httpCURLStartTimeout(CURLM* curlM, long timeout, void* userData) {
    uv_timer_t* timer = (uv_timer_t*)userData;
    if (timeout < 0) uv_timer_stop(timer);
    else {
        if (timeout == 0) timeout = 1;
        uv_timer_start(timer, httpCURLOnTimeout, timeout, 0);
    }
    return 0;
}

CURLcode httpDownloadFile(VM* vm, ObjString* src, ObjString* dest, CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_URL, src->chars);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, httpCURLWriteFile);
    FILE* file;
    fopen_s(&file, dest->chars, "w");

    if (file != NULL) {
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        CURLcode code = curl_easy_perform(curl);
        fclose(file);
        return code;
    }
    return CURLE_FAILED_INIT;
}

struct curl_slist* httpParseHeaders(VM* vm, ObjDictionary* headers, CURL* curl) {
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

ObjString* httpParsePostData(VM* vm, ObjDictionary* postData) {
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

bool httpPrepareDownloadFile(VM* vm, ObjString* url, ObjString* dest, CURLM* curlM) {
    FILE* file;
    fopen_s(&file, dest->chars, "w");
    if (file != NULL) {
        CURL* curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_PRIVATE, file);
        curl_easy_setopt(curl, CURLOPT_URL, url->chars);
        curl_multi_add_handle(curlM, curl);
        return true;
    }
    return false;
}

ObjString* httpRawURL(VM* vm, Value value) {
    if (IS_INSTANCE(value)) {
        ObjInstance* url = AS_INSTANCE(value);
        return AS_STRING(getObjProperty(vm, url, "raw"));
    }
    else return AS_STRING(value);
}

CURLcode httpSendRequest(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* data, CURL* curl, CURLResponse* curlResponse) {
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