#include <stdlib.h>
#include <string.h>

#include "collection.h"
#include "../vm/assert.h"
#include "../vm/hash.h"
#include "../vm/memory.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

static bool dictContainsKey(ObjDictionary* dict, Value key) {
    if (dict->count == 0) return false;
    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    return !IS_UNDEFINED(entry->key);
}

static bool dictContainsValue(ObjDictionary* dict, Value value) {
    if (dict->count == 0) return false;
    for (int i = 0; i < dict->capacity; i++) {
        ObjEntry* entry = &dict->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        if (valuesEqual(entry->value, value)) return true;
    }
    return false;
}

static bool dictDelete(ObjDictionary* dict, Value key) {
    if (dict->count == 0) return false;

    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    if (IS_UNDEFINED(entry->key)) return false;

    entry->key = UNDEFINED_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

static void dictAddAll(VM* vm, ObjDictionary* from, ObjDictionary* to) {
    for (int i = 0; i < from->capacity; i++) {
        ObjEntry* entry = &from->entries[i];
        if (!IS_UNDEFINED(entry->key)) {
            dictSet(vm, to, entry->key, entry->value);
        }
    }
}

static ObjDictionary* dictCopy(VM* vm, ObjDictionary* original) {
    ObjDictionary* copied = newDictionary(vm);
    push(vm, OBJ_VAL(copied));
    dictAddAll(vm, original, copied);
    pop(vm);
    return copied;
}

static bool dictsEqual(ObjDictionary* aDict, ObjDictionary* dict2) {
    for (int i = 0; i < aDict->capacity; i++) {
        ObjEntry* entry = &aDict->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        Value bValue;
        bool keyExists = dictGet(dict2, entry->key, &bValue);
        if (!keyExists || entry->value != bValue) return false;
    }

    for (int i = 0; i < dict2->capacity; i++) {
        ObjEntry* entry = &dict2->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        Value aValue;
        bool keyExists = dictGet(aDict, entry->key, &aValue);
        if (!keyExists || entry->value != aValue) return false;
    }

    return true;
}

static int dictFindIndex(ObjDictionary* dict, Value key) {
    uint32_t hash = hashValue(key);
    uint32_t index = hash & (dict->capacity - 1);
    ObjEntry* tombstone = NULL;

    for (;;) {
        ObjEntry* entry = &dict->entries[index];
        if (IS_UNDEFINED(entry->key)) {
            if (IS_NIL(entry->value)) {
                return -1;
            }
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key) {
            return index;
        }

        index = (index + 1) & (dict->capacity - 1);
    }
}

static int dictLength(ObjDictionary* dict) {
    if (dict->count == 0) return 0;
    int length = 0;
    for (int i = 0; i < dict->capacity; i++) {
        ObjEntry* entry = &dict->entries[i];
        if (!IS_UNDEFINED(entry->key)) length++;
    }
    return length;
}

ObjString* dictToString(VM* vm, ObjDictionary* dict) {
    if (dict->count == 0) return copyString(vm, "[]", 2);
    else {
        char string[UINT8_MAX] = "";
        string[0] = '[';
        size_t offset = 1;
        int startIndex = 0;

        for (int i = 0; i < dict->capacity; i++) {
            ObjEntry* entry = &dict->entries[i];
            if (IS_UNDEFINED(entry->key)) continue;
            Value key = entry->key;
            char* keyChars = valueToString(vm, key);
            size_t keyLength = strlen(keyChars);
            Value value = entry->value;
            char* valueChars = valueToString(vm, value);
            size_t valueLength = strlen(valueChars);

            memcpy(string + offset, keyChars, keyLength);
            offset += keyLength;
            memcpy(string + offset, ": ", 2);
            offset += 2;
            memcpy(string + offset, valueChars, valueLength);
            offset += valueLength;
            startIndex = i + 1;
            break;
        }

        for (int i = startIndex; i < dict->capacity; i++) {
            ObjEntry* entry = &dict->entries[i];
            if (IS_UNDEFINED(entry->key)) continue;
            Value key = entry->key;
            char* keyChars = valueToString(vm, key);
            size_t keyLength = strlen(keyChars);
            Value value = entry->value;
            char* valueChars = valueToString(vm, value);
            size_t valueLength = strlen(valueChars);

            memcpy(string + offset, ", ", 2);
            offset += 2;
            memcpy(string + offset, keyChars, keyLength);
            offset += keyLength;
            memcpy(string + offset, ": ", 2);
            offset += 2;
            memcpy(string + offset, valueChars, valueLength);
            offset += valueLength;
        }

        string[offset] = ']';
        string[offset + 1] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
}

ObjList* listCopy(VM* vm, ValueArray elements, int fromIndex, int toIndex) {
    ObjList* list = newList(vm);
    push(vm, OBJ_VAL(list));
    for (int i = fromIndex; i < toIndex; i++) {
        valueArrayWrite(vm, &list->elements, elements.values[i]);
    }
    pop(vm);
    return list;
}

ObjInstance* setCopy(VM* vm, ObjInstance* original) {
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, original, "dict"));
    ObjDictionary* dict2 = newDictionary(vm);
    push(vm, OBJ_VAL(dict2));
    dictAddAll(vm, dict, dict2);
    pop(vm);
    ObjInstance* copied = newInstance(vm, getNativeClass(vm, "Set"));
    setObjProperty(vm, copied, "dict", OBJ_VAL(dict2));
    return copied;
}

ObjString* setToString(VM* vm, ObjInstance* set) {
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, set, "dict"));
    if (dict->count == 0) return copyString(vm, "[]", 2);
    else {
        char string[UINT8_MAX] = "";
        string[0] = '[';
        size_t offset = 1;
        int startIndex = 0;

        for (int i = 0; i < dict->capacity; i++) {
            ObjEntry* entry = &dict->entries[i];
            if (IS_UNDEFINED(entry->key)) continue;
            Value key = entry->key;
            char* keyChars = valueToString(vm, key);
            size_t keyLength = strlen(keyChars);

            memcpy(string + offset, keyChars, keyLength);
            offset += keyLength;
            startIndex = i + 1;
            break;
        }

        for (int i = startIndex; i < dict->capacity; i++) {
            ObjEntry* entry = &dict->entries[i];
            if (IS_UNDEFINED(entry->key)) continue;
            Value key = entry->key;
            char* keyChars = valueToString(vm, key);
            size_t keyLength = strlen(keyChars);

            memcpy(string + offset, ", ", 2);
            offset += 2;
            memcpy(string + offset, keyChars, keyLength);
            offset += keyLength;
        }

        string[offset] = ']';
        string[offset + 1] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
}

LOX_METHOD(Dictionary, clear) {
    ASSERT_ARG_COUNT("Dictionary::clear()", 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    FREE_ARRAY(ObjEntry, self->entries, self->capacity);
    self->count = 0;
    self->capacity = 0;
    self->entries = NULL;
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, clone) {
    ASSERT_ARG_COUNT("Dictionary::clone()", 0);
    RETURN_OBJ(dictCopy(vm, AS_DICTIONARY(receiver)));
}

LOX_METHOD(Dictionary, containsKey) {
    ASSERT_ARG_COUNT("Dictionary::containsKey(key)", 1);
    RETURN_BOOL(dictContainsKey(AS_DICTIONARY(receiver), args[0]));
}

LOX_METHOD(Dictionary, containsValue) {
    ASSERT_ARG_COUNT("Dictionary::containsValue(value)", 1);
    RETURN_BOOL(dictContainsValue(AS_DICTIONARY(receiver), args[0]));
}

LOX_METHOD(Dictionary, equals) {
    ASSERT_ARG_COUNT("Dictionary::equals(other)", 1);
    if (!IS_DICTIONARY(args[0])) RETURN_FALSE;
    RETURN_BOOL(dictsEqual(AS_DICTIONARY(receiver), AS_DICTIONARY(args[0])));
}

LOX_METHOD(Dictionary, getAt) {
    ASSERT_ARG_COUNT("Dictionary::getAt(key)", 1);
    Value value;
    bool valueExists = dictGet(AS_DICTIONARY(receiver), args[0], &value);
    if (!valueExists) RETURN_NIL;
    RETURN_VAL(value);
}

LOX_METHOD(Dictionary, init) {
    ASSERT_ARG_COUNT("Dictionary::init()", 0);
    RETURN_OBJ(newDictionary(vm));
}

LOX_METHOD(Dictionary, isEmpty) {
    ASSERT_ARG_COUNT("Dictionary::isEmpty()", 0);
    RETURN_BOOL(AS_DICTIONARY(receiver)->count == 0);
}

LOX_METHOD(Dictionary, length) {
    ASSERT_ARG_COUNT("Dictionary::length()", 0);
    RETURN_INT(dictLength(AS_DICTIONARY(receiver)));
}

LOX_METHOD(Dictionary, next) {
    ASSERT_ARG_COUNT("Dictionary::next(index)", 1);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    if (self->count == 0) RETURN_FALSE;

    int index = 0;
    if (!IS_NIL(args[0])) {
        Value key = args[0];
        index = dictFindIndex(self, key);
        if (index < 0 || index >= self->capacity) RETURN_FALSE;
        index++;
    }

    for (; index < self->capacity; index++) {
        if (!IS_UNDEFINED(self->entries[index].key)) RETURN_VAL(self->entries[index].key);
    }
    RETURN_FALSE;
}

LOX_METHOD(Dictionary, nextValue) {
    ASSERT_ARG_COUNT("Dictionary::nextValue(key)", 1);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    int index = dictFindIndex(self, args[0]);
    RETURN_VAL(self->entries[index].value);
}

LOX_METHOD(Dictionary, putAll) {
    ASSERT_ARG_COUNT("Dictionary::putAll(dictionary)", 1);
    ASSERT_ARG_TYPE("Dictionary::putAll(dictionary)", 0, Dictionary);
    dictAddAll(vm, AS_DICTIONARY(args[0]), AS_DICTIONARY(receiver));
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, putAt) {
    ASSERT_ARG_COUNT("Dictionary::putAt(key, value)", 2);
    dictSet(vm, AS_DICTIONARY(receiver), args[0], args[1]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Dictionary, removeAt) {
    ASSERT_ARG_COUNT("Dictionary::removeAt(key)", 1);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    Value key = args[0];
    Value value;

    bool keyExists = dictGet(self, key, &value);
    if (!keyExists) RETURN_NIL;
    dictDelete(self, key);
    RETURN_VAL(value);
}

LOX_METHOD(Dictionary, toString) {
    ASSERT_ARG_COUNT("Dictionary::toString()", 0);
    RETURN_OBJ(dictToString(vm, AS_DICTIONARY(receiver)));
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
    RETURN_OBJ(listCopy(vm, self->elements, 0, self->elements.count));
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

LOX_METHOD(List, next) {
    ASSERT_ARG_COUNT("List::next(index)", 1);
    ObjList* self = AS_LIST(receiver);
    if (IS_NIL(args[0])) {
        if (self->elements.count == 0) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("List::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index < 0 || index < self->elements.count - 1) RETURN_INT(index + 1);
    RETURN_NIL;
}

LOX_METHOD(List, nextValue) {
    ASSERT_ARG_COUNT("List::nextValue(index)", 1);
    ASSERT_ARG_TYPE("List::nextValue(index)", 0, Int);
    ObjList* self = AS_LIST(receiver);
    int index = AS_INT(args[0]);
    if (index > -1 && index < self->elements.count) RETURN_VAL(self->elements.values[index]);
    RETURN_NIL;
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
    RETURN_OBJ(listCopy(vm, self->elements, fromIndex, toIndex));
}
 
LOX_METHOD(List, toString) {
    ASSERT_ARG_COUNT("List::toString()", 0);
    RETURN_OBJ(valueArrayToString(vm, &AS_LIST(receiver)->elements));
}

LOX_METHOD(Set, add) {
    ASSERT_ARG_COUNT("Set::add(element)", 1);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    dictSet(vm, dict, args[0], NIL_VAL);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Set, clear) {
    ASSERT_ARG_COUNT("Set::clear()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, self, "dict"));
    FREE_ARRAY(ObjEntry, dict->entries, dict->capacity);
    dict->count = 0;
    dict->capacity = 0;
    dict->entries = NULL;
    RETURN_OBJ(receiver);
}

LOX_METHOD(Set, clone) {
    ASSERT_ARG_COUNT("Set::clone()", 0);
    RETURN_OBJ(setCopy(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Set, contains) {
    ASSERT_ARG_COUNT("Set::contains(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, self, "dict"));
    RETURN_BOOL(dictContainsKey(dict, args[0]));
}

LOX_METHOD(Set, equals) {
    ASSERT_ARG_COUNT("Set::equals(other)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    if (!isObjInstanceOf(vm, args[0], self->obj.klass)) RETURN_FALSE;
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, self, "dict"));
    ObjDictionary* dict2 = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(args[0]), "dict"));
    RETURN_BOOL(dictsEqual(dict, dict2));
}

LOX_METHOD(Set, init) {
    ASSERT_ARG_COUNT("Set::init()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "dict", OBJ_VAL(newDictionary(vm)));
    RETURN_OBJ(receiver);
}

LOX_METHOD(Set, isEmpty) {
    ASSERT_ARG_COUNT("Set::isEmpty()", 0);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    RETURN_BOOL(dict->count == 0);
}

LOX_METHOD(Set, length) {
    ASSERT_ARG_COUNT("Set::length()", 0);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    RETURN_INT(dictLength(dict));
}

LOX_METHOD(Set, next) {
    ASSERT_ARG_COUNT("Set::next(index)", 1);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    if (dict->count == 0) RETURN_FALSE;

    int index = 0;
    if (!IS_NIL(args[0])) {
        Value key = args[0];
        index = dictFindIndex(dict, key);
        if (index < 0 || index >= dict->capacity) RETURN_FALSE;
        index++;
    }

    for (; index < dict->capacity; index++) {
        if (!IS_UNDEFINED(dict->entries[index].key)) RETURN_VAL(dict->entries[index].key);
    }
    RETURN_FALSE;
}

LOX_METHOD(Set, nextValue) {
    ASSERT_ARG_COUNT("Set::nextValue(index)", 1);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    int index = dictFindIndex(dict, args[0]);
    RETURN_VAL(dict->entries[index].key);
}

LOX_METHOD(Set, remove) {
    ASSERT_ARG_COUNT("Set::removeAt(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, self, "dict"));
    Value key = args[0];
    Value value;

    bool keyExists = dictGet(dict, key, &value);
    if (!keyExists) RETURN_NIL;
    dictDelete(dict, key);
    RETURN_VAL(value);
}

LOX_METHOD(Set, toString) {
    ASSERT_ARG_COUNT("Set::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_OBJ(setToString(vm, self));   
}

void registerCollectionPackage(VM* vm) {
    initNativePackage(vm, "src/std/collection.lox");
    ObjClass* collectionClass = getNativeClass(vm, "Collection");
    bindSuperclass(vm, collectionClass, vm->objectClass);

    vm->listClass = getNativeClass(vm, "List");
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
    DEF_METHOD(vm->listClass, List, next, 1);
    DEF_METHOD(vm->listClass, List, nextValue, 1);
    DEF_METHOD(vm->listClass, List, putAt, 2);
    DEF_METHOD(vm->listClass, List, remove, 1);
    DEF_METHOD(vm->listClass, List, removeAt, 1);
    DEF_METHOD(vm->listClass, List, subList, 2);
    DEF_METHOD(vm->listClass, List, toString, 0);

    vm->dictionaryClass = getNativeClass(vm, "Dictionary");
    DEF_METHOD(vm->dictionaryClass, Dictionary, clear, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, clone, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, containsKey, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, containsValue, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, equals, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, getAt, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, init, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, isEmpty, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, length, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, next, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, nextValue, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, putAll, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, putAt, 2);
    DEF_METHOD(vm->dictionaryClass, Dictionary, removeAt, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, toString, 0);

    ObjClass* setClass = defineNativeClass(vm, "Set");
    bindSuperclass(vm, setClass, collectionClass);
    DEF_METHOD(setClass, Set, add, 1);
    DEF_METHOD(setClass, Set, clear, 0);
    DEF_METHOD(setClass, Set, clone, 0);
    DEF_METHOD(setClass, Set, contains, 1);
    DEF_METHOD(setClass, Set, equals, 1);
    DEF_METHOD(setClass, Set, init, 0);
    DEF_METHOD(setClass, Set, isEmpty, 0);
    DEF_METHOD(setClass, Set, length, 0);
    DEF_METHOD(setClass, Set, next, 1);
    DEF_METHOD(setClass, Set, nextValue, 1);
    DEF_METHOD(setClass, Set, remove, 1);
    DEF_METHOD(setClass, Set, toString, 0);
}