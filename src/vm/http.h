#pragma once
#ifndef clox_http_h
#define clox_http_h

#include <curl/curl.h>

#include "common.h"
#include "loop.h"

typedef struct {
    VM* vm;
    CURLM* curlM;
    uv_timer_t* timer;
    ObjPromise* promise;
} CURLData;

typedef struct CURLContext {
    uv_poll_t poll;
    curl_socket_t socket;
    CURLData* data;
    bool isInitialized;
} CURLContext;

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

ObjArray* httpCreateCookies(VM* vm, CURL* curl);
ObjArray* httpCreateHeaders(VM* vm, CURLResponse curlResponse);
ObjInstance* httpCreateResponse(VM* vm, ObjString* url, CURL* curl, CURLResponse curlResponse);
void httpCURLClose(CURLContext* context);
CURLContext* httpCURLCreateContext(CURLData* data);
size_t httpCURLHeaders(void* headers, size_t size, size_t nitems, void* userData);
void httpCURLInitContext(CURLContext* context, curl_socket_t socket);
int httpCURLPollSocket(CURL* curl, curl_socket_t socket, int action, void* userData, void* socketData);
size_t httpCURLResponse(void* contents, size_t size, size_t nmemb, void* userData);
void httpCURLStartTimeout(CURLM* curlM, long timeout, void* userData);
CURLcode httpDownloadFile(VM* vm, ObjString* src, ObjString* dest, CURL* curl);
ObjPromise* httpDownloadFileAsync(VM* vm, ObjString* src, ObjString* dest, CURLM* curlM);
struct curl_slist* httpParseHeaders(VM* vm, ObjDictionary* headers, CURL* curl);
ObjString* httpParsePostData(VM* vm, ObjDictionary* postData);
bool httpPrepareDownloadFile(VM* vm, ObjString* src, ObjString* dest, CURLM* curlM, ObjPromise* promise);
ObjString* httpRawURL(VM* vm, Value value);
CURLcode httpSendRequest(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* data, CURL* curl, CURLResponse* curlResponse);

static inline char* httpMapMethod(HTTPMethod method) {
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

#endif // !clox_http_h
