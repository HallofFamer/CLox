#pragma once
#ifndef clox_date_h
#define clox_date_h

#include "value.h"

double dateObjGetTimestamp(VM* vm, ObjInstance* date);
ObjInstance* dateObjNow(VM* vm, ObjClass* klass);
double dateTimeObjGetTimestamp(VM* vm, ObjInstance* dateTime);
ObjInstance* dateObjFromTimestamp(VM* vm, ObjClass* dateClass, double timeValue);
ObjInstance* dateTimeObjFromTimestamp(VM* vm, ObjClass* dateTimeClass, double timeValue);
ObjInstance* dateTimeObjNow(VM* vm, ObjClass* klass);
void durationFromSeconds(int* duration, double seconds);
void durationFromArgs(int* duration, Value* args);
void durationObjInit(VM* vm, int* duration, ObjInstance* object);
double durationTotalSeconds(VM* vm, ObjInstance* duration);

#endif // !clox_date_h