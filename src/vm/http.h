#pragma once
#ifndef clox_http_h
#define clox_http_h

#include <curl/curl.h>

#include "loop.h"

typedef struct CURLData CURLData;
typedef void (*curl_multi_cb)(CURLData* data);

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

typedef struct CURLResponse {
    char* headers;
    char* content;
    size_t hSize;
    size_t cSize;
} CURLResponse;

struct CURLData{
    VM* vm;
    CURL* curl;
    ObjString* url;
    HTTPMethod method;
    ObjPromise* promise;
    struct curl_slist* curlHeaders;
    CURLResponse* curlResponse;
    curl_multi_cb callback;
    FILE* file;
};

typedef struct {
    VM* vm;
    CURLM* curlM;
    uv_timer_t* timer;
} CURLMData;

typedef struct CURLContext {
    uv_poll_t poll;
    curl_socket_t socket;
    CURLMData* data;
    bool isInitialized;
} CURLContext;

ObjArray* httpCreateCookies(VM* vm, CURL* curl);
ObjArray* httpCreateHeaders(VM* vm, CURLResponse curlResponse);
ObjInstance* httpCreateResponse(VM* vm, ObjString* url, CURL* curl, CURLResponse curlResponse);
CURLContext* httpCURLCreateContext(CURLMData* data);
CURLData* httpCURLData(VM* vm, CURL* curl, ObjString* url, HTTPMethod method, ObjPromise* promise, CURLResponse* curlResponse, curl_multi_cb callback);
size_t httpCURLHeaders(void* headers, size_t size, size_t nitems, void* userData);
void httpCURLInitContext(CURLContext* context, curl_socket_t socket);
CURLMData* httpCURLMData(VM* vm, CURLM* curlM, uv_timer_t* timer);
int httpCURLPollSocket(CURL* curl, curl_socket_t socket, int action, void* userData, void* socketData);
size_t httpCURLResponse(void* contents, size_t size, size_t nmemb, void* userData);
void httpCURLStartTimeout(CURLM* curlM, long timeout, void* userData);
CURLcode httpDownloadFile(VM* vm, ObjString* src, ObjString* dest, CURL* curl);
ObjPromise* httpDownloadFileAsync(VM* vm, ObjString* src, ObjString* dest, CURLMData* curlMData, curl_multi_cb callback);
void httpOnDownloadFile(CURLData* data);
void httpOnSendRequest(CURLData* data);
struct curl_slist* httpParseHeaders(VM* vm, ObjDictionary* headers, CURL* curl);
ObjString* httpParsePostData(VM* vm, ObjDictionary* postData);
ObjString* httpRawURL(VM* vm, Value value);
CURLcode httpSendRequest(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* data, CURL* curl, CURLResponse* curlResponse);
ObjPromise* httpSendRequestAsync(VM* vm, ObjString* url, HTTPMethod method, ObjDictionary* headers, ObjDictionary* data, CURLMData* curlMData, curl_multi_cb callback);

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
