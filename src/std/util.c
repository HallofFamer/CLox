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
    setObjProperty(vm, date, "year", INT_VAL(1900 + time.tm_year));
    setObjProperty(vm, date, "month", INT_VAL(1 + time.tm_mon));
    setObjProperty(vm, date, "day", INT_VAL(time.tm_mday));
    return date;
}

static ObjInstance* dateTimeObjFromTimestamp(VM* vm, ObjClass* dateTimeClass, double timeValue) {
    time_t timestamp = (time_t)timeValue;
    struct tm time;
    localtime_s(&time, &timestamp);
    ObjInstance* dateTime = newInstance(vm, dateTimeClass);
    setObjProperty(vm, dateTime, "year", INT_VAL(1900 + time.tm_year));
    setObjProperty(vm, dateTime, "month", INT_VAL(1 + time.tm_mon));
    setObjProperty(vm, dateTime, "day", INT_VAL(time.tm_mday));
    setObjProperty(vm, dateTime, "hour", INT_VAL(time.tm_hour));
    setObjProperty(vm, dateTime, "minute", INT_VAL(time.tm_min));
    setObjProperty(vm, dateTime, "second", INT_VAL(time.tm_sec));
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
    setObjProperty(vm, object, "days", INT_VAL(duration[0]));
    setObjProperty(vm, object, "hours", INT_VAL(duration[1]));
    setObjProperty(vm, object, "minutes", INT_VAL(duration[2]));
    setObjProperty(vm, object, "seconds", INT_VAL(duration[3]));
}

static double durationTotalSeconds(VM* vm, ObjInstance* duration) {
    Value days = getObjProperty(vm, duration, "days");
    Value hours = getObjProperty(vm, duration, "hours");
    Value minutes = getObjProperty(vm, duration, "minutes");
    Value seconds = getObjProperty(vm, duration, "seconds");
    return 86400.0 * AS_INT(days) + 3600.0 * AS_INT(hours) + 60.0 * AS_INT(minutes) + AS_INT(seconds);
}

static void listAddAll(VM* vm, ObjList* from, ObjList* to) {
    if (from->elements.count == 0) return;
    for (int i = 0; i < from->elements.count; i++) {
        writeValueArray(vm, &to->elements, from->elements.values[i]);
    }
}

static bool listEqual(ObjList* list, ObjList* list2) {
    if (list->elements.count != list2->elements.count) return false;
    for (int i = 0; i < list->elements.count; i++) {
        if (list->elements.values[i] != list2->elements.values[i]) return false;
    }
    return true;
}

static int listIndexOf(VM* vm, ObjList* list, Value element) {
    for (int i = 0; i < list->elements.count; i++) {
        if (valuesEqual(list->elements.values[i], element)) {
            return i;
        }
    }
    return -1;
}

static void listInsertAt(VM* vm, ObjList* list, int index, Value element) {
    if (IS_OBJ(element)) push(vm, element);
    writeValueArray(vm, &list->elements, NIL_VAL);
    if (IS_OBJ(element)) pop(vm);
    
    for (int i = list->elements.count - 1; i > index; i--) {
        list->elements.values[i] = list->elements.values[i - 1];
    }
    list->elements.values[index] = element;
}

static int listLastIndexOf(VM* vm, ObjList* list, Value element) {
    for (int i = list->elements.count - 1; i >= 0; i--) {
        if (valuesEqual(list->elements.values[i], element)) {
            return i;
        }
    }
    return -1;
}

static Value listRemoveAt(VM* vm, ObjList* list, int index) {
    Value element = list->elements.values[index];
    if (IS_OBJ(element)) push(vm, element);
    
    for (int i = index; i < list->elements.count - 1; i++) {
        list->elements.values[i] = list->elements.values[i + 1];
    }
    list->elements.count--;
    
    if (IS_OBJ(element)) pop(vm);
    return element;
}

static ObjString* listToString(VM* vm, ObjList* list) {
    if (list->elements.count == 0) return copyString(vm, "[]", 2);
    else {
        char string[UINT8_MAX] = "";
        string[0] = '[';
        size_t offset = 1;
        for (int i = 0; i < list->elements.count; i++) {
            char* chars = valueToString(vm, list->elements.values[i]);
            size_t length = strlen(chars);
            memcpy(string + offset, chars, length);
            if (i == list->elements.count - 1) {
                offset += length;
            }
            else{
                memcpy(string + offset + length, ", ", 2);
                offset += length + 2;
            }
        }
        string[offset] = ']';
        string[offset + 1] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
}

LOX_METHOD(Date, after) {
    assertArgCount(vm, "Date::after(date)", 1, argCount);
    assertObjInstanceOfClass(vm, "Date::after(date)", args[0], "Date", 0);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp > timestamp2);
}

LOX_METHOD(Date, before) {
    assertArgCount(vm, "Date::before(date)", 1, argCount);
    assertObjInstanceOfClass(vm, "Date::before(date)", args[0], "Date", 0);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp < timestamp2);
}

LOX_METHOD(Date, diff) {
    assertArgCount(vm, "Date::diff(date)", 1, argCount);
    assertObjInstanceOfClass(vm, "Date::diff(date)", args[0], "Date", 0);
    double timestamp = dateObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_NUMBER(timestamp - timestamp2);
}

LOX_METHOD(Date, getTimestamp) {
    assertArgCount(vm, "Date::getTimestamp()", 0, argCount);
    RETURN_NUMBER(dateObjGetTimestamp(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Date, init) {
    assertArgCount(vm, "Date::init(year, month, day)", 3, argCount);
    assertArgIsInt(vm, "Date::init(year, month, day)", args, 0);
    assertArgIsInt(vm, "Date::init(year, month, day)", args, 1);
    assertArgIsInt(vm, "Date::init(year, month, day)", args, 2);

    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "year", args[0]);
    setObjProperty(vm, self, "month", args[1]);
    setObjProperty(vm, self, "day", args[2]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Date, minus) {
    assertArgCount(vm, "Date::minus(duration)", 1, argCount);
    assertObjInstanceOfClass(vm, "Date::minus(duration)", args[0], "Duration", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateObjGetTimestamp(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* date = dateObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(date);
}

LOX_METHOD(Date, plus) {
    assertArgCount(vm, "Date::plus(duration)", 1, argCount);
    assertObjInstanceOfClass(vm, "Date::plus(duration)", args[0], "Duration", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateObjGetTimestamp(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* date = dateObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(date);
}

LOX_METHOD(Date, toDateTime) {
    assertArgCount(vm, "Date::toDateTime()", 0, argCount);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* dateTime = newInstance(vm, getNativeClass(vm, "DateTime"));
    setObjProperty(vm, dateTime, "year", getObjProperty(vm, self, "year"));
    setObjProperty(vm, dateTime, "month", getObjProperty(vm, self, "month"));
    setObjProperty(vm, dateTime, "day", getObjProperty(vm, self, "day"));
    setObjProperty(vm, dateTime, "hour", INT_VAL(0));
    setObjProperty(vm, dateTime, "minute", INT_VAL(0));
    setObjProperty(vm, dateTime, "second", INT_VAL(0));
    RETURN_OBJ(dateTime);
}

LOX_METHOD(Date, toString) {
    assertArgCount(vm, "Date::toString()", 0, argCount);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value year = getObjProperty(vm, self, "year");
    Value month = getObjProperty(vm, self, "month");
    Value day = getObjProperty(vm, self, "day");
    RETURN_STRING_FMT("%d-%02d-%02d", AS_INT(year), AS_INT(month), AS_INT(day));
}

LOX_METHOD(DateTime, after) {
    assertArgCount(vm, "DateTime::after(date)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::after(date)", args[0], "DateTime", 0);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp > timestamp2);
}

LOX_METHOD(DateTime, before) {
    assertArgCount(vm, "DateTime::before(date)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::before(date)", args[0], "DateTime", 0);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_BOOL(timestamp < timestamp2);
}

LOX_METHOD(DateTime, diff) {
    assertArgCount(vm, "DateTime::diff(date)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::diff(date)", args[0], "DateTime", 0);
    double timestamp = dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver));
    double timestamp2 = dateTimeObjGetTimestamp(vm, AS_INSTANCE(args[0]));
    RETURN_NUMBER(timestamp - timestamp2);
}

LOX_METHOD(DateTime, getTimestamp) {
    assertArgCount(vm, "DateTime::getTimestamp()", 0, argCount);
    RETURN_NUMBER(dateTimeObjGetTimestamp(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(DateTime, init) {
    assertArgCount(vm, "DateTime::init(year, month, day, hour, minute, second)", 6, argCount);
    assertArgIsInt(vm, "DateTime::init(year, month, day, hour, minute, second)", args, 0);
    assertArgIsInt(vm, "DateTime::init(year, month, day, hour, minute, second)", args, 1);
    assertArgIsInt(vm, "DateTime::init(year, month, day, hour, minute, second)", args, 2);
    assertArgIsInt(vm, "DateTime::init(year, month, day, hour, minute, second)", args, 3);
    assertArgIsInt(vm, "DateTime::init(year, month, day, hour, minute, second)", args, 4);
    assertArgIsInt(vm, "DateTime::init(year, month, day, hour, minute, second)", args, 5);
    
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
    assertArgCount(vm, "DateTime::minus(duration)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::minus(duration)", args[0], "Duration", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateTimeObjGetTimestamp(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* dateTime = dateTimeObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(dateTime);
}

LOX_METHOD(DateTime, plus) {
    assertArgCount(vm, "DateTime::plus(duration)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::plus(duration)", args[0], "Duration", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    double timestamp = dateTimeObjGetTimestamp(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0]));
    ObjInstance* dateTime = dateTimeObjFromTimestamp(vm, self->obj.klass, timestamp);
    RETURN_OBJ(dateTime);
}

LOX_METHOD(DateTime, toDate) {
    assertArgCount(vm, "DateTime::toDate()", 0, argCount);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjInstance* date = newInstance(vm, getNativeClass(vm, "Date"));
    setObjProperty(vm, date, "year", getObjProperty(vm, self, "year"));
    setObjProperty(vm, date, "month", getObjProperty(vm, self, "month"));
    setObjProperty(vm, date, "day", getObjProperty(vm, self, "day"));
    RETURN_OBJ(date);
}

LOX_METHOD(DateTime, toString) {
    assertArgCount(vm, "DateTime::toString()", 0, argCount);
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
    assertArgCount(vm, "Dictionary::clear()", 0, argCount);
    freeTable(vm, &AS_DICTIONARY(receiver)->table);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, clone) {
    assertArgCount(vm, "Dictionary::clone()", 0, argCount);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    RETURN_OBJ(copyDictionary(vm, self->table));
}

LOX_METHOD(Dictionary, containsKey) {
    assertArgCount(vm, "Dictionary::containsKey(key)", 1, argCount);
    assertArgIsString(vm, "Dictionary::containsKey(key)", args, 0);
    RETURN_BOOL(tableContainsKey(&AS_DICTIONARY(receiver)->table, AS_STRING(args[0])));
}

LOX_METHOD(Dictionary, containsValue) {
    assertArgCount(vm, "Dictionary::containsValue(value)", 1, argCount);
    RETURN_BOOL(tableContainsValue(&AS_DICTIONARY(receiver)->table, args[0]));
}

LOX_METHOD(Dictionary, getAt) {
    assertArgCount(vm, "Dictionary::getAt(key)", 1, argCount);
    assertArgIsString(vm, "Dictionary::getAt(key)", args, 0);
    Value value;
    bool valueExists = tableGet(&AS_DICTIONARY(receiver)->table, AS_STRING(args[0]), &value);
    if (!valueExists) RETURN_NIL;
    RETURN_VAL(value);
}

LOX_METHOD(Dictionary, init) {
    assertArgCount(vm, "Dictionary::init()", 0, argCount);
    RETURN_OBJ(newDictionary(vm));
}

LOX_METHOD(Dictionary, isEmpty) {
    assertArgCount(vm, "Dictionary::isEmpty()", 0, argCount);
    RETURN_BOOL(AS_DICTIONARY(receiver)->table.count == 0);
}

LOX_METHOD(Dictionary, length) {
    assertArgCount(vm, "Dictionary::length()", 0, argCount);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    RETURN_INT(AS_DICTIONARY(receiver)->table.count);
}

LOX_METHOD(Dictionary, putAll) {
    assertArgCount(vm, "Dictionary::putAll(dictionary)", 1, argCount);
    assertArgIsDictionary(vm, "Dictionary::putAll(dictionary)", args, 0);
    tableAddAll(vm, &AS_DICTIONARY(args[0])->table, &AS_DICTIONARY(receiver)->table);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, putAt) {
    assertArgCount(vm, "Dictionary::putAt(key, value)", 2, argCount);
    assertArgIsString(vm, "Dictionary::putAt(key, value)", args, 0);
    tableSet(vm, &AS_DICTIONARY(receiver)->table, AS_STRING(args[0]), args[1]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, removeAt) {
    assertArgCount(vm, "Dictionary::removeAt(key)", 1, argCount);
    assertArgIsString(vm, "Dictionary::removeAt(key)", args, 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    ObjString* key = AS_STRING(args[0]);
    Value value;
    
    bool keyExists = tableGet(&self->table, key, &value);
    if (!keyExists) RETURN_NIL;
    tableDelete(&self->table, key);
    RETURN_VAL(value);
}

LOX_METHOD(Dictionary, toString) {
    assertArgCount(vm, "Dictionary::toString()", 0, argCount);
    RETURN_OBJ(tableToString(vm, &AS_DICTIONARY(receiver)->table));
}

LOX_METHOD(Duration, getTotalSeconds) {
    assertArgCount(vm, "Duration::getTotalSeconds()", 0, argCount);
    RETURN_NUMBER(durationTotalSeconds(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Duration, init) {
    assertArgCount(vm, "Duration::init(days, hours, minutes, seconds)", 4, argCount);
    assertArgIsInt(vm, "Duration::init(days, hours, minutes, seconds)", args, 0);
    assertArgIsInt(vm, "Duration::init(days, hours, minutes, seconds)", args, 1);
    assertArgIsInt(vm, "Duration::init(days, hours, minutes, seconds)", args, 2);
    assertArgIsInt(vm, "Duration::init(days, hours, minutes, seconds)", args, 3);

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
    assertArgCount(vm, "Duration::minus(duration)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::minus(duration)", args[0], "Duration", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromSeconds(duration, durationTotalSeconds(vm, self) - durationTotalSeconds(vm, AS_INSTANCE(args[0])));
    ObjInstance* object = newInstance(vm, self->obj.klass);
    durationObjInit(vm, duration, object);
    RETURN_OBJ(object);
}

LOX_METHOD(Duration, plus) {
    assertArgCount(vm, "Duration::plus(duration)", 1, argCount);
    assertObjInstanceOfClass(vm, "DateTime::plus(duration)", args[0], "Duration", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int duration[4];
    durationFromSeconds(duration, durationTotalSeconds(vm, self) + durationTotalSeconds(vm, AS_INSTANCE(args[0])));
    ObjInstance* object = newInstance(vm, self->obj.klass);
    durationObjInit(vm, duration, object);
    RETURN_OBJ(object);
}

LOX_METHOD(Duration, toString) {
    assertArgCount(vm, "Duration::toString()", 0, argCount);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value days = getObjProperty(vm, self, "days");
    Value hours = getObjProperty(vm, self, "hours");
    Value minutes = getObjProperty(vm, self, "minutes");
    Value seconds = getObjProperty(vm, self, "seconds");
    RETURN_STRING_FMT("%d days, %02d hours, %02d minutes, %02d seconds", AS_INT(days), AS_INT(hours), AS_INT(minutes), AS_INT(seconds));
}

LOX_METHOD(List, add) {
    assertArgCount(vm, "List::add(element)", 1, argCount);
    writeValueArray(vm, &AS_LIST(receiver)->elements, args[0]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, addAll) {
    assertArgCount(vm, "List::add(list)", 1, argCount);
    assertArgIsList(vm, "List::add(list)", args, 0);
    listAddAll(vm, AS_LIST(args[0]), AS_LIST(receiver));
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, clear) {
    assertArgCount(vm, "List::clear()", 0, argCount);
    freeValueArray(vm, &AS_LIST(receiver)->elements);
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, clone) {
    assertArgCount(vm, "List::clone()", 0, argCount);
    ObjList* self = AS_LIST(receiver);
    RETURN_OBJ(copyList(vm, self->elements, 0, self->elements.count));
}

LOX_METHOD(List, contains) {
    assertArgCount(vm, "List::contains(element)", 1, argCount);
    RETURN_BOOL(listIndexOf(vm, AS_LIST(receiver), args[0]) != -1);
}

LOX_METHOD(List, equals) {
    assertArgCount(vm, "List::equals(other)", 1, argCount);
    if (!IS_LIST(args[0])) RETURN_FALSE;
    RETURN_BOOL(listEqual(AS_LIST(receiver), AS_LIST(args[0])));
}

LOX_METHOD(List, getAt) {
    assertArgCount(vm, "List::getAt(index)", 1, argCount);
    assertArgIsInt(vm, "List::getAt(index)", args, 0);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::getAt(index)", index, 0, self->elements.count - 1, 0);
    RETURN_VAL(self->elements.values[index]);
}

LOX_METHOD(List, indexOf) {
    assertArgCount(vm, "List::indexOf(element)", 1, argCount);
    ObjList* self = AS_LIST(receiver);
    if (self->elements.count == 0) return -1;
    RETURN_INT(listIndexOf(vm, self, args[0]));
}

LOX_METHOD(List, init) {
    assertArgCount(vm, "List::init()", 0, argCount);
    RETURN_OBJ(newList(vm));
}

LOX_METHOD(List, insertAt) {
    assertArgCount(vm, "List::insertAt(index, element)", 2, argCount);
    assertArgIsInt(vm, "List::insertAt(index, element)", args, 0);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::insertAt(index)", index, 0, self->elements.count, 0);
    listInsertAt(vm, self, index, args[1]);
    RETURN_VAL(args[1]);
}

LOX_METHOD(List, isEmpty) {
    assertArgCount(vm, "List::isEmpty()", 0, argCount);
    RETURN_BOOL(AS_LIST(receiver)->elements.count == 0);
}

LOX_METHOD(List, lastIndexOf) {
    assertArgCount(vm, "List::indexOf(element)", 1, argCount);
    ObjList* self = AS_LIST(receiver);
    if (self->elements.count == 0) return -1;
    RETURN_INT(listLastIndexOf(vm, self, args[0]));
}

LOX_METHOD(List, length) {
    assertArgCount(vm, "List::length()", 0, argCount);
    RETURN_INT(AS_LIST(receiver)->elements.count);
}

LOX_METHOD(List, putAt) {
    assertArgCount(vm, "List::putAt(index, element)", 2, argCount);
    assertArgIsInt(vm, "List::putAt(index, element)", args, 0);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::putAt(index)", index, 0, self->elements.count, 0);
    self->elements.values[index] = args[1];
    if (index == self->elements.count) self->elements.count++;
    RETURN_OBJ(receiver);
}

LOX_METHOD(List, remove) {
    assertArgCount(vm, "List::remove(element)", 1, argCount);
    ObjList* self = AS_LIST(receiver);
    int index = listIndexOf(vm, self, args[0]);
    if (index == -1) RETURN_FALSE;
    listRemoveAt(vm, self, index);
    RETURN_TRUE;
}

LOX_METHOD(List, removeAt) {
    assertArgCount(vm, "List::removeAt(index)", 1, argCount);
    assertArgIsInt(vm, "List::removeAt(index)", args, 0);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "List::removeAt(index)", AS_INT(args[0]), 0, self->elements.count - 1, 0);
    Value element = listRemoveAt(vm, self, index);
    RETURN_VAL(element);
}

LOX_METHOD(List, subList) {
    assertArgCount(vm, "List::subList(from, to)", 2, argCount);
    assertArgIsInt(vm, "List::subList(from, to)", args, 0);
    assertArgIsInt(vm, "List::subList(from, to)", args, 1);
    ObjList* self = AS_LIST(receiver);
    int fromIndex = AS_INT(args[0]);
    int toIndex = AS_INT(args[1]);

    assertIntWithinRange(vm, "List::subList(from, to)", fromIndex, 0, self->elements.count, 0);
    assertIntWithinRange(vm, "List::subList(from, to", toIndex, fromIndex, self->elements.count, 1);
    RETURN_OBJ(copyList(vm, self->elements, fromIndex, toIndex));
}

LOX_METHOD(List, toString) {
    assertArgCount(vm, "List::toString()", 0, argCount);
    RETURN_OBJ(listToString(vm, AS_LIST(receiver)));
}

LOX_METHOD(Random, getSeed) {
    assertArgCount(vm, "Random::getSeed()", 0, argCount);
    Value seed = getObjProperty(vm, AS_INSTANCE(receiver), "seed");
    RETURN_VAL(seed);
}

LOX_METHOD(Random, init) {
    assertArgCount(vm, "Random::init()", 0, argCount);
    ObjInstance* self = AS_INSTANCE(receiver);
    uint64_t seed = (uint64_t)time(NULL);
    pcg32_seed(seed);
    setObjProperty(vm, self, "seed", INT_VAL(abs((int)seed)));
    RETURN_OBJ(receiver);
}

LOX_METHOD(Random, nextBool) {
    assertArgCount(vm, "Random::nextBool()", 0, argCount);
    bool value = pcg32_random_bool();
    RETURN_BOOL(value);
}

LOX_METHOD(Random, nextFloat) {
    assertArgCount(vm, "Random::nextFloat()", 0, argCount);
    double value = pcg32_random_double();
    RETURN_NUMBER(value);
}

LOX_METHOD(Random, nextInt) {
    assertArgCount(vm, "Random::nextInt()", 0, argCount);
    uint32_t value = pcg32_random_int();
    RETURN_INT((int)value);
}

LOX_METHOD(Random, nextIntBounded) {
    assertArgCount(vm, "Random::nextIntBounded(bound)", 1, argCount);
    assertArgIsInt(vm, "Random::nextIntBounded(bound)", args, 0);
    assertNumberNonNegative(vm, "Random::nextIntBounded(bound)", AS_NUMBER(args[0]), 0);
    uint32_t value = pcg32_random_int_bounded((uint32_t)AS_INT(args[0]));
    RETURN_INT((int)value);
}

LOX_METHOD(Random, setSeed) {
    assertArgCount(vm, "Random::setSeed(seed)", 1, argCount);
    assertArgIsInt(vm, "Random::setSeed(seed)", args, 0);
    assertNumberNonNegative(vm, "Random::setSeed(seed)", AS_NUMBER(args[0]), 0);
    pcg32_seed((uint64_t)AS_INT(args[0]));
    setObjProperty(vm, AS_INSTANCE(receiver), "seed", args[0]);
    RETURN_NIL;
}

LOX_METHOD(Regex, init) {
    assertArgCount(vm, "Regex::init(pattern)", 1, argCount);
    assertArgIsString(vm, "Regex::init(pattern)", args, 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "pattern", args[0]);
    RETURN_OBJ(self);
}

LOX_METHOD(Regex, match) {
    assertArgCount(vm, "Regex::match(string)", 1, argCount);
    assertArgIsString(vm, "Regex::match(string)", args, 0);
    Value pattern = getObjProperty(vm, AS_INSTANCE(receiver), "pattern");
    int length;
    int index = re_match(AS_CSTRING(pattern), AS_CSTRING(args[0]), &length);
    RETURN_BOOL(index != -1);
}

LOX_METHOD(Regex, replace) {
    assertArgCount(vm, "Regex::replace(original, replacement)", 2, argCount);
    assertArgIsString(vm, "Regex::replace(original, replacement)", args, 0);
    assertArgIsString(vm, "Regex::replace(original, replacement)", args, 1);
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
    assertArgCount(vm, "Regex::toString()", 0, argCount);
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