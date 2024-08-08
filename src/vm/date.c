#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "native.h"
#include "object.h"
#include "vm.h"
#include "../common/os.h"

static struct tm dateToTm(int year, int month, int day) {
    struct tm cDate = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day
    };
    return cDate;
}

static struct tm dateTimeToTm(int year, int month, int day, int hour, int minute, int second) {
    struct tm cDate = {
        .tm_year = year - 1900,
        .tm_mon = month - 1,
        .tm_mday = day,
        .tm_hour = hour,
        .tm_min = minute,
        .tm_sec = second
    };
    return cDate;
}

static double dateGetTimestamp(int year, int month, int day) {
    struct tm cTime = dateToTm(year, month, day);
    return (double)mktime(&cTime);
}

static double dateTimeGetTimestamp(int year, int month, int day, int hour, int minute, int second) {
    struct tm cTime = dateTimeToTm(year, month, day, hour, minute, second);
    return (double)mktime(&cTime);
}

double dateObjGetTimestamp(VM* vm, ObjInstance* date) {
    Value year = getObjProperty(vm, date, "year");
    Value month = getObjProperty(vm, date, "month");
    Value day = getObjProperty(vm, date, "day");
    return dateGetTimestamp(AS_INT(year), AS_INT(month), AS_INT(day));
}

ObjInstance* dateObjNow(VM* vm, ObjClass* klass) {
    time_t nowTime;
    time(&nowTime);
    struct tm now;
    localtime_s(&now, &nowTime);
    ObjInstance* date = newInstance(vm, klass);
    push(vm, OBJ_VAL(date));
    setObjProperty(vm, date, "year", INT_VAL(1900 + now.tm_year));
    setObjProperty(vm, date, "month", INT_VAL(1 + now.tm_mon));
    setObjProperty(vm, date, "day", INT_VAL(now.tm_mday));
    pop(vm);
    return date;
}

double dateTimeObjGetTimestamp(VM* vm, ObjInstance* dateTime) {
    Value year = getObjProperty(vm, dateTime, "year");
    Value month = getObjProperty(vm, dateTime, "month");
    Value day = getObjProperty(vm, dateTime, "day");
    Value hour = getObjProperty(vm, dateTime, "hour");
    Value minute = getObjProperty(vm, dateTime, "minute");
    Value second = getObjProperty(vm, dateTime, "second");
    return dateTimeGetTimestamp(AS_INT(year), AS_INT(month), AS_INT(day), AS_INT(hour), AS_INT(minute), AS_INT(second));
}

ObjInstance* dateObjFromTimestamp(VM* vm, ObjClass* dateClass, double timeValue) {
    time_t timestamp = (time_t)timeValue;
    struct tm time;
    localtime_s(&time, &timestamp);
    ObjInstance* date = newInstance(vm, dateClass);
    push(vm, OBJ_VAL(date));
    setObjProperty(vm, date, "year", INT_VAL(1900 + time.tm_year));
    setObjProperty(vm, date, "month", INT_VAL(1 + time.tm_mon));
    setObjProperty(vm, date, "day", INT_VAL(time.tm_mday));
    pop(vm);
    return date;
}

ObjInstance* dateTimeObjFromTimestamp(VM* vm, ObjClass* dateTimeClass, double timeValue) {
    time_t timestamp = (time_t)timeValue;
    struct tm time;
    localtime_s(&time, &timestamp);
    ObjInstance* dateTime = newInstance(vm, dateTimeClass);
    push(vm, OBJ_VAL(dateTime));
    setObjProperty(vm, dateTime, "year", INT_VAL(1900 + time.tm_year));
    setObjProperty(vm, dateTime, "month", INT_VAL(1 + time.tm_mon));
    setObjProperty(vm, dateTime, "day", INT_VAL(time.tm_mday));
    setObjProperty(vm, dateTime, "hour", INT_VAL(time.tm_hour));
    setObjProperty(vm, dateTime, "minute", INT_VAL(time.tm_min));
    setObjProperty(vm, dateTime, "second", INT_VAL(time.tm_sec));
    pop(vm);
    return dateTime;
}

ObjInstance* dateTimeObjNow(VM* vm, ObjClass* klass) {
    time_t nowTime;
    time(&nowTime);
    struct tm now;
    localtime_s(&now, &nowTime);
    ObjInstance* dateTime = newInstance(vm, getNativeClass(vm, "clox.std.util.DateTime"));
    push(vm, OBJ_VAL(dateTime));
    setObjProperty(vm, dateTime, "year", INT_VAL(1900 + now.tm_year));
    setObjProperty(vm, dateTime, "month", INT_VAL(1 + now.tm_mon));
    setObjProperty(vm, dateTime, "day", INT_VAL(now.tm_mday));
    setObjProperty(vm, dateTime, "hour", INT_VAL(now.tm_hour));
    setObjProperty(vm, dateTime, "minute", INT_VAL(now.tm_min));
    setObjProperty(vm, dateTime, "second", INT_VAL(now.tm_sec));
    pop(vm);
    return dateTime;
}

static void durationInit(int* duration, int days, int hours, int minutes, int seconds) {
    if (seconds > 60) {
        minutes += seconds / 60;
        seconds %= 60;
    }

    if (minutes > 60) {
        hours += minutes / 60;
        minutes %= 60;
    }

    if (hours > 60) {
        days += hours / 24;
        hours %= 24;
    }

    duration[0] = days;
    duration[1] = hours;
    duration[2] = minutes;
    duration[3] = seconds;
}

void durationFromSeconds(int* duration, double seconds) {
    durationInit(duration, 0, 0, 0, (int)seconds);
}

void durationFromArgs(int* duration, Value* args) {
    durationInit(duration, AS_INT(args[0]), AS_INT(args[1]), AS_INT(args[2]), AS_INT(args[3]));
}

void durationObjInit(VM* vm, int* duration, ObjInstance* object) {
    push(vm, OBJ_VAL(object));
    setObjProperty(vm, object, "days", INT_VAL(duration[0]));
    setObjProperty(vm, object, "hours", INT_VAL(duration[1]));
    setObjProperty(vm, object, "minutes", INT_VAL(duration[2]));
    setObjProperty(vm, object, "seconds", INT_VAL(duration[3]));
    pop(vm);
}

double durationTotalSeconds(VM* vm, ObjInstance* duration) {
    Value days = getObjProperty(vm, duration, "days");
    Value hours = getObjProperty(vm, duration, "hours");
    Value minutes = getObjProperty(vm, duration, "minutes");
    Value seconds = getObjProperty(vm, duration, "seconds");
    return 86400.0 * AS_INT(days) + 3600.0 * AS_INT(hours) + 60.0 * AS_INT(minutes) + AS_INT(seconds);
}