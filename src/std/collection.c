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

static ObjEntry* dictFindEntry(ObjEntry* entries, int capacity, Value key) {
    uint32_t hash = hashValue(key);
    uint32_t index = hash & (capacity - 1);
    ObjEntry* tombstone = NULL;

    for (;;) {
        ObjEntry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            }
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

static void dictAdjustCapacity(VM* vm, ObjDictionary* dict, int capacity) {
    Entry* entries = ALLOCATE(ObjEntry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    dict->count = 0;
    for (int i = 0; i < dict->capacity; i++) {
        Entry* entry = &dict->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = dictFindEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        dict->count++;
    }

    FREE_ARRAY(ObjEntry, dict->entries, dict->capacity);
    dict->entries = entries;
    dict->capacity = capacity;
}

static bool DictContainsKey(ObjDictionary* dict, Value key) {
    if (dict->count == 0) return false;
    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    return entry->key != NULL;
}

static bool tableContainsValue(ObjDictionary* dict, Value value) {
    if (dict->count == 0) return false;
    for (int i = 0; i < dict->capacity; i++) {
        ObjEntry* entry = &dict->entries[i];
        if (entry->key == NULL) continue;
        if (valuesEqual(entry->value, value)) return true;
    }
    return false;
}

static bool dictGet(ObjDictionary* dict, Value key, Value* value) {
    if (dict->count == 0) return false;
    ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
    if (entry->key == NULL) return false;
    *value = entry->value;
    return true;
}

static bool dictSet(VM* vm, ObjDictionary* dict, Value key, Value value) {
    if (dict->count + 1 > dict->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(dict->capacity);
        ObjEntry* entries = ALLOCATE(ObjEntry, capacity);
        for (int i = 0; i < capacity; i++) {
            entries[i].key = NULL;
            entries[i].value = NIL_VAL;
        }

        dict->count = 0;
        for (int i = 0; i < dict->capacity; i++) {
            ObjEntry* entry = &dict->entries[i];
            if (entry->key == NULL) continue;

            ObjEntry* dest = dictFindEntry(entries, capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;
            dict->count++;
        }

        FREE_ARRAY(ObjEntry, dict->entries, dict->capacity);
        dict->entries = entries;
        dict->capacity = capacity;

        ObjEntry* entry = dictFindEntry(dict->entries, dict->capacity, key);
        bool isNewKey = entry->key == NULL;
        if (isNewKey && IS_NIL(entry->value)) dict->count++;

        entry->key = key;
        entry->value = value;
        return isNewKey;
    }
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

LOX_METHOD(Dictionary, next) {
    ASSERT_ARG_COUNT("Dictionary::next(index)", 1);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    if (self->table.count == 0) RETURN_FALSE;

    int index = 0;
    if (!IS_NIL(args[0])) {
        ObjString* key = AS_STRING(args[0]);
        index = tableFindIndex(&self->table, key);
        if (index < 0 || index >= self->table.capacity) RETURN_FALSE;
        index++;
    }

    for (; index < self->table.capacity; index++) {
        if (self->table.entries[index].key != NULL) RETURN_OBJ(self->table.entries[index].key);
    }
    RETURN_FALSE;
}

LOX_METHOD(Dictionary, nextValue) {
    ASSERT_ARG_COUNT("Dictionary::nextValue(key)", 1);
    ASSERT_ARG_TYPE("Dictionary::nextValue(key)", 0, String);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    int index = tableFindIndex(&self->table, AS_STRING(args[0]));
    RETURN_VAL(self->table.entries[index].value);
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
    RETURN_OBJ(copyList(vm, self->elements, fromIndex, toIndex));
}

LOX_METHOD(List, toString) {
    ASSERT_ARG_COUNT("List::toString()", 0);
    RETURN_OBJ(valueArrayToString(vm, &AS_LIST(receiver)->elements));
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

    ObjClass* entryClass = defineNativeClass(vm, "Entry");
    bindSuperclass(vm, entryClass, vm->objectClass);

    ObjClass* setClass = defineNativeClass(vm, "Set");
    bindSuperclass(vm, setClass, collectionClass);
}