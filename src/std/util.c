#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "../inc/pcg.h"
#include "../inc/regex.h"
#include "../inc/uuid4.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/os.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

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

static double dateObjGetTimestamp(VM* vm, ObjInstance* date) {
    Value year = getObjProperty(vm, date, "year");
    Value month = getObjProperty(vm, date, "month");
    Value day = getObjProperty(vm, date, "day");
    return dateGetTimestamp(AS_INT(year), AS_INT(month), AS_INT(day));
}

static ObjInstance* dateObjNow(VM* vm, ObjClass* klass) {
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

static double dateTimeObjGetTimestamp(VM* vm, ObjInstance* dateTime) {
    Value year = getObjProperty(vm, dateTime, "year");
    Value month = getObjProperty(vm, dateTime, "month");
    Value day = getObjProperty(vm, dateTime, "day");
    Value hour = getObjProperty(vm, dateTime, "hour");
    Value minute = getObjProperty(vm, dateTime, "minute");
    Value second = getObjProperty(vm, dateTime, "second");
    return dateTimeGetTimestamp(AS_INT(year), AS_INT(month), AS_INT(day), AS_INT(hour), AS_INT(minute), AS_INT(second));
}

static ObjInstance* dateObjFromTimestamp(VM* vm, ObjClass* dateClass, double timeValue) {
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

static ObjInstance* dateTimeObjFromTimestamp(VM* vm, ObjClass* dateTimeClass, double timeValue) {
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

static ObjInstance* dateTimeObjNow(VM* vm, ObjClass* klass) {
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

static void durationFromSeconds(int* duration, double seconds) {
    durationInit(duration, 0, 0, 0, (int)seconds);
}

static void durationFromArgs(int* duration, Value* args) {
    durationInit(duration, AS_INT(args[0]), AS_INT(args[1]), AS_INT(args[2]), AS_INT(args[3]));
}

static void durationObjInit(VM* vm, int* duration, ObjInstance* object) {
    push(vm, OBJ_VAL(object));
    setObjProperty(vm, object, "days", INT_VAL(duration[0]));
    setObjProperty(vm, object, "hours", INT_VAL(duration[1]));
    setObjProperty(vm, object, "minutes", INT_VAL(duration[2]));
    setObjProperty(vm, object, "seconds", INT_VAL(duration[3]));
    pop(vm);
}

static double durationTotalSeconds(VM* vm, ObjInstance* duration) {
    Value days = getObjProperty(vm, duration, "days");
    Value hours = getObjProperty(vm, duration, "hours");
    Value minutes = getObjProperty(vm, duration, "minutes");
    Value seconds = getObjProperty(vm, duration, "seconds");
    return 86400.0 * AS_INT(days) + 3600.0 * AS_INT(hours) + 60.0 * AS_INT(minutes) + AS_INT(seconds);
}

static bool uuidCheckChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static bool uuidCheckSubString(ObjString* uuid, int start, int end) {
    for (int i = start; i < end; i++) {
        if (!uuidCheckChar(uuid->chars[i])) return false;
    }
    return true;
}

static bool uuidCheckString(ObjString* uuid) {
    if (uuid->length != UUID4_LEN - 1) return false;
    if(!uuidCheckSubString(uuid, 0, 8)) return false;
    
    if (uuid->chars[8] != '-') return false;
    if(!uuidCheckSubString(uuid, 9, 13)) return false;

    if (uuid->chars[13] != '-' || uuid->chars[14] != '4') return false;
    if(!uuidCheckSubString(uuid, 15, 18)) return false;

    if (uuid->chars[18] != '-') return false;
    if (uuid->chars[19] != '8' && uuid->chars[19] != '9' && uuid->chars[19] != 'a' && uuid->chars[19] != 'b') return false;
    if(!uuidCheckSubString(uuid, 20, 23)) return false;

    if (uuid->chars[23] != '-') return false;
    if (!uuidCheckSubString(uuid, 24, 36)) return false;
    return true;
}

LOX_METHOD(Date, __init__) {
    ASSERT_ARG_COUNT("Date::__init__(year, month, day)", 3);
    ASSERT_ARG_TYPE("Date::__init__(year, month, day)", 0, Int);
    ASSERT_ARG_TYPE("Date::__init__(year, month, day)", 1, Int);
    ASSERT_ARG_TYPE("Date::__init__(year, month, day)", 2, Int);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "year", args[0]);
    setObjProperty(vm, self, "month", args[1]);
    setObjProperty(vm, self, "day", args[2]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Date, compareTo) {
    ASSERT_ARG_COUNT("Date::compareTo(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::compareTo(date)", 0, clox.std.util.Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    if (timestamp > timestamp2) RETURN_INT(1);
    else if (timestamp < timestamp2) RETURN_INT(-1);
    else RETURN_INT(0);
}

LOX_METHOD(Date, diff) {
    ASSERT_ARG_COUNT("Date::diff(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::diff(date)", 0, clox.std.util.Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_NUMBER(timestamp - timestamp2);
}

LOX_METHOD(Date, getTimestamp) {
    ASSERT_ARG_COUNT("Date::getTimestamp()", 0);
    RETURN_NUMBER(dateObjGetTimestamp(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Date, toDateTime) {
    ASSERT_ARG_COUNT("Date::toDateTime()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* dateTime = newInstance(vm, getNativeClass(vm, "clox.std.util.DateTime"));
    push(vm, OBJ_VAL(dateTime));
    copyObjProperty(vm, self, dateTime, "year");
    copyObjProperty(vm, self, dateTime, "month");
    copyObjProperty(vm, self, dateTime, "day");
    setObjProperty(vm, dateTime, "hour", INT_VAL(0));
    setObjProperty(vm, dateTime, "minute", INT_VAL(0));
    setObjProperty(vm, dateTime, "second", INT_VAL(0));
    pop(vm);
    RETURN_OBJ(dateTime);
}

LOX_METHOD(Date, toString) {
    ASSERT_ARG_COUNT("Date::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value year = getObjProperty(vm, self, "year");
    Value month = getObjProperty(vm, self, "month");
    Value day = getObjProperty(vm, self, "day");
    RETURN_STRING_FMT("%d-%02d-%02d", AS_INT(year), AS_INT(month), AS_INT(day));
}

LOX_METHOD(Date, __equal__) { 
    ASSERT_ARG_COUNT("Date::==(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::==(date)", 0, clox.std.util.Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp == timestamp2);
}

LOX_METHOD(Date, __greater__) { 
    ASSERT_ARG_COUNT("Date::>(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::>(date)", 0, clox.std.util.Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp > timestamp2);
}

LOX_METHOD(Date, __less__) {
    ASSERT_ARG_COUNT("Date::<(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::<(date)", 0, clox.std.util.Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp < timestamp2);
}

LOX_METHOD(Date, __add__) {
    ASSERT_ARG_COUNT("Date::+(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::+(duration)", 0, clox.std.util.Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateObjGetTimestamp(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* date = dateObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(date);
}

LOX_METHOD(Date, __subtract__) {
    ASSERT_ARG_COUNT("Date::-(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::-(duration)", 0, clox.std.util.Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateObjGetTimestamp(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* date = dateObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(date);
}

LOX_METHOD(DateClass, fromTimestamp) {
    ASSERT_ARG_COUNT("Date class::fromTimestamp(timestamp)", 1);
    ASSERT_ARG_TYPE("Date class::fromTimestamp(timestamp)", 0, Number);
    RETURN_OBJ(dateObjFromTimestamp(vm, AS_CLASS(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(DateClass, parse) {
    ASSERT_ARG_COUNT("Date class::parse(dateString)", 1);
    ASSERT_ARG_TYPE("Date class::parse(dateString)", 0, String);
    ObjClass* self = AS_CLASS(receiver);
    ObjString* dateString = AS_STRING(args[0]);

    int year, month, day;
    if (sscanf_s(dateString->chars, "%d-%d-%d", &year, &month, &day) == 3) {
        ObjInstance* instance = newInstance(vm, self);
        push(vm, OBJ_VAL(instance));
        setObjProperty(vm, instance, "year", INT_VAL(year));
        setObjProperty(vm, instance, "month", INT_VAL(month));
        setObjProperty(vm, instance, "day", INT_VAL(day));
        pop(vm);
        RETURN_OBJ(instance);
    }
    else {
        THROW_EXCEPTION(clox.std.util.DateFormatException, "Failed to parse Date from input string, please make sure the date has format YYYY-MM-DD.");
    }
}

LOX_METHOD(DateTime, __init__) {
    ASSERT_ARG_COUNT("DateTime::__init__(year, month, day, hour, minute, second)", 6);
    ASSERT_ARG_TYPE("DateTime::__init__(year, month, day, hour, minute, second)", 0, Int);
    ASSERT_ARG_TYPE("DateTime::__init__(year, month, day, hour, minute, second)", 1, Int);
    ASSERT_ARG_TYPE("DateTime::__init__(year, month, day, hour, minute, second)", 2, Int);
    ASSERT_ARG_TYPE("DateTime::__init__(year, month, day, hour, minute, second)", 3, Int);
    ASSERT_ARG_TYPE("DateTime::__init__(year, month, day, hour, minute, second)", 4, Int);
    ASSERT_ARG_TYPE("DateTime::__init__(year, month, day, hour, minute, second)", 5, Int);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "year", args[0]);
    setObjProperty(vm, self, "month", args[1]);
    setObjProperty(vm, self, "day", args[2]);
    setObjProperty(vm, self, "hour", args[3]);
    setObjProperty(vm, self, "minute", args[4]);
    setObjProperty(vm, self, "second", args[5]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(DateTime, compareTo) {
    ASSERT_ARG_COUNT("DateTime::compareTo(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::compareTo(dateTime)", 0, clox.std.util.Date);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    if (timestamp > timestamp2) RETURN_INT(1);
    else if (timestamp < timestamp2) RETURN_INT(-1);
    else RETURN_INT(0);
}

LOX_METHOD(DateTime, diff) {
    ASSERT_ARG_COUNT("DateTime::diff(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::diff(dateTime)", 0, clox.std.util.DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_NUMBER(timestamp - timestamp2);
}

LOX_METHOD(DateTime, getTimestamp) {
    ASSERT_ARG_COUNT("DateTime::getTimestamp()", 0);
    RETURN_NUMBER(dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(DateTime, toDate) {
    ASSERT_ARG_COUNT("DateTime::toDate()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* date = newInstance(vm, getNativeClass(vm, "clox.std.util.Date"));
    push(vm, OBJ_VAL(date));
    copyObjProperty(vm, self, date, "year");
    copyObjProperty(vm, self, date, "month");
    copyObjProperty(vm, self, date, "day");
    pop(vm);
    RETURN_OBJ(date);
}

LOX_METHOD(DateTime, toString) {
    ASSERT_ARG_COUNT("DateTime::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value year = getObjProperty(vm, self, "year");
    Value month = getObjProperty(vm, self, "month");
    Value day = getObjProperty(vm, self, "day");
    Value hour = getObjProperty(vm, self, "hour");
    Value minute = getObjProperty(vm, self, "minute");
    Value second = getObjProperty(vm, self, "second");
    RETURN_STRING_FMT("%d-%02d-%02d %02d:%02d:%02d", AS_INT(year), AS_INT(month), AS_INT(day), AS_INT(hour), AS_INT(minute), AS_INT(second));
}

LOX_METHOD(DateTime, __equal__) {
    ASSERT_ARG_COUNT("DateTime::==(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::==(dateTime)", 0, clox.std.util.DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp == timestamp2);
}

LOX_METHOD(DateTime, __greater__) {
    ASSERT_ARG_COUNT("DateTime::>(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::>(dateTime)", 0, clox.std.util.DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp > timestamp2);
}

LOX_METHOD(DateTime, __less__) {
    ASSERT_ARG_COUNT("DateTime::<(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::<(dateTime)", 0, clox.std.util.DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp < timestamp2);
}

LOX_METHOD(DateTime, __add__) {
    ASSERT_ARG_COUNT("DateTime::+(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::+(duration)", 0, clox.std.util.Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateTimeObjGetTimestamp(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* dateTime = dateTimeObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(dateTime);
}

LOX_METHOD(DateTime, __subtract__) {
    ASSERT_ARG_COUNT("DateTime::-(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::-(duration)", 0, clox.std.util.Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateTimeObjGetTimestamp(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* dateTime = dateTimeObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(dateTime);
}


LOX_METHOD(DateTimeClass, fromTimestamp) {
    ASSERT_ARG_COUNT("DateTime class::fromTimestamp(timestamp)", 1);
    ASSERT_ARG_TYPE("DateTime class::fromTimestamp(timestamp)", 0, Number);
    RETURN_OBJ(dateTimeObjFromTimestamp(vm, AS_CLASS(receiver), AS_NUMBER(args[0])));
}

LOX_METHOD(DateTimeClass, parse) {
    ASSERT_ARG_COUNT("DateTime class::parse(dateString)", 1);
    ASSERT_ARG_TYPE("DateTime class::parse(dateString)", 0, String);
    ObjClass* self = AS_CLASS(receiver);
    ObjString* dateTimeString = AS_STRING(args[0]);

    int year, month, day, hour, minute, second;
    if (sscanf_s(dateTimeString->chars, "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
        ObjInstance* instance = newInstance(vm, self);
        push(vm, OBJ_VAL(instance));
        setObjProperty(vm, instance, "year", INT_VAL(year));
        setObjProperty(vm, instance, "month", INT_VAL(month));
        setObjProperty(vm, instance, "day", INT_VAL(day));
        setObjProperty(vm, instance, "hour", INT_VAL(hour));
        setObjProperty(vm, instance, "minute", INT_VAL(minute));
        setObjProperty(vm, instance, "second", INT_VAL(second));
        pop(vm);
        RETURN_OBJ(instance);
    }
    else {
        THROW_EXCEPTION(clox.std.util.DateFormatException, "Failed to parse DateTime from input string, please make sure the date has format YYYY-MM-DD H:i:s.");
    }
}

LOX_METHOD(Duration, __init__) {
    ASSERT_ARG_COUNT("Duration::__init__(days, hours, minutes, seconds)", 4);
    ASSERT_ARG_TYPE("Duration::__init__(days, hours, minutes, seconds)", 0, Int);
    ASSERT_ARG_TYPE("Duration::__init__(days, hours, minutes, seconds)", 1, Int);
    ASSERT_ARG_TYPE("Duration::__init__(days, hours, minutes, seconds)", 2, Int);
    ASSERT_ARG_TYPE("Duration::__init__(days, hours, minutes, seconds)", 3, Int);

    int days = AS_INT(args[0]);
    int hours = AS_INT(args[1]);
    int minutes = AS_INT(args[2]);
    int seconds = AS_INT(args[3]);
    if (days < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration::__init__(days, hours, minutes, seconds) expects argument 1 to be a non negative integer but got %d.", days);
    if (hours < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration::__init__(days, hours, minutes, seconds) expects argument 2 to be a non negative integer but got %d.", hours);
    if (minutes < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration::__init__(days, hours, minutes, seconds) expects argument 3 to be a non negative integer but got %d.", minutes);
    if (seconds < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration::__init__(days, hours, minutes, seconds) expects argument 4 to be a non negative integer but got %d.", seconds);

    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromArgs(duration, args);
    durationObjInit(vm, duration, self);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Duration, compareTo) {
    ASSERT_ARG_COUNT("Duration::compareTo(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::compareTo(duration)", 0, clox.std.util.Duration);
    double totalSeconds = durationTotalSeconds(vm, AS_INSTANCE(receiver));
    double totalSeconds2 = durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    if (totalSeconds > totalSeconds2) RETURN_INT(1);
    else if (totalSeconds < totalSeconds2) RETURN_INT(-1);
    else RETURN_INT(0);
}

LOX_METHOD(Duration, getTotalSeconds) {
    ASSERT_ARG_COUNT("Duration::getTotalSeconds()", 0);
    RETURN_NUMBER(durationTotalSeconds(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Duration, toString) {
    ASSERT_ARG_COUNT("Duration::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value days = getObjProperty(vm, self, "days");
    Value hours = getObjProperty(vm, self, "hours");
    Value minutes = getObjProperty(vm, self, "minutes");
    Value seconds = getObjProperty(vm, self, "seconds");
    RETURN_STRING_FMT("%d days, %02d hours, %02d minutes, %02d seconds", AS_INT(days), AS_INT(hours), AS_INT(minutes), AS_INT(seconds));
}

LOX_METHOD(Duration, __equal__) {
    ASSERT_ARG_COUNT("Duration::==(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::==(duration)", 0, clox.std.util.Duration);   
    RETURN_BOOL(durationTotalSeconds(vm, AS_INSTANCE(receiver)) == durationTotalSeconds(vm, AS_INSTANCE(args[0])));
}

LOX_METHOD(Duration, __greater__) {
    ASSERT_ARG_COUNT("Duration::>(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::>(duration)", 0, clox.std.util.Duration);
    RETURN_BOOL(durationTotalSeconds(vm, AS_INSTANCE(receiver)) > durationTotalSeconds(vm, AS_INSTANCE(args[0])));
}

LOX_METHOD(Duration, __less__) {
    ASSERT_ARG_COUNT("Duration::<(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::<(duration)", 0, clox.std.util.Duration);
    RETURN_BOOL(durationTotalSeconds(vm, AS_INSTANCE(receiver)) < durationTotalSeconds(vm, AS_INSTANCE(args[0])));
}

LOX_METHOD(Duration, __add__) {
    ASSERT_ARG_COUNT("Duration::+(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::+(duration)", 0, clox.std.util.Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromSeconds(duration, durationTotalSeconds(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0])));
    ObjInstance* object = newInstance(vm, self->obj.klass);
    durationObjInit(vm, duration, object);
    RETURN_OBJ(object);
}

LOX_METHOD(Duration, __subtract__) {
    ASSERT_ARG_COUNT("Duration::-(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::-(duration)", 0, clox.std.util.Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromSeconds(duration, durationTotalSeconds(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0])));
    ObjInstance* object = newInstance(vm, self->obj.klass);
    durationObjInit(vm, duration, object);
    RETURN_OBJ(object);
}

LOX_METHOD(DurationClass, ofDays) {
    ASSERT_ARG_COUNT("Duration class::ofDays(days)", 1);
    ASSERT_ARG_TYPE("Duration class::ofDays(days)", 0, Int);
    int days = AS_INT(args[0]);
    if (days < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration class::ofDays(days) expects argument 1 to be a non negative integer but got %d.", days);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { days, 0, 0, 0 };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(DurationClass, ofHours) {
    ASSERT_ARG_COUNT("Duration class::ofHours(hours)", 1);
    ASSERT_ARG_TYPE("Duration class::ofHours(hours)", 0, Int);
    int hours = AS_INT(args[0]);
    if (hours < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration class::ofHours(hours) expects argument 1 to be a non negative integer but got %d.", hours);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { 0, hours, 0, 0 };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(DurationClass, ofMinutes) {
    ASSERT_ARG_COUNT("Duration class::ofMinutes(minutes)", 1);
    ASSERT_ARG_TYPE("Duration class::ofMinutes(minutes)", 0, Int);
    int minutes = AS_INT(args[0]);
    if (minutes < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration class::ofMinutes(minutes) expects argument 1 to be a non negative integer but got %d.", minutes);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { 0, 0, minutes, 0 };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(DurationClass, ofSeconds) {
    ASSERT_ARG_COUNT("Duration class::ofSeconds(seconds)", 1);
    ASSERT_ARG_TYPE("Duration class::ofSeconds(seconds)", 0, Int);
    int seconds = AS_INT(args[0]);
    if (seconds < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Duration class::ofSeconds(seconds) expects argument 1 to be a non negative integer but got %d.", seconds);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { 0, 0, 0, seconds };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(Promise, __init__) {
    ASSERT_ARG_COUNT("Promise::__init__(executor)", 1);
    ASSERT_ARG_INSTANCE_OF("Promise::__init__(executor)", 0, clox.std.lang.TCallable);
    ObjPromise* self = AS_PROMISE(receiver);
    self->executor = args[0];
    promiseExecute(vm, self);
    RETURN_OBJ(self);
}

LOX_METHOD(Promise, catch) {
    ASSERT_ARG_COUNT("Promise::catch(closure)", 1);
    ASSERT_ARG_INSTANCE_OF("Promise::catch(closure)", 0, clox.std.lang.TCallable);
    ObjPromise* self = AS_PROMISE(receiver);
    if (self->state == PROMISE_REJECTED) callReentrantMethod(vm, OBJ_VAL(self), args[0], OBJ_VAL(self->exception));
    else self->onCatch = args[0];
    RETURN_OBJ(self);
}

LOX_METHOD(Promise, catchAll) {
    ASSERT_ARG_COUNT("Promise::catchAll(exception)", 1);
    ASSERT_ARG_TYPE("Promise::catchAll(exception)", 0, Exception);
    ObjPromise* self = AS_PROMISE(receiver);
    ObjBoundMethod* reject = AS_BOUND_METHOD(self->capturedValues->elements.values[5]);
    callReentrantMethod(vm, reject->receiver, reject->method, args[0]);
    RETURN_OBJ(self);
}

LOX_METHOD(Promise, finally) {
    ASSERT_ARG_COUNT("Promise::finally(closure)", 1);
    ASSERT_ARG_INSTANCE_OF("Promise::finally(closure)", 0, clox.std.lang.TCallable);
    ObjPromise* self = AS_PROMISE(receiver);
    if (self->state == PROMISE_FULFILLED || self->state == PROMISE_REJECTED) callReentrantMethod(vm, OBJ_VAL(self), args[0], self->value);
    else self->onFinally = args[0];
    RETURN_OBJ(self);
}

LOX_METHOD(Promise, fulfill) {
    ASSERT_ARG_COUNT("Promise::fulfill(value)", 1);
    promiseFulfill(vm, AS_PROMISE(receiver), args[0]);
    RETURN_VAL(receiver);
}

LOX_METHOD(Promise, isResolved) {
    ASSERT_ARG_COUNT("Promise::isResolved()", 0);
    ObjPromise* self = AS_PROMISE(receiver);
    RETURN_BOOL(self->state == PROMISE_FULFILLED || self->state == PROMISE_REJECTED);
}

LOX_METHOD(Promise, raceAll) {
    ASSERT_ARG_COUNT("Promise::raceAll(result)", 1);
    ObjPromise* self = AS_PROMISE(receiver);
    ObjPromise* racePromise = AS_PROMISE(self->capturedValues->elements.values[0]);
    if (racePromise->state == PROMISE_PENDING) { 
        self->value = args[0];
        self->state = PROMISE_FULFILLED;
        promiseThen(vm, racePromise, args[0]);
    }
    RETURN_NIL;
}

LOX_METHOD(Promise, reject) {
    ASSERT_ARG_COUNT("Promise::reject(exception)", 1);
    ASSERT_ARG_TYPE("Promise::reject(exception)", 0, Exception);
    promiseReject(vm, AS_PROMISE(receiver), args[0]);
    RETURN_NIL;
}

LOX_METHOD(Promise, then) {
    ASSERT_ARG_COUNT("Promise::then(onFulfilled)", 1);
    ASSERT_ARG_INSTANCE_OF("Promise::then(onFulfilled)", 0, clox.std.lang.TCallable);
    ObjPromise* self = AS_PROMISE(receiver);
    if (self->state == PROMISE_FULFILLED) {
        self->value = callReentrantMethod(vm, OBJ_VAL(self), args[0], self->value);
        if (IS_PROMISE(self->value)) RETURN_VAL(self->value);
        else RETURN_OBJ(promiseWithFulfilled(vm, self->value));
    }
    else {
        ObjPromise* thenPromise = self->capturedValues->elements.count > 0 ? AS_PROMISE(self->capturedValues->elements.values[0]) : newPromise(vm, PROMISE_PENDING, NIL_VAL, NIL_VAL);
        Value thenChain = getObjMethod(vm, receiver, "thenChain");
        ObjBoundMethod* thenChainMethod = newBoundMethod(vm, receiver, thenChain);
        promiseCapture(vm, self, 2, OBJ_VAL(thenPromise), args[0]);
        promisePushHandler(vm, self, OBJ_VAL(thenChainMethod), thenPromise);
        RETURN_OBJ(thenPromise);
    }
}

LOX_METHOD(Promise, thenAll) {
    ASSERT_ARG_COUNT("Promise::thenAll(result)", 1);
    ObjPromise* self = AS_PROMISE(receiver);
    ObjArray* promises = AS_ARRAY(self->capturedValues->elements.values[0]);
    ObjPromise* allPromise = AS_PROMISE(self->capturedValues->elements.values[1]);
    ObjArray* results = AS_ARRAY(self->capturedValues->elements.values[2]);
    int remainingCount = AS_INT(self->capturedValues->elements.values[3]);
    int index = AS_INT(self->capturedValues->elements.values[4]);

    valueArrayPut(vm, &results->elements, index, args[0]);
    remainingCount--;
    for (int i = 0; i < promises->elements.count; i++) {
        ObjPromise* promise = AS_PROMISE(promises->elements.values[i]);
        promise->capturedValues->elements.values[3] = INT_VAL(remainingCount);
    }

    if (remainingCount <= 0 && allPromise->state == PROMISE_PENDING) {
        promiseThen(vm, allPromise, OBJ_VAL(results));
    }
    RETURN_OBJ(self);
}

LOX_METHOD(Promise, thenChain) {
    ASSERT_ARG_COUNT("Promise::thenChain(result)", 1);
    ObjPromise* self = AS_PROMISE(receiver);
    ObjPromise* thenPromise = AS_PROMISE(self->capturedValues->elements.values[0]);
    Value onFulfilled = self->capturedValues->elements.values[1];
    Value result = callReentrantMethod(vm, OBJ_VAL(thenPromise), onFulfilled, args[0]);
    if (IS_PROMISE(result)) {
        ObjPromise* resultPromise = AS_PROMISE(result);
        Value then = getObjMethod(vm, result, "then");
        Value thenFulfill = getObjMethod(vm, receiver, "thenFulfill");
        ObjBoundMethod* thenFulfillMethod = newBoundMethod(vm, result, thenFulfill);
        promiseCapture(vm, resultPromise, 2, OBJ_VAL(thenPromise), onFulfilled);
        callReentrantMethod(vm, OBJ_VAL(resultPromise), then, OBJ_VAL(thenFulfillMethod));
    }
    else promiseFulfill(vm, thenPromise, result);
    RETURN_OBJ(thenPromise);
}

LOX_METHOD(Promise, thenFulfill) {
    ASSERT_ARG_COUNT("Promise::thenFulfill()", 0);
    ObjPromise* self = AS_PROMISE(receiver);
    ObjPromise* thenPromise = AS_PROMISE(self->capturedValues->elements.values[0]);
    promiseFulfill(vm, thenPromise, NIL_VAL);
    RETURN_OBJ(self);
}

LOX_METHOD(PromiseClass, all) {
    ASSERT_ARG_COUNT("Promise class::all(promises)", 1);
    ASSERT_ARG_TYPE("Promise class::all(promises)", 0, Array);
    RETURN_OBJ(promiseAll(vm, AS_CLASS(receiver), AS_ARRAY(args[0])));
}

LOX_METHOD(PromiseClass, fulfill) {
    ASSERT_ARG_COUNT("Promise class::fulfill(value)", 1);
    ObjClass* klass = AS_CLASS(receiver);
    if (IS_PROMISE(args[0])) RETURN_VAL(args[0]);
    else {
        ObjPromise* promise = newPromise(vm, PROMISE_FULFILLED, args[0], getObjMethod(vm, receiver, "fulfill"));
        promise->obj.klass = klass;
        RETURN_OBJ(promise);
    }
}

LOX_METHOD(PromiseClass, race) {
    ASSERT_ARG_COUNT("Promise class::race(promises)", 1);
    ASSERT_ARG_TYPE("Promise class::race(promises)", 0, Array);
    RETURN_OBJ(promiseRace(vm, AS_CLASS(receiver), AS_ARRAY(args[0])));
}

LOX_METHOD(PromiseClass, reject) {
    ASSERT_ARG_COUNT("Promise class::reject(exception)", 1);
    ASSERT_ARG_TYPE("Promise class::reject(exception)", 0, Exception);
    ObjClass* klass = AS_CLASS(receiver);
    Value reject;
    tableGet(&klass->methods, copyString(vm, "reject", 6), &reject);
    ObjPromise* promise = newPromise(vm, PROMISE_REJECTED, NIL_VAL, reject);
    promise->obj.klass = klass;
    promise->exception = AS_EXCEPTION(args[0]);
    RETURN_OBJ(promise);
}

LOX_METHOD(Random, __init__) {
    ASSERT_ARG_COUNT("Random::__init__()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    uint64_t seed = (uint64_t)time(NULL);
    pcg32_seed(seed);
    setObjProperty(vm, self, "seed", INT_VAL(abs((int)seed)));
    RETURN_OBJ(receiver);
}

LOX_METHOD(Random, getSeed) {
    ASSERT_ARG_COUNT("Random::getSeed()", 0);
    Value seed = getObjProperty(vm, AS_INSTANCE(receiver), "seed");
    RETURN_VAL(seed);
}

LOX_METHOD(Random, nextBool) {
    ASSERT_ARG_COUNT("Random::nextBool()", 0);
    bool value = pcg32_random_bool();
    RETURN_BOOL(value);
}

LOX_METHOD(Random, nextFloat) {
    ASSERT_ARG_COUNT("Random::nextFloat()", 0);
    double value = pcg32_random_double();
    RETURN_NUMBER(value);
}

LOX_METHOD(Random, nextInt) {
    ASSERT_ARG_COUNT("Random::nextInt()", 0);
    uint32_t value = pcg32_random_int();
    RETURN_INT((int)value);
}

LOX_METHOD(Random, nextIntBounded) {
    ASSERT_ARG_COUNT("Random::nextIntBounded(bound)", 1);
    ASSERT_ARG_TYPE("Random::nextIntBounded(bound)", 0, Int);
    int bound = AS_INT(args[0]);
    if (bound < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Random::nextIntBounded(bound) expects argument 1 to be a non negative integer but got %d.", bound);
    uint32_t value = pcg32_random_int_bounded((uint32_t)AS_INT(args[0]));
    RETURN_INT((int)value);
}

LOX_METHOD(Random, setSeed) {
    ASSERT_ARG_COUNT("Random::setSeed(seed)", 1);
    ASSERT_ARG_TYPE("Random::setSeed(seed)", 0, Int);
    int seed = AS_INT(args[0]);
    if (seed < 0) THROW_EXCEPTION_FMT(clox.std.lang.IllegalArgumentException, "method Random::setSeed(seed) expects argument 1 to be a non negative integer but got %d.", seed);
    pcg32_seed((uint64_t)AS_INT(args[0]));
    setObjProperty(vm, AS_INSTANCE(receiver), "seed", args[0]);
    RETURN_NIL;
}

LOX_METHOD(Regex, __init__) {
    ASSERT_ARG_COUNT("Regex::__init__(pattern)", 1);
    ASSERT_ARG_TYPE("Regex::__init__(pattern)", 0, String);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "pattern", args[0]);
    RETURN_OBJ(self);
}

LOX_METHOD(Regex, match) {
    ASSERT_ARG_COUNT("Regex::match(string)", 1);
    ASSERT_ARG_TYPE("Regex::match(string)", 0, String);
    Value pattern = getObjProperty(vm, AS_INSTANCE(receiver), "pattern");
    int length;
    int index = re_match(AS_CSTRING(pattern), AS_CSTRING(args[0]), &length);
    RETURN_BOOL(index != -1);
}

LOX_METHOD(Regex, replace) {
    ASSERT_ARG_COUNT("Regex::replace(original, replacement)", 2);
    ASSERT_ARG_TYPE("Regex::replace(original, replacement)", 0, String);
    ASSERT_ARG_TYPE("Regex::replace(original, replacement)", 1, String);
    Value pattern = getObjProperty(vm, AS_INSTANCE(receiver), "pattern");
    ObjString* original = AS_STRING(args[0]);
    ObjString* replacement = AS_STRING(args[1]);
    int length;
    int index = re_match(AS_CSTRING(pattern), original->chars, &length);
    if (length == -1) RETURN_OBJ(original);
    ObjString* needle = subString(vm, original, index, index + length);
    RETURN_OBJ(replaceString(vm, original, needle, replacement));
}

LOX_METHOD(Regex, toString) {
    ASSERT_ARG_COUNT("Regex::toString()", 0);
    Value pattern = getObjProperty(vm, AS_INSTANCE(receiver), "pattern");
    RETURN_OBJ(pattern);
}

LOX_METHOD(Timer, __init__) {
    ASSERT_ARG_COUNT("Timer::__init__(closure, delay, interval)", 3);
    ASSERT_ARG_TYPE("Timer::__init__(closure, delay, interval)", 0, Closure);
    ASSERT_ARG_TYPE("Timer::__init__(closure, delay, interval)", 1, Int);
    ASSERT_ARG_TYPE("Timer::__init__(closure, delay, interval)", 2, Int);
    ObjTimer* self = AS_TIMER(receiver);
    TimerData* data = (TimerData*)self->timer->data;
    data->receiver = receiver;
    data->closure = AS_CLOSURE(args[0]);
    data->delay = AS_INT(args[1]);
    data->interval = AS_INT(args[2]);
    RETURN_OBJ(self);
}

LOX_METHOD(Timer, clear) {
    ASSERT_ARG_COUNT("Timer::clear()", 0);
    ObjTimer* self = AS_TIMER(receiver);
    uv_timer_stop(self->timer);
    RETURN_NIL;
}

LOX_METHOD(Timer, isRunning) {
    ASSERT_ARG_COUNT("Timer::isRunning()", 0);
    RETURN_BOOL(AS_TIMER(receiver)->isRunning);
}

LOX_METHOD(Timer, run) {
    ASSERT_ARG_COUNT("Timer::run()", 0);
    ObjTimer* self = AS_TIMER(receiver);
    if (self->isRunning) THROW_EXCEPTION_FMT(clox.std.lang.UnsupportedOperationException, "Timer ID: %d is already running...", self->id);
    else {
        TimerData* data = (TimerData*)self->timer->data;
        uv_timer_init(vm->eventLoop, self->timer);
        uv_timer_start(self->timer, timerRun, data->delay, data->interval);
        self->id = (int)self->timer->start_id;
        RETURN_OBJ(self);
    }
}

LOX_METHOD(Timer, toString) {
    ASSERT_ARG_COUNT("Timer::run()", 0);
    ObjTimer* self = AS_TIMER(receiver);
    TimerData* data = (TimerData*)self->timer->data;
    if (data->delay != 0 && data->interval == 0) RETURN_STRING_FMT("Timer: delay after %dms", data->delay);
    if (data->delay == 0 && data->interval != 0) RETURN_STRING_FMT("Timer: interval at %dms", data->interval);
    RETURN_STRING_FMT("Timer: delay after %dms, interval at %dms", data->delay, data->interval);
}

LOX_METHOD(TimerClass, interval) {
    ASSERT_ARG_COUNT("Timer class::interval(closure, interval)", 2);
    ASSERT_ARG_TYPE("Timer class::interval(closure, interval)", 0, Closure);
    ASSERT_ARG_TYPE("Timer class::interval(closure, interval)", 1, Int);

    ObjClass* self = AS_CLASS(receiver);
    ObjTimer* timer = newTimer(vm, AS_CLOSURE(args[0]), 0, AS_INT(args[1]));
    TimerData* data = (TimerData*)timer->timer->data;
    timer->obj.klass = self;
    data->receiver = OBJ_VAL(timer);
    uv_timer_init(vm->eventLoop, timer->timer);
    uv_timer_start(timer->timer, timerRun, 0, (uint64_t)data->interval);
    timer->id = (int)timer->timer->start_id;
    RETURN_OBJ(timer);
}

LOX_METHOD(TimerClass, timeout) {
    ASSERT_ARG_COUNT("Timer class::timeout(closure, delay)", 2);
    ASSERT_ARG_TYPE("Timer class::timeout(closure, delay)", 0, Closure);
    ASSERT_ARG_TYPE("Timer class::timeout(closure, delay)", 1, Int);

    ObjClass* self = AS_CLASS(receiver);
    ObjTimer* timer = newTimer(vm, AS_CLOSURE(args[0]), AS_INT(args[1]), 0);
    TimerData* data = (TimerData*)timer->timer->data;
    timer->obj.klass = self;
    data->receiver = OBJ_VAL(timer);
    uv_timer_init(vm->eventLoop, timer->timer);
    uv_timer_start(timer->timer, timerRun, (uint64_t)data->delay, 0);
    timer->id = (int)timer->timer->start_id;
    RETURN_OBJ(timer);
}

LOX_METHOD(UUID, __init__) {
    ASSERT_ARG_COUNT("UUID::__init__()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    char buffer[UUID4_LEN];
    uuid4_init();
    uuid4_generate(buffer);
    setObjProperty(vm, self, "buffer", OBJ_VAL(newString(vm, buffer)));
    RETURN_OBJ(self);
}

LOX_METHOD(UUID, toString) {
    ASSERT_ARG_COUNT("UUID::toString()", 0);
    Value buffer = getObjProperty(vm, AS_INSTANCE(receiver), "buffer");
    RETURN_OBJ(buffer);
}

LOX_METHOD(UUIDClass, generate) {
    ASSERT_ARG_COUNT("UUID class::generate()", 0);
    char buffer[UUID4_LEN];
    uuid4_init();
    uuid4_generate(buffer);
    RETURN_OBJ(newString(vm, buffer));
}

LOX_METHOD(UUIDClass, isUUID) {
    ASSERT_ARG_COUNT("UUID class::isUUID(uuid)", 1);
    ASSERT_ARG_TYPE("UUID class::isUUID(uuid)", 0, String);
    RETURN_BOOL(uuidCheckString(AS_STRING(args[0])));
}

LOX_METHOD(UUIDClass, parse) {
    ASSERT_ARG_COUNT("UUID class::parse(uuid)", 1);
    ASSERT_ARG_TYPE("UUID class::parse(uuid)", 0, String);
    ObjString* uuid = AS_STRING(args[0]);
    if (!uuidCheckString(uuid)) RETURN_NIL;
    else {
        ObjInstance* instance = newInstance(vm, AS_CLASS(receiver));
        setObjProperty(vm, instance, "buffer", OBJ_VAL(uuid));
        RETURN_OBJ(instance);
    }
}

void registerUtilPackage(VM* vm) {
    ObjNamespace* utilNamespace = defineNativeNamespace(vm, "util", vm->stdNamespace);
    vm->currentNamespace = utilNamespace;

    ObjClass* comparableTrait = getNativeClass(vm, "clox.std.lang.TComparable");
    ObjClass* dateClass = defineNativeClass(vm, "Date");
    bindSuperclass(vm, dateClass, vm->objectClass);
    bindTrait(vm, dateClass, comparableTrait);
    DEF_INTERCEPTOR(dateClass, Date, INTERCEPTOR_INIT, __init__, 3);
    DEF_METHOD(dateClass, Date, compareTo, 1);
    DEF_METHOD(dateClass, Date, diff, 1);
    DEF_METHOD(dateClass, Date, getTimestamp, 0);
    DEF_METHOD(dateClass, Date, toDateTime, 0);
    DEF_METHOD(dateClass, Date, toString, 0);
    DEF_OPERATOR(dateClass, Date, == , __equal__, 1);
    DEF_OPERATOR(dateClass, Date, > , __greater__, 1);
    DEF_OPERATOR(dateClass, Date, < , __less__, 1);
    DEF_OPERATOR(dateClass, Date, +, __add__, 1);
    DEF_OPERATOR(dateClass, Date, -, __subtract__, 1);

    ObjClass* dateMetaclass = dateClass->obj.klass;
    setClassProperty(vm, dateClass, "now", OBJ_VAL(dateObjNow(vm, dateClass)));
    DEF_METHOD(dateMetaclass, DateClass, fromTimestamp, 1);
    DEF_METHOD(dateMetaclass, DateClass, parse, 1);

    ObjClass* dateTimeClass = defineNativeClass(vm, "DateTime");
    bindSuperclass(vm, dateTimeClass, dateClass);
    bindTrait(vm, dateTimeClass, comparableTrait);
    DEF_INTERCEPTOR(dateTimeClass, DateTime, INTERCEPTOR_INIT, __init__, 6);
    DEF_METHOD(dateTimeClass, DateTime, compareTo, 1);
    DEF_METHOD(dateTimeClass, DateTime, diff, 1);
    DEF_METHOD(dateTimeClass, DateTime, getTimestamp, 0);
    DEF_METHOD(dateTimeClass, DateTime, toDate, 0);
    DEF_METHOD(dateTimeClass, DateTime, toString, 0);
    DEF_OPERATOR(dateTimeClass, DateTime, == , __equal__, 1);
    DEF_OPERATOR(dateTimeClass, DateTime, > , __greater__, 1);
    DEF_OPERATOR(dateTimeClass, DateTime, < , __less__, 1);
    DEF_OPERATOR(dateTimeClass, DateTime, +, __add__, 1);
    DEF_OPERATOR(dateTimeClass, DateTime, -, __subtract__, 1);

    ObjClass* dateTimeMetaClass = dateTimeClass->obj.klass;
    setClassProperty(vm, dateTimeClass, "now", OBJ_VAL(dateTimeObjNow(vm, dateTimeClass)));
    DEF_METHOD(dateTimeMetaClass, DateTimeClass, fromTimestamp, 1);
    DEF_METHOD(dateTimeMetaClass, DateTimeClass, parse, 1);

    ObjClass* durationClass = defineNativeClass(vm, "Duration");
    bindSuperclass(vm, durationClass, vm->objectClass);
    bindTrait(vm, durationClass, comparableTrait);
    DEF_INTERCEPTOR(durationClass, Duration, INTERCEPTOR_INIT, __init__, 4);
    DEF_METHOD(durationClass, Duration, compareTo, 1);
    DEF_METHOD(durationClass, Duration, getTotalSeconds, 0);
    DEF_METHOD(durationClass, Duration, toString, 0);
    DEF_OPERATOR(durationClass, Duration, == , __equal__, 1);
    DEF_OPERATOR(durationClass, Duration, > , __greater__, 1);
    DEF_OPERATOR(durationClass, Duration, < , __less__, 1);
    DEF_OPERATOR(durationClass, Duration, +, __add__, 1);
    DEF_OPERATOR(durationClass, Duration, -, __subtract__, 1);

    ObjClass* durationMetaclass = durationClass->obj.klass;
    DEF_METHOD(durationMetaclass, DurationClass, ofDays, 1);
    DEF_METHOD(durationMetaclass, DurationClass, ofHours, 1);
    DEF_METHOD(durationMetaclass, DurationClass, ofMinutes, 1);
    DEF_METHOD(durationMetaclass, DurationClass, ofSeconds, 1);

    vm->promiseClass = defineNativeClass(vm, "Promise");
    bindSuperclass(vm, vm->promiseClass, vm->objectClass);
    vm->promiseClass->classType = OBJ_PROMISE;
    DEF_INTERCEPTOR(vm->promiseClass, Promise, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(vm->promiseClass, Promise, catch, 1);
    DEF_METHOD(vm->promiseClass, Promise, catchAll, 1);
    DEF_METHOD(vm->promiseClass, Promise, finally, 1);
    DEF_METHOD(vm->promiseClass, Promise, fulfill, 1);
    DEF_METHOD(vm->promiseClass, Promise, isResolved, 0);
    DEF_METHOD(vm->promiseClass, Promise, raceAll, 1);
    DEF_METHOD(vm->promiseClass, Promise, reject, 1);
    DEF_METHOD(vm->promiseClass, Promise, then, 1);
    DEF_METHOD(vm->promiseClass, Promise, thenAll, 1);
    DEF_METHOD(vm->promiseClass, Promise, thenChain, 1);
    DEF_METHOD(vm->promiseClass, Promise, thenFulfill, 0);

    ObjClass* promiseMetaclass = vm->promiseClass->obj.klass;
    setClassProperty(vm, vm->promiseClass, "statePending", INT_VAL(PROMISE_PENDING));
    setClassProperty(vm, vm->promiseClass, "stateFulfilled", INT_VAL(PROMISE_FULFILLED));
    setClassProperty(vm, vm->promiseClass, "stateRejected", INT_VAL(PROMISE_REJECTED));
    DEF_METHOD(promiseMetaclass, PromiseClass, all, 1);
    DEF_METHOD(promiseMetaclass, PromiseClass, fulfill, 1);
    DEF_METHOD(promiseMetaclass, PromiseClass, race, 1);
    DEF_METHOD(promiseMetaclass, PromiseClass, reject, 1);

    ObjClass* randomClass = defineNativeClass(vm, "Random");
    bindSuperclass(vm, randomClass, vm->objectClass);
    DEF_INTERCEPTOR(randomClass, Random, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(randomClass, Random, getSeed, 0);
    DEF_METHOD(randomClass, Random, nextBool, 0);
    DEF_METHOD(randomClass, Random, nextFloat, 0);
    DEF_METHOD(randomClass, Random, nextInt, 0);
    DEF_METHOD(randomClass, Random, nextIntBounded, 1);
    DEF_METHOD(randomClass, Random, setSeed, 1);

    ObjClass* regexClass = defineNativeClass(vm, "Regex");
    bindSuperclass(vm, regexClass, vm->objectClass);
    DEF_INTERCEPTOR(regexClass, Regex, INTERCEPTOR_INIT, __init__, 1);
    DEF_METHOD(regexClass, Regex, match, 1);
    DEF_METHOD(regexClass, Regex, replace, 2);
    DEF_METHOD(regexClass, Regex, toString, 0);

    vm->timerClass = defineNativeClass(vm, "Timer");
    bindSuperclass(vm, vm->timerClass, vm->objectClass);
    vm->timerClass->classType = OBJ_TIMER;
    DEF_INTERCEPTOR(vm->timerClass, Timer, INTERCEPTOR_INIT, __init__, 3);
    DEF_METHOD(vm->timerClass, Timer, clear, 0);
    DEF_METHOD(vm->timerClass, Timer, isRunning, 0);
    DEF_METHOD(vm->timerClass, Timer, run, 0);
    DEF_METHOD(vm->timerClass, Timer, toString, 0);

    ObjClass* timerMetaclass = vm->timerClass->obj.klass;
    DEF_METHOD(timerMetaclass, TimerClass, interval, 2);
    DEF_METHOD(timerMetaclass, TimerClass, timeout, 2);

    ObjClass* uuidClass = defineNativeClass(vm, "UUID");
    bindSuperclass(vm, uuidClass, vm->objectClass);
    DEF_INTERCEPTOR(uuidClass, UUID, INTERCEPTOR_INIT, __init__, 0);
    DEF_METHOD(uuidClass, UUID, toString, 0);

    ObjClass* uuidMetaclass = uuidClass->obj.klass;
    setClassProperty(vm, uuidClass, "length", INT_VAL(UUID4_LEN));
    setClassProperty(vm, uuidClass, "version", INT_VAL(4));
    DEF_METHOD(uuidMetaclass, UUIDClass, generate, 0);
    DEF_METHOD(uuidMetaclass, UUIDClass, isUUID, 1);
    DEF_METHOD(uuidMetaclass, UUIDClass, parse, 1);

    ObjClass* runtimeExceptionClass = getNativeClass(vm, "clox.std.lang.RuntimeException");
    defineNativeException(vm, "DateFormatException", runtimeExceptionClass);

    vm->currentNamespace = vm->rootNamespace;
}