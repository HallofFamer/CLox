#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "../inc/pcg.h"
#include "../inc/regex.h"
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
    setObjProperty(vm, dateTime, "year", getObjProperty(vm, self, "year"));
    setObjProperty(vm, dateTime, "month", getObjProperty(vm, self, "month"));
    setObjProperty(vm, dateTime, "day", getObjProperty(vm, self, "day"));
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
    setObjProperty(vm, date, "year", getObjProperty(vm, self, "year"));
    setObjProperty(vm, date, "month", getObjProperty(vm, self, "month"));
    setObjProperty(vm, date, "day", getObjProperty(vm, self, "day"));
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

LOX_METHOD(Dictionary, clear) {
    ASSERT_ARG_COUNT("Dictionary::clear()", 0);
    freeTable(vm, &AS_DICTIONARY(receiver)->table);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, clone) {
    ASSERT_ARG_COUNT("Dictionary::clone()", 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    RETURN_OBJ(copyDictionary(vm, self->table));
}

LOX_METHOD(Dictionary, containsKey) {
    ASSERT_ARG_COUNT("Dictionary::containsKey(key)", 1);
    ASSERT_ARG_TYPE("Dictionary::containsKey(key)", 0, String);
    RETURN_BOOL(tableContainsKey(&AS_DICTIONARY(receiver)->table, AS_STRING(args[0])));
}

LOX_METHOD(Dictionary, containsValue) {
    ASSERT_ARG_COUNT("Dictionary::containsValue(value)", 1);
    RETURN_BOOL(tableContainsValue(&AS_DICTIONARY(receiver)->table, args[0]));
}

LOX_METHOD(Dictionary, equals) {
    ASSERT_ARG_COUNT("Dictionary::equals(other)", 1);
    if (!IS_DICTIONARY(args[0])) RETURN_FALSE;
    RETURN_BOOL(tablesEqual(&AS_DICTIONARY(receiver)->table, &AS_DICTIONARY(args[0])->table));
}

LOX_METHOD(Dictionary, getAt) {
    ASSERT_ARG_COUNT("Dictionary::getAt(key)", 1);
    ASSERT_ARG_TYPE("Dictionary::getAt(key)", 0, String);
    Value value;
    bool valueExists = tableGet(&AS_DICTIONARY(receiver)->table, AS_STRING(args[0]), &value);
    if (!valueExists) RETURN_NIL;
    RETURN_VAL(value);
}

LOX_METHOD(Dictionary, init) {
    ASSERT_ARG_COUNT("Dictionary::init()", 0);
    RETURN_OBJ(newDictionary(vm));
}

LOX_METHOD(Dictionary, isEmpty) {
    ASSERT_ARG_COUNT("Dictionary::isEmpty()", 0);
    RETURN_BOOL(AS_DICTIONARY(receiver)->table.count == 0);
}

LOX_METHOD(Dictionary, length) {
    ASSERT_ARG_COUNT("Dictionary::length()", 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    RETURN_INT(tableLength(&AS_DICTIONARY(receiver)->table));
}

LOX_METHOD(Dictionary, putAll) {
    ASSERT_ARG_COUNT("Dictionary::putAll(dictionary)", 1);
    ASSERT_ARG_TYPE("Dictionary::putAll(dictionary)", 0, Dictionary);
    tableAddAll(vm, &AS_DICTIONARY(args[0])->table, &AS_DICTIONARY(receiver)->table);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, putAt) {
    ASSERT_ARG_COUNT("Dictionary::putAt(key, value)", 2);
    ASSERT_ARG_TYPE("Dictionary::putAt(key, valuue)", 0, String);
    tableSet(vm, &AS_DICTIONARY(receiver)->table, AS_STRING(args[0]), args[1]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, removeAt) {
    ASSERT_ARG_COUNT("Dictionary::removeAt(key)", 1);
    ASSERT_ARG_TYPE("Dictionary::removeAt(key)", 0, String);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    ObjString* key = AS_STRING(args[0]);
    Value value;
    
    bool keyExists = tableGet(&self->table, key, &value);
    if (!keyExists) RETURN_NIL;
    tableDelete(&self->table, key);
    RETURN_VAL(value);
}

LOX_METHOD(Dictionary, toString) {
    ASSERT_ARG_COUNT("Dictionary::toString()", 0);
    RETURN_OBJ(tableToString(vm, &AS_DICTIONARY(receiver)->table));
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

LOX_METHOD(List, add) {
    ASSERT_ARG_COUNT("List::add(element)", 1);
    valueArrayWrite(vm, &AS_LIST(receiver)->elements, args[0]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, addAll) {
    ASSERT_ARG_COUNT("List::add(list)", 1);
    ASSERT_ARG_TYPE("List::add(list)", 0, List);
    valueArrayAddAll(vm, &AS_LIST(args[0])->elements, &AS_LIST(receiver)->elements);
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, clear) {
    ASSERT_ARG_COUNT("List::clear()", 0);
    freeValueArray(vm, &AS_LIST(receiver)->elements);
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, clone) {
    ASSERT_ARG_COUNT("List::clone()", 0);
    ObjList* self = AS_LIST(receiver);
    RETURN_OBJ(copyList(vm, self->elements, 0, self->elements.count));
}

LOX_METHOD(List, contains) {
    ASSERT_ARG_COUNT("List::contains(element)", 1);
    RETURN_BOOL(valueArrayFirstIndex(vm, &AS_LIST(receiver)->elements, args[0]) != -1);
}

LOX_METHOD(List, equals) {
    ASSERT_ARG_COUNT("List::equals(other)", 1);
    if (!IS_LIST(args[0])) RETURN_FALSE;
    RETURN_BOOL(valueArraysEqual(&AS_LIST(receiver)->elements, &AS_LIST(args[0])->elements));
}

LOX_METHOD(List, getAt) {
    ASSERT_ARG_COUNT("List::getAt(index)", 1);
    ASSERT_ARG_TYPE("List::getAt(index)", 0, Int);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::getAt(index)", index, 0, self->elements.count - 1, 0);
    RETURN_VAL(self->elements.values[index]);
}

LOX_METHOD(List, indexOf) {
    ASSERT_ARG_COUNT("List::indexOf(element)", 1);
    ObjList* self = AS_LIST(receiver);
    if (self->elements.count == 0) return -1;
    RETURN_INT(valueArrayFirstIndex(vm, &self->elements, args[0]));
}

LOX_METHOD(List, init) {
    ASSERT_ARG_COUNT("List::init()", 0);
    RETURN_OBJ(newList(vm));
}

LOX_METHOD(List, insertAt) {
    ASSERT_ARG_COUNT("List::insertAt(index, element)", 2);
    ASSERT_ARG_TYPE("List::insertAt(index, element)", 0, Int);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::insertAt(index)", index, 0, self->elements.count, 0);
    valueArrayInsert(vm, &self->elements, index, args[1]);
    RETURN_VAL(args[1]);
}

LOX_METHOD(List, isEmpty) {
    ASSERT_ARG_COUNT("List::isEmpty()", 0);
    RETURN_BOOL(AS_LIST(receiver)->elements.count == 0);
}

LOX_METHOD(List, lastIndexOf) {
    ASSERT_ARG_COUNT("List::indexOf(element)", 1);
    ObjList* self = AS_LIST(receiver);
    if (self->elements.count == 0) return -1;
    RETURN_INT(valueArrayLastIndex(vm, &self->elements, args[0]));
}

LOX_METHOD(List, length) {
    ASSERT_ARG_COUNT("List::length()", 0);
    RETURN_INT(AS_LIST(receiver)->elements.count);
}

LOX_METHOD(List, putAt) {
    ASSERT_ARG_COUNT("List::putAt(index, element)", 2);
    ASSERT_ARG_TYPE("List::putAt(index, element)", 0, Int);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::putAt(index)", index, 0, self->elements.count, 0);
    self->elements.values[index] = args[1];
    if (index == self->elements.count) self->elements.count++;
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, remove) {
    ASSERT_ARG_COUNT("List::remove(element)", 1);
    ObjList* self = AS_LIST(receiver);
    int index = valueArrayFirstIndex(vm, &self->elements, args[0]);
    if (index == -1) RETURN_FALSE;
    valueArrayDelete(vm, &self->elements, index);
    RETURN_TRUE;
}

LOX_METHOD(List, removeAt) {
    ASSERT_ARG_COUNT("List::removeAt(index)", 1);
    ASSERT_ARG_TYPE("List::removeAt(index)", 0, Int);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::removeAt(index)", AS_INT(args[0]), 0, self->elements.count - 1, 0);
    Value element = valueArrayDelete(vm, &self->elements, index);
    RETURN_VAL(element);
}

LOX_METHOD(List, subList) {
    ASSERT_ARG_COUNT("List::subList(from, to)", 2);
    ASSERT_ARG_TYPE("List::subList(from, to)", 0, Int);
    ASSERT_ARG_TYPE("List::subList(from, to)", 1, Int);
    ObjList* self = AS_LIST(receiver);
    int fromIndex = AS_INT(args[0]);
    int toIndex = AS_INT(args[1]);

    assertIntWithinRange(vm, "List::subList(from, to)", fromIndex, 0, self->elements.count, 0);
    assertIntWithinRange(vm, "List::subList(from, to", toIndex, fromIndex, self->elements.count, 1);
    RETURN_OBJ(copyList(vm, self->elements, fromIndex, toIndex));
}

LOX_METHOD(List, toString) {
    ASSERT_ARG_COUNT("List::toString()", 0);
    RETURN_OBJ(valueArrayToString(vm, &AS_LIST(receiver)->elements));
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

void registerUtilPackage(VM* vm) {
    vm->listClass = defineNativeClass(vm, "List");
    bindSuperclass(vm, vm->listClass, vm->objectClass);
    DEF_METHOD(vm->listClass, List, add, 1);
    DEF_METHOD(vm->listClass, List, addAll, 1);
    DEF_METHOD(vm->listClass, List, clear, 0);
    DEF_METHOD(vm->listClass, List, clone, 0);
    DEF_METHOD(vm->listClass, List, contains, 1);
    DEF_METHOD(vm->listClass, List, equals, 1);
    DEF_METHOD(vm->listClass, List, getAt, 1);
    DEF_METHOD(vm->listClass, List, indexOf, 1);
    DEF_METHOD(vm->listClass, List, init, 0);
    DEF_METHOD(vm->listClass, List, insertAt, 2);
    DEF_METHOD(vm->listClass, List, isEmpty, 0);
    DEF_METHOD(vm->listClass, List, lastIndexOf, 1);
    DEF_METHOD(vm->listClass, List, length, 0);
    DEF_METHOD(vm->listClass, List, putAt, 2);
    DEF_METHOD(vm->listClass, List, remove, 1);
    DEF_METHOD(vm->listClass, List, removeAt, 1);
    DEF_METHOD(vm->listClass, List, subList, 2);
    DEF_METHOD(vm->listClass, List, toString, 0);

    vm->dictionaryClass = defineNativeClass(vm, "Dictionary");
    bindSuperclass(vm, vm->dictionaryClass, vm->objectClass);
    DEF_METHOD(vm->dictionaryClass, Dictionary, clear, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, clone, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, containsKey, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, containsValue, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, equals, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, getAt, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, init, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, isEmpty, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, length, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, putAll, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, putAt, 2);
    DEF_METHOD(vm->dictionaryClass, Dictionary, removeAt, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, toString, 0);
    
    ObjClass* randomClass = defineNativeClass(vm, "Random");
    bindSuperclass(vm, randomClass, vm->objectClass);
    DEF_METHOD(randomClass, Random, getSeed, 0);
    DEF_METHOD(randomClass, Random, init, 0);
    DEF_METHOD(randomClass, Random, nextBool, 0);
    DEF_METHOD(randomClass, Random, nextFloat, 0);
    DEF_METHOD(randomClass, Random, nextInt, 0);
    DEF_METHOD(randomClass, Random, nextIntBounded, 1);
    DEF_METHOD(randomClass, Random, setSeed, 1);

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
    
    ObjClass* durationClass = defineNativeClass(vm, "Duration");
    bindSuperclass(vm, durationClass, vm->objectClass);
    DEF_METHOD(durationClass, Duration, getTotalSeconds, 0);
    DEF_METHOD(durationClass, Duration, init, 4);
    DEF_METHOD(durationClass, Duration, minus, 1);
    DEF_METHOD(durationClass, Duration, plus, 1);
    DEF_METHOD(durationClass, Duration, toString, 0);

    ObjClass* regexClass = defineNativeClass(vm, "Regex");
    bindSuperclass(vm, regexClass, vm->objectClass);
    DEF_METHOD(regexClass, Regex, init, 1);
    DEF_METHOD(regexClass, Regex, match, 1);
    DEF_METHOD(regexClass, Regex, replace, 2);
    DEF_METHOD(regexClass, Regex, toString, 0);
}