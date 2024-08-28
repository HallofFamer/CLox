#pragma once
#ifndef clox_os_h
#define clox_os_h

#include "common.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define closesocket(descriptor) close(descriptor)
#define fopen_s(fp,filename,mode) ((*(fp))=fopen((filename),(mode)))==NULL
#define localtime_s(buf,timer) localtime(timer)
#define sscanf_s(buffer,format,...) sscanf(buffer,format,__VA_ARGS__)
#define sprintf_s(buffer,bufsz,format,...) sprintf(buffer,format,__VA_ARGS__)
#define snprintf_s(buffer,bufsz,format,...) snprintf(buffer,format,__VA_ARGS__)
#define strtok_s(str,delim,ctx) strtok(str,delim)
#define _chmod(path,mode) chmod(path,mode)
#define _getcwd(buffer,size) getcwd(buffer,size)
#define _mkdir(path) mkdir(path,777)
#define _rmdir(path) rmdir(path)
#define _strdup(str1) strdup(str1)
#define SOCKET int
#define INVALID_SOCKET (SOCKET)(~0)
#define O_BINARY 0
#define O_TEXT 0

void _itoa_s(int value, char buffer[], size_t bufsz, int radix);
#endif

void runAtStartup();
void runAtExit(void);

static inline bool isWindows() { 
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

#endif // !clox_os_h
