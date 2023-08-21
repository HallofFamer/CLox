#pragma once
#ifndef clox_os_h
#define clox_os_h

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#define fopen_s(fp,filename,mode) ((*(fp))=fopen((filename),(mode)))==NULL
#define localtime_s(buf,timer) localtime(timer)
#define sscanf_s(buffer,format,...) sscanf(buffer,format,__VA_ARGS__)
#define sprintf_s(buffer,bufsz,format,...) sprintf(buffer,format,__VA_ARGS__)
#define strtok_s(str,delim,ctx) strtok(str,delim)
#define _chmod(path, mode) chmod(path, mode)
#define _getcwd(buffer, size) getcwd(buffer, size)
#define _mkdir(path) mkdir(path, 777)
#define _rmdir(path) rmdir(path)
#define _strdup(str1) strdup(str1)
#endif

static bool isWindows() { 
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

#endif // !clox_os_h
