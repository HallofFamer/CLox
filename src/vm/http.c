#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "memory.h"
#include "native.h"
#include "object.h"
#include "vm.h"
#include "../common/os.h"

static CURLMsg* httpCURLInfoRead(CURLContext* context, CURLData* data, int* messagesInQueue) {
    CURLMsg* message = curl_multi_info_read(context->data->curlM, messagesInQueue);
    if (data != NULL) {
        curl_multi_cb callback = data->callback;
        callback(data);
    }
    return message;
}

static void httpCURLInitResponse(CURLResponse* curlResponse) {
    curlResponse->headers = malloc(0);
    curlResponse->content = malloc(0);
    curlResponse->hSize = 0;
    curlResponse->cSize = 0;
}

static void httpCURLPerform(uv_poll_t* poll, int status, int events) {
    int running;
    int flags = 0;
    CURLContext* context = (CURLContext*)poll->data;
    uv_timer_stop(context->data->timer);

    if (events & UV_READABLE) flags |= CURL_CSELECT_IN;
    if (events & UV_WRITABLE) flags |= CURL_CSELECT_OUT;
    curl_multi_socket_action(context->data->curlM, context->socket, flags, &running);
    
    CURLMsg* message;
    int pending;
    CURLData* curlData = NULL;

    while (message = httpCURLInfoRead(context, curlData, &pending)) {
        switch (message->msg) {
            case CURLMSG_DONE:
                curl_multi_remove_handle(context->data->curlM, message->easy_handle);
                curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &curlData);
                break;
            default:
                curlData = NULL;
                break;
        }
    }
}

static size_t httpCURLWriteFile(void* contents, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(contents, size, nmemb, stream);
}

static void httpCURLOnClose(uv_handle_t* handle) {
    CURLContext* context = (CURLContext*)handle->data;
    free(context);
}

static void httpCURLOnTimeout(uv_timer_t* timer) {
    int numRunningHandles;
    CURLMData* curlMData = (CURLMData*)timer->data;
    curl_multi_socket_action(curlMData->curlM, CURL_SOCKET_TIMEOUT, 0, &numRunningHandles);
}

static void httpCURLRequest(VM* vm, CURL* curl, ObjString* url, HTTPMethod method, ObjDictionary* data, CURLResponse* curlResponse) {
    const char* urlChars = url->chars;
    curl_easy_setopt(curl, CURLOPT_URL, urlChars);

    if (method > HTTP_POST) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, httpMapMethod(method));
    }

    if (method == HTTP_HEAD) {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    else if (method == HTTP_POST || method == HTTP_PUT || method == HTTP_PATCH) {
        const char* dataChars = httpParsePostData(vm, data)->chars;
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
}

static void httpCURLClose(CURLContext* context) {
    uv_close((uv_handle_t*)&context->poll, httpCURLOnClose);
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

CURLContext* httpCURLCreateContext(CURLMData* data) {
    CURLContext* context = ALLOCATE_STRUCT(CURLContext);
    if (context != NULL) {
        context->data = data;
        context->isInitialized = false;
    }
    return context;
}

CURLData* httpCURLData(VM* vm, CURL* curl, ObjString* url, HTTPMethod method, ObjPromise* promise, CURLResponse* curlResponse, curl_multi_cb callback) {
    CURLData* curlData = ALLOCATE_STRUCT(CURLData);
    if (curlData != NULL) {
        curlData->vm = vm;
        curlData->curl = curl;
        curlData->url = url;
        curlData->method = method;
        curlData->promise = promise;
        curlData->curlHeaders = NULL;
        curlData->curlResponse = curlResponse;
        curlData->callback = callback;
    }
    return curlData;
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

void httpCURLInitContext(CURLContext* context, curl_socket_t socket) {
    context->socket = socket;
    uv_poll_init_socket(context->data->vm->eventLoop, &context->poll, socket);
    context->poll.data = context;
    context->isInitialized = true;
}

CURLMData* httpCURLMData(VM* vm, CURLM* curlM, uv_timer_t* timer) {
    CURLMData* curlMData = ALLOCATE_STRUCT(CURLMData);
    if (curlMData != NULL) {
        curlMData->vm = vm;
        curlMData->curlM = curlM;
        curlMData->timer = timer;
    }
    return curlMData;
}


int httpCURLPollSocket(CURL* curl, curl_socket_t socket, int action, void* userData, void* socketData) {
    CURLContext* context = NULL;
    int events = 0;

    switch (action) {
        case CURL_POLL_IN:
        case CURL_POLL_OUT:
        case CURL_POLL_INOUT:
            if (socketData) context = (CURLContext*)socketData;
            else {
                CURLMData* curlMData = (CURLMData*)userData;
                context = httpCURLCreateContext(curlMData);                
                if (!context->isInitialized) httpCURLInitContext(context, socket);
            }

            curl_multi_assign(context->data->curlM, socket, (void*)context);
            if (action != CURL_POLL_IN) events |= UV_WRITABLE;
            if (action != CURL_POLL_OUT) events |= UV_READABLE;
            uv_poll_start(&context->poll, events, httpCURLPerform);
            break;
        case CURL_POLL_REMOVE:
            if (socketData) {
                context = (CURLContext*)socketData;
                uv_poll_stop(&context->poll);
                httpCURLClose(context);
                curl_multi_assign(context->data->curlM, socket, NULL);
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

void httpCURLStartTimeout(CURLM* curlM, long timeout, void* userData) {
    if (timeout <= 0) timeout = 1;
    CURLMData* curlMData = (CURLMData*)userData;
    uv_timer_start(curlMData->timer, httpCURLOnTimeout, timeout, 0);
}

CURLcode httpDownloadFile(VM* vm, ObjString* src, ObjString* dest, CURL* curl) {
    const char* url = src->chars;
    curl_easy_setopt(curl, CURLOPT_URL, url);
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

ObjPromise* httpDownloadFileAsync(VM* vm, ObjString* src, ObjString* dest, CURLMData* curlMData, curl_multi_cb callback) {
    FILE* file;
    fopen_s(&file, dest->chars, "w");
    if (file != NULL) {
        CURL* curl = curl_easy_init();
        const char* url = src->chars;
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        CURLData* curlData = httpCURLData(vm, curl, src, HTTP_GET, promise, NULL, callback);
        curlData->file = file;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
        curl_easy_setopt(curl, CURLOPT_PRIVATE, curlData);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_multi_add_handle(curlMData->curlM, curl);
        return promise;
    }
    return NULL;
}

void httpOnDownloadFile(CURLData* data) {
    LOOP_PUSH_DATA(data);
    if (data->file) fclose(data->file);
    promiseFulfill(data->vm, data->promise, NIL_VAL);
    curl_easy_cleanup(data->curl);
    LOOP_POP_DATA(data);
}

void httpOnSendRequest(CURLData* data) {
    LOOP_PUSH_DATA(data);
    ObjInstance* httpResponse = httpCreateResponse(data->vm, data->url, data->curl, *data->curlResponse);
    promiseFulfill(data->vm, data->promise, OBJ_VAL(httpResponse));
    curl_easy_cleanup(data->curl);
    if (data->curlHeaders != NULL) curl_slist_free_all(data->curlHeaders);
    free(data->curlResponse);
    LOOP_POP_DATA(data);
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

ObjString* httpRawURL(VM* vm, Value value) {
    if (IS_INSTANCE(value)) {
        ObjInstance* url = AS_INSTANCE(value);
        return AS_STRING(getObjProperty(vm, url, "raw"));
    }
    else return AS_STRING(value);
}

CURLcode httpSendRequest(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* data, CURL* curl, CURLResponse* curlResponse) {
    httpCURLInitResponse(curlResponse);
    httpCURLRequest(vm, curl, url, method, data, curlResponse);
    return curl_easy_perform(curl);
}

ObjPromise* httpSendRequestAsync(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* headers, ObjDictionary* data, CURLMData* curlMData, curl_multi_cb callback) {
    CURLResponse* curlResponse = ALLOCATE_STRUCT(CURLResponse);
    if (curlResponse != NULL) {
        CURL* curl = curl_easy_init();
        ObjPromise* promise = newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        CURLData* curlData = httpCURLData(vm, curl, url, method, promise, curlResponse, callback);
        httpCURLInitResponse(curlResponse);
        if (headers != NULL) curlData->curlHeaders = httpParseHeaders(vm, headers, curl);
        httpCURLRequest(vm, curl, url, method, data, curlResponse);
        curl_easy_setopt(curl, CURLOPT_PRIVATE, curlData);
        curl_multi_add_handle(curlMData->curlM, curl);
        return promise;
    }
    return NULL;
}
