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
    ObjInstance* dateTime = newInstance(vm, getNativeClass(vm, "DateTime"));
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

LOX_METHOD(Date, after) {
    ASSERT_ARG_COUNT("Date::after(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::after(date)", 0, Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp > timestamp2);
}

LOX_METHOD(Date, before) {
    ASSERT_ARG_COUNT("Date::before(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::before(date)", 0, Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp < timestamp2);
}

LOX_METHOD(Date, diff) {
    ASSERT_ARG_COUNT("Date::diff(date)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::diff(date)", 0, Date);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_NUMBER(timestamp - timestamp2);
}

LOX_METHOD(Date, getTimestamp) {
    ASSERT_ARG_COUNT("Date::getTimestamp()", 0);
    RETURN_NUMBER(dateObjGetTimestamp(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Date, init) {
    ASSERT_ARG_COUNT("Date::init(year, month, day)", 3);
    ASSERT_ARG_TYPE("Date::init(year, month, day)", 0, Int);
    ASSERT_ARG_TYPE("Date::init(year, month, day)", 1, Int);
    ASSERT_ARG_TYPE("Date::init(year, month, day)", 2, Int);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "year", args[0]);
    setObjProperty(vm, self, "month", args[1]);
    setObjProperty(vm, self, "day", args[2]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Date, minus) {
    ASSERT_ARG_COUNT("Date::minus(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::minus(duration)", 0, Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateObjGetTimestamp(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* date = dateObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(date);
}

LOX_METHOD(Date, plus) {
    ASSERT_ARG_COUNT("Date::plus(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Date::plus(duration)", 0, Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateObjGetTimestamp(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* date = dateObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(date);
}

LOX_METHOD(Date, toDateTime) {
    ASSERT_ARG_COUNT("Date::toDateTime()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* dateTime = newInstance(vm, getNativeClass(vm, "DateTime"));
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
        raiseError(vm, "Failed to parse Date from input string, please make sure the date has format MM/DD/YYYY.");
        RETURN_NIL;
    }
}

LOX_METHOD(DateTime, after) {
    ASSERT_ARG_COUNT("DateTime::after(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::after(dateTime)", 0, DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp > timestamp2);
}

LOX_METHOD(DateTime, before) {
    ASSERT_ARG_COUNT("DateTime::before(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::before(dateTime)", 0, DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp < timestamp2);
}

LOX_METHOD(DateTime, diff) {
    ASSERT_ARG_COUNT("DateTime::diff(dateTime)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::diff(dateTime)", 0, DateTime);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_NUMBER(timestamp - timestamp2);
}

LOX_METHOD(DateTime, getTimestamp) {
    ASSERT_ARG_COUNT("DateTime::getTimestamp()", 0);
    RETURN_NUMBER(dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(DateTime, init) {
    ASSERT_ARG_COUNT("DateTime::init(year, month, day, hour, minute, second)", 6);
    ASSERT_ARG_TYPE("DateTime::init(year, month, day, hour, minute, second)", 0, Int);
    ASSERT_ARG_TYPE("DateTime::init(year, month, day, hour, minute, second)", 1, Int);
    ASSERT_ARG_TYPE("DateTime::init(year, month, day, hour, minute, second)", 2, Int);
    ASSERT_ARG_TYPE("DateTime::init(year, month, day, hour, minute, second)", 3, Int);
    ASSERT_ARG_TYPE("DateTime::init(year, month, day, hour, minute, second)", 4, Int);
    ASSERT_ARG_TYPE("DateTime::init(year, month, day, hour, minute, second)", 5, Int);
    
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "year", args[0]);
    setObjProperty(vm, self, "month", args[1]);
    setObjProperty(vm, self, "day", args[2]);
    setObjProperty(vm, self, "hour", args[3]);
    setObjProperty(vm, self, "minute", args[4]);
    setObjProperty(vm, self, "second", args[5]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(DateTime, minus) {
    ASSERT_ARG_COUNT("DateTime::minus(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::minus(duration)", 0, Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateTimeObjGetTimestamp(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* dateTime = dateTimeObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(dateTime);
}

LOX_METHOD(DateTime, plus) {
    ASSERT_ARG_COUNT("DateTime::plus(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("DateTime::plus(duration)", 0, Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateTimeObjGetTimestamp(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* dateTime = dateTimeObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(dateTime);
}

LOX_METHOD(DateTime, toDate) {
    ASSERT_ARG_COUNT("DateTime::toDate()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* date = newInstance(vm, getNativeClass(vm, "Date"));
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
        raiseError(vm, "Failed to parse DateTime from input string, please make sure the date has format MM/DD/YYYY.");
        RETURN_NIL;
    }
}

LOX_METHOD(Duration, getTotalSeconds) {
    ASSERT_ARG_COUNT("Duration::getTotalSeconds()", 0);
    RETURN_NUMBER(durationTotalSeconds(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Duration, init) {
    ASSERT_ARG_COUNT("Duration::init(days, hours, minutes, seconds)", 4);
    ASSERT_ARG_TYPE("Duration::init(days, hours, minutes, seconds)", 0, Int);
    ASSERT_ARG_TYPE("Duration::init(days, hours, minutes, seconds)", 1, Int);
    ASSERT_ARG_TYPE("Duration::init(days, hours, minutes, seconds)", 2, Int);
    ASSERT_ARG_TYPE("Duration::init(days, hours, minutes, seconds)", 3, Int);

    assertNumberNonNegative(vm, "Duration::init(days, hours, minutes, seconds)", AS_NUMBER(args[0]), 0);
    assertNumberNonNegative(vm, "Duration::init(days, hours, minutes, seconds)", AS_NUMBER(args[1]), 1);
    assertNumberNonNegative(vm, "Duration::init(days, hours, minutes, seconds)", AS_NUMBER(args[2]), 2);
    assertNumberNonNegative(vm, "Duration::init(days, hours, minutes, seconds)", AS_NUMBER(args[3]), 3);

    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromArgs(duration, args);
    durationObjInit(vm, duration, self);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Duration, minus) {
    ASSERT_ARG_COUNT("Duration::minus(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::minus(duration)", 0, Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromSeconds(duration, durationTotalSeconds(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0])));
    ObjInstance* object = newInstance(vm, self->obj.klass);
    durationObjInit(vm, duration, object);
    RETURN_OBJ(object);
}

LOX_METHOD(Duration, plus) {
    ASSERT_ARG_COUNT("Duration::plus(duration)", 1);
    ASSERT_ARG_INSTANCE_OF("Duration::plus(duration)", 0, Duration);
    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromSeconds(duration, durationTotalSeconds(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0])));
    ObjInstance* object = newInstance(vm, self->obj.klass);
    durationObjInit(vm, duration, object);
    RETURN_OBJ(object);
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

LOX_METHOD(DurationClass, ofDays) {
    ASSERT_ARG_COUNT("Duration class::ofDays(days)", 1);
    ASSERT_ARG_TYPE("Duration class::ofDays(days)", 0, Int);
    assertNumberNonNegative(vm, "Duration class::ofDays(days)", AS_NUMBER(args[0]), 0);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { AS_INT(args[0]), 0, 0, 0 };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(DurationClass, ofHours) {
    ASSERT_ARG_COUNT("Duration class::ofHours(hours)", 1);
    ASSERT_ARG_TYPE("Duration class::ofHours(hours)", 0, Int);
    assertNumberNonNegative(vm, "Duration class::ofHours(hours)", AS_NUMBER(args[0]), 0);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { 0, AS_INT(args[0]), 0, 0 };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(DurationClass, ofMinutes) {
    ASSERT_ARG_COUNT("Duration class::ofMinutes(minutes)", 1);
    ASSERT_ARG_TYPE("Duration class::ofMinutes(minutes)", 0, Int);
    assertNumberNonNegative(vm, "Duration class::ofMinutes(minutes)", AS_NUMBER(args[0]), 0);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { 0, 0, AS_INT(args[0]), 0 };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(DurationClass, ofSeconds) {
    ASSERT_ARG_COUNT("Duration class::ofSeconds(seconds)", 1);
    ASSERT_ARG_TYPE("Duration class::ofSeconds(seconds)", 0, Int);
    assertNumberNonNegative(vm, "Duration class::ofSeconds(seconds)", AS_NUMBER(args[0]), 0);

    ObjClass* self = AS_CLASS(receiver);
    ObjInstance* instance = newInstance(vm, self);
    push(vm, OBJ_VAL(instance));
    int duration[4] = { 0, 0, 0, AS_INT(args[0]) };
    durationObjInit(vm, duration, instance);
    pop(vm);
    RETURN_OBJ(instance);
}

LOX_METHOD(Random, getSeed) {
    ASSERT_ARG_COUNT("Random::getSeed()", 0);
    Value seed = getObjProperty(vm, AS_INSTANCE(receiver), "seed");
    RETURN_VAL(seed);
}

LOX_METHOD(Random, init) {
    ASSERT_ARG_COUNT("Random::init()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    uint64_t seed = (uint64_t)time(NULL);
    pcg32_seed(seed);
    setObjProperty(vm, self, "seed", INT_VAL(abs((int)seed)));
    RETURN_OBJ(receiver);
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
    assertNumberNonNegative(vm, "Random::nextIntBounded(bound)", AS_NUMBER(args[0]), 0);
    uint32_t value = pcg32_random_int_bounded((uint32_t)AS_INT(args[0]));
    RETURN_INT((int)value);
}

LOX_METHOD(Random, setSeed) {
    ASSERT_ARG_COUNT("Random::setSeed(seed)", 1);
    ASSERT_ARG_TYPE("Random::setSeed(seed)", 0, Int);
    assertNumberNonNegative(vm, "Random::setSeed(seed)", AS_NUMBER(args[0]), 0);
    pcg32_seed((uint64_t)AS_INT(args[0]));
    setObjProperty(vm, AS_INSTANCE(receiver), "seed", args[0]);
    RETURN_NIL;
}

LOX_METHOD(Regex, init) {
    ASSERT_ARG_COUNT("Regex::init(pattern)", 1);
    ASSERT_ARG_TYPE("Regex::init(pattern)", 0, String);
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

LOX_METHOD(UUID, init) {
    ASSERT_ARG_COUNT("UUID::init()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    char buffer[UUID4_LEN];
    uuid4_init();
    uuid4_generate(buffer);
    setObjProperty(vm, self, "buffer", newString(vm, buffer));
    RETURN_OBJ(self);
}

LOX_METHOD(UUID, toString) {
    ASSERT_ARG_COUNT("UUID::toString()", 0);
    Value buffer = getObjProperty(vm, AS_INSTANCE(receiver), "buffer");
    RETURN_OBJ(buffer);
}

LOX_METHOD(UUIDClass, generate) {
    ASSERT_ARG_COUNT("UUID class::generate()", 0);
    ObjInstance* instance = newInstance(vm, AS_CLASS(receiver));
    char buffer[UUID4_LEN];
    uuid4_init();
    uuid4_generate(buffer);
    setObjProperty(vm, instance, "buffer", newString(vm, buffer));
    RETURN_OBJ(instance);
}

LOX_METHOD(UUIDClass, isUUID) {
    ASSERT_ARG_COUNT("UUID class::isUUID(uuid)", 1);
    ASSERT_ARG_TYPE("UUID class::isUUID(uuid)", 0, String);
    int length;
    int index = re_match("[a-f0-9]{8}-?[a-f0-9]{4}-?4[a-f0-9]{3}-?[89ab][a-f0-9]{3}-?[a-f0-9]{12}", AS_CSTRING(args[0]), &length);
    RETURN_BOOL(index != -1);
}

LOX_METHOD(UUIDClass, parse) {
    ASSERT_ARG_COUNT("UUID class::parse(uuid)", 1);
    ASSERT_ARG_TYPE("UUID class::parse(uuid)", 0, String);
    int length;
    int index = re_match("[a-f0-9]{8}-?[a-f0-9]{4}-?4[a-f0-9]{3}-?[89ab][a-f0-9]{3}-?[a-f0-9]{12}", AS_CSTRING(args[0]), &length);
    RETURN_BOOL(index == -1 ? NIL_VAL : AS_STRING(args[0]));
}

void registerUtilPackage(VM* vm) {
    ObjClass* dateClass = defineNativeClass(vm, "Date");
    bindSuperclass(vm, dateClass, vm->objectClass);
    DEF_METHOD(dateClass, Date, after, 1);
    DEF_METHOD(dateClass, Date, before, 1);
    DEF_METHOD(dateClass, Date, diff, 1);
    DEF_METHOD(dateClass, Date, getTimestamp, 0);
    DEF_METHOD(dateClass, Date, init, 3);
    DEF_METHOD(dateClass, Date, minus, 1);
    DEF_METHOD(dateClass, Date, plus, 1);
    DEF_METHOD(dateClass, Date, toDateTime, 0);
	DEF_METHOD(dateClass, Date, toString, 0);

    ObjClass* dateMetaclass = dateClass->obj.klass;
    setClassProperty(vm, dateClass, "now", OBJ_VAL(dateObjNow(vm, dateClass)));
    DEF_METHOD(dateMetaclass, DateClass, fromTimestamp, 1);
    DEF_METHOD(dateMetaclass, DateClass, parse, 1);
   
    ObjClass* dateTimeClass = defineNativeClass(vm, "DateTime");
    bindSuperclass(vm, dateTimeClass, dateClass);
    DEF_METHOD(dateTimeClass, DateTime, after, 1);
    DEF_METHOD(dateTimeClass, DateTime, before, 1);
    DEF_METHOD(dateTimeClass, DateTime, diff, 1);
    DEF_METHOD(dateTimeClass, DateTime, getTimestamp, 0);
    DEF_METHOD(dateTimeClass, DateTime, init, 6);
    DEF_METHOD(dateTimeClass, DateTime, minus, 1);
    DEF_METHOD(dateTimeClass, DateTime, plus, 1);
    DEF_METHOD(dateTimeClass, DateTime, toDate, 0);
    DEF_METHOD(dateTimeClass, DateTime, toString, 0);

    ObjClass* dateTimeMetaClass = dateTimeClass->obj.klass;
    setClassProperty(vm, dateTimeClass, "now", OBJ_VAL(dateTimeObjNow(vm, dateTimeClass)));
    DEF_METHOD(dateTimeMetaClass, DateTimeClass, fromTimestamp, 1);
    DEF_METHOD(dateTimeMetaClass, DateTimeClass, parse, 1);
    
    ObjClass* durationClass = defineNativeClass(vm, "Duration");
    bindSuperclass(vm, durationClass, vm->objectClass);
    DEF_METHOD(durationClass, Duration, getTotalSeconds, 0);
    DEF_METHOD(durationClass, Duration, init, 4);
    DEF_METHOD(durationClass, Duration, minus, 1);
    DEF_METHOD(durationClass, Duration, plus, 1);
    DEF_METHOD(durationClass, Duration, toString, 0);

    ObjClass* durationMetaclass = durationClass->obj.klass;
    DEF_METHOD(durationMetaclass, DurationClass, ofDays, 1);
    DEF_METHOD(durationMetaclass, DurationClass, ofHours, 1);
    DEF_METHOD(durationMetaclass, DurationClass, ofMinutes, 1);
    DEF_METHOD(durationMetaclass, DurationClass, ofSeconds, 1);

    ObjClass* randomClass = defineNativeClass(vm, "Random");
    bindSuperclass(vm, randomClass, vm->objectClass);
    DEF_METHOD(randomClass, Random, getSeed, 0);
    DEF_METHOD(randomClass, Random, init, 0);
    DEF_METHOD(randomClass, Random, nextBool, 0);
    DEF_METHOD(randomClass, Random, nextFloat, 0);
    DEF_METHOD(randomClass, Random, nextInt, 0);
    DEF_METHOD(randomClass, Random, nextIntBounded, 1);
    DEF_METHOD(randomClass, Random, setSeed, 1);

    ObjClass* regexClass = defineNativeClass(vm, "Regex");
    bindSuperclass(vm, regexClass, vm->objectClass);
    DEF_METHOD(regexClass, Regex, init, 1);
    DEF_METHOD(regexClass, Regex, match, 1);
    DEF_METHOD(regexClass, Regex, replace, 2);
    DEF_METHOD(regexClass, Regex, toString, 0);

    ObjClass* uuidClass = defineNativeClass(vm, "UUID");
    bindSuperclass(vm, uuidClass, vm->objectClass);
    DEF_METHOD(uuidClass, UUID, init, 0);
    DEF_METHOD(uuidClass, UUID, toString, 0);

    ObjClass* uuidMetaclass = uuidClass->obj.klass;
    setClassProperty(vm, uuidClass, "length", INT_VAL(UUID4_LEN));
    DEF_METHOD(uuidMetaclass, UUIDClass, generate, 0);
    DEF_METHOD(uuidMetaclass, UUIDClass, isUUID, 1);
    DEF_METHOD(uuidMetaclass, UUIDClass, parse, 1);
}