#include <stdlib.h>
#include <string.h>

#include "collection.h"
#include "../vm/assert.h"
#include "../vm/dict.h"
#include "../vm/hash.h"
#include "../vm/memory.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

ObjArray* arrayCopy(VM* vm, ValueArray elements, int fromIndex, int toIndex) {
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    for (int i = fromIndex; i < toIndex; i++) {
        valueArrayWrite(vm, &array->elements, elements.values[i]);
    }
    pop(vm);
    return array;
}

static bool collectionIsEmpty(VM* vm, ObjInstance* collection) {
    int length = AS_INT(getObjProperty(vm, collection, "length"));
    return (length == 0);
}

static void collectionLengthDecrement(VM* vm, ObjInstance* collection) {
    int length = AS_INT(getObjProperty(vm, collection, "length"));
    setObjProperty(vm, collection, "length", INT_VAL(length - 1));
}

static void collectionLengthIncrement(VM* vm, ObjInstance* collection) {
    int length = AS_INT(getObjProperty(vm, collection, "length"));
    setObjProperty(vm, collection, "length", INT_VAL(length + 1));
}

static ObjDictionary* dictCopy(VM* vm, ObjDictionary* original) {
    ObjDictionary* copied = newDictionary(vm);
    push(vm, OBJ_VAL(copied));
    dictAddAll(vm, original, copied);
    pop(vm);
    return copied;
}

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

static bool dictsEqual(ObjDictionary* aDict, ObjDictionary* dict2) {
    for (int i = 0; i < aDict->capacity; i++) {
        ObjEntry* entry = &aDict->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        Value bValue;
        bool keyExists = dictGet(dict2, entry->key, &bValue);
        if (!keyExists || entry->value != bValue) {
            return false;
        }
    }

    for (int i = 0; i < dict2->capacity; i++) {
        ObjEntry* entry = &dict2->entries[i];
        if (IS_UNDEFINED(entry->key)) continue;
        Value aValue;
        bool keyExists = dictGet(aDict, entry->key, &aValue);
        if (!keyExists || entry->value != aValue) {
            return false;
        }
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

static void linkAddBefore(VM* vm, ObjInstance* linkedList, Value element, ObjNode* succ) {
    if (succ == NULL) raiseError(vm, "The next element cannot be nil.");
    else {
        ObjNode* pred = succ->prev;
        ObjNode* new = newNode(vm, element, pred, succ);
        push(vm, OBJ_VAL(new));
        succ->prev = new;
        if (pred == NULL) setObjProperty(vm, linkedList, "first", OBJ_VAL(new));
        else pred->next = new;
        pop(vm);
        collectionLengthIncrement(vm, linkedList);
    }
}

static void linkAddFirst(VM* vm, ObjInstance* linkedList, Value element) {
    Value f = getObjProperty(vm, linkedList, "first");
    ObjNode* first = IS_NIL(f) ? NULL : AS_NODE(f);
    ObjNode* new = newNode(vm, element, NULL, first);
    push(vm, OBJ_VAL(new));
    setObjProperty(vm, linkedList, "first", OBJ_VAL(new));
    if (first == NULL) setObjProperty(vm, linkedList, "last", OBJ_VAL(new));
    else first->prev = new;
    pop(vm);
    collectionLengthIncrement(vm, linkedList);
}

static void linkAddLast(VM* vm, ObjInstance* linkedList, Value element) {
    Value l = getObjProperty(vm, linkedList, "last");
    ObjNode* last = IS_NIL(l) ? NULL : AS_NODE(l);
    ObjNode* new = newNode(vm, element, last, NULL);
    push(vm, OBJ_VAL(new));
    setObjProperty(vm, linkedList, "last", OBJ_VAL(new));
    if (last == NULL) setObjProperty(vm, linkedList, "first", OBJ_VAL(new));
    else last->next = new;
    pop(vm);
    collectionLengthIncrement(vm, linkedList);
}

static int linkFindIndex(VM* vm, ObjInstance* linkedList, Value element) {
    int index = 0;
    Value f = getObjProperty(vm, linkedList, "first");
    ObjNode* first = IS_NIL(f) ? NULL : AS_NODE(f);
    for (ObjNode* node = first; node != NULL; node = node->next) {
        if (valuesEqual(element, node->element)) return index;
        index++;
    }
    return -1;
}

static int linkFindLastIndex(VM* vm, ObjInstance* linkedList, Value element) {
    int index = AS_INT(getObjProperty(vm, linkedList, "length"));
    Value l = getObjProperty(vm, linkedList, "last");
    ObjNode* last = IS_NIL(l) ? NULL : AS_NODE(l);
    for (ObjNode* node = last; node != NULL; node = node->prev) {
        index--;
        if (valuesEqual(element, node->element)) return index;
    }
    return -1;
}

static bool linkIndexIsValid(VM* vm, ObjInstance* linkedList, int index) {
    int length = AS_INT(getObjProperty(vm, linkedList, "length"));
    return (index >= 0 && index < length);
}

static void linkIndexValidate(VM* vm, ObjInstance* linkedList, int index) {
    if (!linkIndexIsValid(vm, linkedList, index)) {
        raiseError(vm, "Index out of bound for LinkedList.");
    }
}

static ObjNode* linkNode(VM* vm, ObjInstance* linkedList, int index) {
    int length = AS_INT(getObjProperty(vm, linkedList, "length"));
    if (index < (length >> 1)) {
        ObjNode* node = AS_NODE(getObjProperty(vm, linkedList, "first"));
        for (int i = 0; i < index; i++) {
            node = node->next;
        }
        return node;
    }
    else {
        ObjNode* node = AS_NODE(getObjProperty(vm, linkedList, "last"));
        for (int i = length - 1; i > index; i--) {
            node = node->prev;
        }
        return node;
    }
}

static Value linkRemove(VM* vm, ObjInstance* linkedList, ObjNode* node) {
    if (node == NULL) raiseError(vm, "Cannot unlink NULL node.");
    else {
        Value element = node->element;
        ObjNode* next = node->next;
        ObjNode* prev = node->prev;

        if (prev == NULL) {
            setObjProperty(vm, linkedList, "first", OBJ_VAL(next));
        }
        else {
            prev->next = next;
            node->prev = NULL;
        }

        if (next == NULL) {
            setObjProperty(vm, linkedList, "last", OBJ_VAL(prev));
        }
        else {
            next->prev = prev;
            node->next = NULL;
        }

        node->element = NIL_VAL;
        collectionLengthDecrement(vm, linkedList);
        RETURN_VAL(element);
    }
}

static Value linkRemoveFirst(VM* vm, ObjInstance* linkedList, ObjNode* first) {
    Value element = first->element;
    ObjNode* next = first->next;
    first->element = NIL_VAL;
    first->next = NULL;
    setObjProperty(vm, linkedList, "first", OBJ_VAL(next));
    if (next == NULL) setObjProperty(vm, linkedList, "last", NIL_VAL);
    else next->prev = NULL;
    collectionLengthDecrement(vm, linkedList);
    RETURN_VAL(element);
}

static Value linkRemoveLast(VM* vm, ObjInstance* linkedList, ObjNode* last) {
    Value element = last->element;
    ObjNode* prev = last->prev;
    last->element = NIL_VAL;
    last->next = NULL;
    setObjProperty(vm, linkedList, "last", OBJ_VAL(prev));
    if (prev == NULL) setObjProperty(vm, linkedList, "first", NIL_VAL);
    else prev->next = NULL;
    collectionLengthDecrement(vm, linkedList);
    RETURN_VAL(element);
}

static int linkSearchElement(VM* vm, ObjInstance* linkedList, Value element) {
    int length = AS_INT(getObjProperty(vm, linkedList, "length"));
    if (length > 0) {
        ObjNode * first = AS_NODE(getObjProperty(vm, linkedList, "first"));
        for (int i = 0; i < length; i++) {
            if (valuesEqual(element, first->element)) return i;
            first = first->next;
        }
    }
    return -1;
}

static ObjString* linkToString(VM* vm, ObjInstance* linkedList) {
    int size = AS_INT(getObjProperty(vm, linkedList, "length"));
    if (size == 0) return copyString(vm, "[]", 2);
    else {
        char string[UINT8_MAX] = "";
        string[0] = '[';
        size_t offset = 1;
        ObjNode* node = AS_NODE(getObjProperty(vm, linkedList, "first"));
        for (int i = 0; i < size; i++) {
            char* chars = valueToString(vm, node->element);
            size_t length = strlen(chars);
            memcpy(string + offset, chars, length);
            if (i == size - 1) {
                offset += length;
            }
            else {
                memcpy(string + offset + length, ", ", 2);
                offset += length + 2;
            }
            node = node->next;
        }
        string[offset] = ']';
        string[offset + 1] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
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

LOX_METHOD(Array, add) {
    ASSERT_ARG_COUNT("Array::add(element)", 1);
    valueArrayWrite(vm, &AS_ARRAY(receiver)->elements, args[0]);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Array, addAll) {
    ASSERT_ARG_COUNT("Array::addAll(array)", 1);
    ASSERT_ARG_TYPE("Array::addAll(array)", 0, Array);
    valueArrayAddAll(vm, &AS_ARRAY(args[0])->elements, &AS_ARRAY(receiver)->elements);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Array, clear) {
    ASSERT_ARG_COUNT("Array::clear()", 0);
    freeValueArray(vm, &AS_ARRAY(receiver)->elements);
    RETURN_OBJ(receiver);
}

LOX_METHOD(Array, clone) {
    ASSERT_ARG_COUNT("Array::clone()", 0);
    ObjArray* self = AS_ARRAY(receiver);
    RETURN_OBJ(arrayCopy(vm, self->elements, 0, self->elements.count));
}

LOX_METHOD(Array, collect) {
    ASSERT_ARG_COUNT("Array::collect(closure)", 1);
    ASSERT_ARG_TYPE("Array::collect(closure)", 0, Closure);
    ObjArray* self = AS_ARRAY(receiver);
    ObjClosure* closure = AS_CLOSURE(args[0]);

    ObjArray* collected = newArray(vm);
    push(vm, OBJ_VAL(collected));
    for (int i = 0; i < self->elements.count; i++) {
        Value result = callReentrant(vm, OBJ_VAL(closure), self->elements.values[i]);
        valueArrayWrite(vm, &collected->elements, result);
    }
    pop(vm);
    RETURN_OBJ(collected);
}

LOX_METHOD(Array, contains) {
    ASSERT_ARG_COUNT("Array::contains(element)", 1);
    RETURN_BOOL(valueArrayFirstIndex(vm, &AS_ARRAY(receiver)->elements, args[0]) != -1);
}

LOX_METHOD(Array, detect) {
    ASSERT_ARG_COUNT("Array::detect(closure)", 1);
    ASSERT_ARG_TYPE("Array::detect(closure)", 0, Closure);
    ObjArray* self = AS_ARRAY(receiver);
    ObjClosure* closure = AS_CLOSURE(args[0]);

    for (int i = 0; i < self->elements.count; i++) {
        Value result = callReentrant(vm, OBJ_VAL(closure), self->elements.values[i]);
        if (!isFalsey(result)) RETURN_VAL(self->elements.values[i]);
    }
    RETURN_NIL;
}

LOX_METHOD(Array, each) {
    ASSERT_ARG_COUNT("Array::each(closure)", 1);
    ASSERT_ARG_TYPE("Array::each(closure)", 0, Closure);
    ObjArray* self = AS_ARRAY(receiver);
    ObjClosure* closure = AS_CLOSURE(args[0]);

    for (int i = 0; i < self->elements.count; i++) {
        callReentrant(vm, OBJ_VAL(closure), self->elements.values[i]);
    }
    RETURN_NIL;
}

LOX_METHOD(Array, equals) {
    ASSERT_ARG_COUNT("Array::equals(other)", 1);
    if (!IS_ARRAY(args[0])) RETURN_FALSE;
    RETURN_BOOL(valueArraysEqual(&AS_ARRAY(receiver)->elements, &AS_ARRAY(args[0])->elements));
}

LOX_METHOD(Array, getAt) {
    ASSERT_ARG_COUNT("Array::getAt(index)", 1);
    ASSERT_ARG_TYPE("Array::getAt(index)", 0, Int);
    ObjArray* self = AS_ARRAY(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "Array::getAt(index)", index, 0, self->elements.count - 1, 0);
    RETURN_VAL(self->elements.values[index]);
}

LOX_METHOD(Array, indexOf) {
    ASSERT_ARG_COUNT("Array::indexOf(element)", 1);
    ObjArray* self = AS_ARRAY(receiver);
    if (self->elements.count == 0) RETURN_INT(-1);
    RETURN_INT(valueArrayFirstIndex(vm, &self->elements, args[0]));
}

LOX_METHOD(Array, init) {
    ASSERT_ARG_COUNT("Array::init()", 0);
    RETURN_OBJ(newArray(vm));
}

LOX_METHOD(Array, insertAt) {
    ASSERT_ARG_COUNT("Array::insertAt(index, element)", 2);
    ASSERT_ARG_TYPE("Array::insertAt(index, element)", 0, Int);
    ObjArray* self = AS_ARRAY(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "Array::insertAt(index, element)", index, 0, self->elements.count, 0);
    valueArrayInsert(vm, &self->elements, index, args[1]);
    RETURN_VAL(args[1]);
}

LOX_METHOD(Array, isEmpty) {
    ASSERT_ARG_COUNT("Array::isEmpty()", 0);
    RETURN_BOOL(AS_ARRAY(receiver)->elements.count == 0);
}

LOX_METHOD(Array, lastIndexOf) {
    ASSERT_ARG_COUNT("Array::lastIndexOf(element)", 1);
    ObjArray* self = AS_ARRAY(receiver);
    if (self->elements.count == 0) return -1;
    RETURN_INT(valueArrayLastIndex(vm, &self->elements, args[0]));
}

LOX_METHOD(Array, length) {
    ASSERT_ARG_COUNT("Array::length()", 0);
    RETURN_INT(AS_ARRAY(receiver)->elements.count);
}

LOX_METHOD(Array, next) {
    ASSERT_ARG_COUNT("Array::next(index)", 1);
    ObjArray* self = AS_ARRAY(receiver);
    if (IS_NIL(args[0])) {
        if (self->elements.count == 0) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("Array::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index < 0 || index < self->elements.count - 1) RETURN_INT(index + 1);
    RETURN_NIL;
}

LOX_METHOD(Array, nextValue) {
    ASSERT_ARG_COUNT("Array::nextValue(index)", 1);
    ASSERT_ARG_TYPE("Array::nextValue(index)", 0, Int);
    ObjArray* self = AS_ARRAY(receiver);
    int index = AS_INT(args[0]);
    if (index > -1 && index < self->elements.count) RETURN_VAL(self->elements.values[index]);
    RETURN_NIL;
}

LOX_METHOD(Array, putAt) {
    ASSERT_ARG_COUNT("Array::putAt(index, element)", 2);
    ASSERT_ARG_TYPE("Array::putAt(index, element)", 0, Int);
    ObjArray* self = AS_ARRAY(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "Array::putAt(index, element)", index, 0, self->elements.count, 0);
    self->elements.values[index] = args[1];
    if (index == self->elements.count) self->elements.count++;
    RETURN_OBJ(receiver);
}

LOX_METHOD(Array, reject) {
    ASSERT_ARG_COUNT("Array::reject(closure)", 1);
    ASSERT_ARG_TYPE("Array::reject(closure)", 0, Closure);
    ObjArray* self = AS_ARRAY(receiver);
    ObjClosure* closure = AS_CLOSURE(args[0]);

    ObjArray* rejected = newArray(vm);
    push(vm, OBJ_VAL(rejected));
    for (int i = 0; i < self->elements.count; i++) {
        Value result = callReentrant(vm, OBJ_VAL(closure), self->elements.values[i]);
        if (isFalsey(result)) valueArrayWrite(vm, &rejected->elements, self->elements.values[i]);
    }
    pop(vm);
    RETURN_OBJ(rejected);
}

LOX_METHOD(Array, remove) {
    ASSERT_ARG_COUNT("Array::remove(element)", 1);
    ObjArray* self = AS_ARRAY(receiver);
    int index = valueArrayFirstIndex(vm, &self->elements, args[0]);
    if (index == -1) RETURN_FALSE;
    valueArrayDelete(vm, &self->elements, index);
    RETURN_TRUE;
}

LOX_METHOD(Array, removeAt) {
    ASSERT_ARG_COUNT("Array::removeAt(index)", 1);
    ASSERT_ARG_TYPE("Array::removeAt(index)", 0, Int);
    ObjArray* self = AS_ARRAY(receiver);
    int index = AS_INT(args[0]);
    assertIntWithinRange(vm, "Array::removeAt(index)", AS_INT(args[0]), 0, self->elements.count - 1, 0);
    Value element = valueArrayDelete(vm, &self->elements, index);
    RETURN_VAL(element);
}

LOX_METHOD(Array, select) {
    ASSERT_ARG_COUNT("Array::select(closure)", 1);
    ASSERT_ARG_TYPE("Array::select(closure)", 0, Closure);
    ObjArray* self = AS_ARRAY(receiver);
    ObjClosure* closure = AS_CLOSURE(args[0]);

    ObjArray* selected = newArray(vm);
    push(vm, OBJ_VAL(selected));
    for (int i = 0; i < self->elements.count; i++) {
        Value result = callReentrant(vm, OBJ_VAL(closure), self->elements.values[i]);
        if (!isFalsey(result)) valueArrayWrite(vm, &selected->elements, self->elements.values[i]);
    }
    pop(vm);
    RETURN_OBJ(selected);
}

LOX_METHOD(Array, slice) {
    ASSERT_ARG_COUNT("Array::slice(from, to)", 2);
    ASSERT_ARG_TYPE("Array::slice(from, to)", 0, Int);
    ASSERT_ARG_TYPE("Array::slice(from, to)", 1, Int);
    ObjArray* self = AS_ARRAY(receiver);
    int fromIndex = AS_INT(args[0]);
    int toIndex = AS_INT(args[1]);
    assertIntWithinRange(vm, "Array::slice(from, to)", fromIndex, 0, self->elements.count, 0);
    assertIntWithinRange(vm, "Array::slice(from, to)", toIndex, fromIndex, self->elements.count, 1);
    RETURN_OBJ(arrayCopy(vm, self->elements, fromIndex, toIndex));
}

LOX_METHOD(Array, toString) {
    ASSERT_ARG_COUNT("Array::toString()", 0);
    RETURN_OBJ(valueArrayToString(vm, &AS_ARRAY(receiver)->elements));
}

LOX_METHOD(ArrayClass, fromElements) {
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    for (int i = 0; i < argCount; i++) {
        valueArrayWrite(vm, &array->elements, args[i]);
    }
    pop(vm);
    RETURN_OBJ(array);
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

LOX_METHOD(Dictionary, entrySet) {
    ASSERT_ARG_COUNT("Dictionary::entrySet()", 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    ObjDictionary* entryDict = newDictionary(vm);

    push(vm, OBJ_VAL(entryDict));
    for (int i = 0; i < self->count; i++) {
        ObjEntry* entry = &self->entries[i];
        dictSet(vm, entryDict, OBJ_VAL(entry), NIL_VAL);
    }
    pop(vm);
    
    ObjInstance* entrySet = newInstance(vm, getNativeClass(vm, "Set"));
    setObjProperty(vm, entrySet, "dict", OBJ_VAL(entryDict));
    RETURN_OBJ(entrySet);
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

LOX_METHOD(Dictionary, keySet) {
    ASSERT_ARG_COUNT("Dictionary::keySet()", 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    ObjDictionary* keyDict = dictCopy(vm, self);

    push(vm, OBJ_VAL(keyDict));
    for (int i = 0; i < keyDict->count; i++) {
        ObjEntry* entry = &keyDict->entries[i];
        entry->value = NIL_VAL;
    }
    pop(vm);
    
    ObjInstance* keySet = newInstance(vm, getNativeClass(vm, "Set"));
    setObjProperty(vm, keySet, "dict", OBJ_VAL(keyDict));
    RETURN_OBJ(keySet);
}

LOX_METHOD(Dictionary, length) {
    ASSERT_ARG_COUNT("Dictionary::length()", 0);
    RETURN_INT(AS_DICTIONARY(receiver)->count);
}

LOX_METHOD(Dictionary, next) {
    ASSERT_ARG_COUNT("Dictionary::next(index)", 1);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    if (self->count == 0) RETURN_NIL;

    int index = 0;
    if (!IS_NIL(args[0])) {
        Value key = args[0];
        index = dictFindIndex(self, key);
        if (index < 0 || index >= self->capacity) RETURN_NIL;
        index++;
    }

    for (; index < self->capacity; index++) {
        if (!IS_UNDEFINED(self->entries[index].key)) RETURN_VAL(self->entries[index].key);
    }
    RETURN_NIL;
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
    if(!IS_NIL(args[0])) dictSet(vm, AS_DICTIONARY(receiver), args[0], args[1]);
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

LOX_METHOD(Dictionary, valueSet) {
    ASSERT_ARG_COUNT("Dictionary::valueSet()", 0);
    ObjDictionary* self = AS_DICTIONARY(receiver);
    ObjDictionary* valueDict = newDictionary(vm);

    push(vm, OBJ_VAL(valueDict));
    for (int i = 0; i < self->count; i++) {
        ObjEntry* entry = &self->entries[i];
        dictSet(vm, valueDict, entry->value, NIL_VAL);
    }
    pop(vm);
    
    ObjInstance* valueSet = newInstance(vm, getNativeClass(vm, "Set"));
    setObjProperty(vm, valueSet, "dict", OBJ_VAL(valueDict));
    RETURN_OBJ(valueSet);
}

LOX_METHOD(Entry, clone) {
    ASSERT_ARG_COUNT("Entry::clone()", 0);
    ObjEntry* self = AS_ENTRY(receiver);
    ObjEntry* entry = newEntry(vm, self->key, self->value);
    RETURN_OBJ(entry);
}

LOX_METHOD(Entry, getKey) {
    ASSERT_ARG_COUNT("Entry::getKey()", 0);
    ObjEntry* self = AS_ENTRY(receiver);
    RETURN_VAL(self->key);
}

LOX_METHOD(Entry, getValue) {
    ASSERT_ARG_COUNT("Entry::getValue()", 0);
    ObjEntry* self = AS_ENTRY(receiver);
    RETURN_VAL(self->value);
}

LOX_METHOD(Entry, init) {
    ASSERT_ARG_COUNT("Entry::init(key, value)", 2);
    ObjEntry* self = AS_ENTRY(receiver);
    self->key = args[0];
    self->value = args[1];
    RETURN_OBJ(self);
}

LOX_METHOD(Entry, setValue) {
    ASSERT_ARG_COUNT("Entry::setValue(value)", 1);
    ObjEntry* self = AS_ENTRY(receiver);
    self->value = args[0];
    RETURN_VAL(self->value);
}

LOX_METHOD(Entry, toString) {
    ASSERT_ARG_COUNT("Entry::toString()", 0);
    ObjEntry* self = AS_ENTRY(receiver);
    char string[UINT8_MAX] = "";
    Value key = self->key;
    char* keyChars = valueToString(vm, key);
    size_t keyLength = strlen(keyChars);
    Value value = self->value;
    char* valueChars = valueToString(vm, value);
    size_t valueLength = strlen(valueChars);

    size_t offset = 0;
    memcpy(string, keyChars, keyLength);
    offset += keyLength;
    memcpy(string + offset, ": ", 2);
    offset += 2;
    memcpy(string + offset, valueChars, valueLength);
    offset += valueLength;
    string[offset + 1] = '\0';
    RETURN_STRING(string, (int)offset + 1);
}

LOX_METHOD(LinkedList, add) {
    ASSERT_ARG_COUNT("LinkedList::add(element)", 1);
    linkAddLast(vm, AS_INSTANCE(receiver), args[0]);
    RETURN_TRUE;
}

LOX_METHOD(LinkedList, addAt) {
    ASSERT_ARG_COUNT("LinkedList::addAt(index, element)", 2);
    ASSERT_ARG_TYPE("LinkedList::addAt(index, element)", 0, Int);
    ObjInstance* self = AS_INSTANCE(receiver);
    int index = AS_INT(args[0]);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    if (index == length) linkAddLast(vm, self, args[1]);
    else {
        linkIndexValidate(vm, self, index);
        linkAddBefore(vm, self, args[1], linkNode(vm, self, index));
    }
    RETURN_VAL(args[1]);
}

LOX_METHOD(LinkedList, addFirst) {
    ASSERT_ARG_COUNT("LinkedList::addFirst(element)", 1);
    linkAddFirst(vm, AS_INSTANCE(receiver), args[0]);
    RETURN_NIL;
}

LOX_METHOD(LinkedList, addLast) {
    ASSERT_ARG_COUNT("LinkedList::addLast(element)", 1);
    linkAddLast(vm, AS_INSTANCE(receiver), args[0]);
    RETURN_NIL;
}

LOX_METHOD(LinkedList, clear) {
    ASSERT_ARG_COUNT("LinkedList::clear()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "first", NIL_VAL);
    setObjProperty(vm, self, "last", NIL_VAL);
    setObjProperty(vm, self, "current", NIL_VAL);
    setObjProperty(vm, self, "length", INT_VAL(0));
    RETURN_NIL;
}

LOX_METHOD(LinkedList, contains) {
    ASSERT_ARG_COUNT("LinkedList::contains(element)", 1);
    RETURN_BOOL(linkFindIndex(vm, AS_INSTANCE(receiver), args[0]) != -1);
}

LOX_METHOD(LinkedList, getAt) {
    ASSERT_ARG_COUNT("LinkedList::getAt(index)", 1);
    ASSERT_ARG_TYPE("LinkedList::getAt(index)", 0, Int);
    RETURN_VAL(linkNode(vm, AS_INSTANCE(receiver), AS_INT(args[0]))->element);
}

LOX_METHOD(LinkedList, getFirst) {
    ASSERT_ARG_COUNT("LinkedList::getFirst()", 0);
    ObjNode* first = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "first"));
    RETURN_VAL(first->element);
}

LOX_METHOD(LinkedList, getLast) {
    ASSERT_ARG_COUNT("LinkedList::getLast()", 0);
    ObjNode* last = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "last"));
    RETURN_VAL(last->element);
}

LOX_METHOD(LinkedList, indexOf) {
    ASSERT_ARG_COUNT("LinkedList::indexOf(element)", 1);
    RETURN_INT(linkFindIndex(vm, AS_INSTANCE(receiver), args[0]));
}

LOX_METHOD(LinkedList, init) {
    ASSERT_ARG_COUNT("LinkedList::init()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "first", NIL_VAL);
    setObjProperty(vm, self, "last", NIL_VAL);
    setObjProperty(vm, self, "current", NIL_VAL);
    setObjProperty(vm, self, "length", INT_VAL(0));
    RETURN_OBJ(self);
}

LOX_METHOD(LinkedList, isEmpty) {
    ASSERT_ARG_COUNT("LinkedList::isEmpty()", 0);
    RETURN_BOOL(collectionIsEmpty(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(LinkedList, lastIndexOf) {
    ASSERT_ARG_COUNT("LinkedList::lastIndexOf(element)", 1);
    RETURN_INT(linkFindLastIndex(vm, AS_INSTANCE(receiver), args[0]));
}

LOX_METHOD(LinkedList, length) {
    ASSERT_ARG_COUNT("LinkedList::length()", 0);
    Value length = getObjProperty(vm, AS_INSTANCE(receiver), "length");
    RETURN_INT(length);
}

LOX_METHOD(LinkedList, next) {
    ASSERT_ARG_COUNT("LinkedList::next(index)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    if (IS_NIL(args[0])) {
        if (length == 0) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("LinkedList::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index >= 0 && index < length - 1) {
        ObjNode* current = AS_NODE(getObjProperty(vm, self, (index == 0) ? "first" : "current"));
        setObjProperty(vm, self, "current", OBJ_VAL(current->next));
        RETURN_INT(index + 1);
    }
    else {
        setObjProperty(vm, self, "current", getObjProperty(vm, self, "first"));
        RETURN_NIL;
    }
}

LOX_METHOD(LinkedList, nextValue) {
    ASSERT_ARG_COUNT("LinkedList::nextValue(index)", 1);
    ASSERT_ARG_TYPE("LinkedList::nextValue(index)", 0, Int);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    int index = AS_INT(args[0]);
    if (index == 0) RETURN_VAL(getObjProperty(vm, self, "first"));
    if (index > 0 && index < length) RETURN_VAL(getObjProperty(vm, self, "current"));
    RETURN_NIL;
}

LOX_METHOD(LinkedList, node) {
    ASSERT_ARG_COUNT("LinkedList::node(index)", 1);
    ASSERT_ARG_TYPE("LinkedList::node(index)", 0, Int);
    RETURN_OBJ(linkNode(vm, AS_INSTANCE(receiver), AS_INT(args[0])));
}

LOX_METHOD(LinkedList, peek) {
    ASSERT_ARG_COUNT("LinkedList::peek()", 0);
    ObjNode* first = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "first"));
    RETURN_VAL(first->element);
}

LOX_METHOD(LinkedList, putAt) {
    ASSERT_ARG_COUNT("LinkedList::putAt(index, element)", 2);
    ASSERT_ARG_TYPE("LinkedList::putAt(index, element)", 0, Int);
    ObjInstance* self = AS_INSTANCE(receiver);
    int index = AS_INT(args[0]);
    linkIndexValidate(vm, self, index);
    ObjNode* node = linkNode(vm, self, index);
    Value old = node->element;
    node->element = args[1];
    RETURN_VAL(old);
}

LOX_METHOD(LinkedList, remove) {
    ASSERT_ARG_COUNT("LinkedList::remove()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value first = getObjProperty(vm, self, "first");
    RETURN_VAL(linkRemoveFirst(vm, self, IS_NIL(first) ? NULL : AS_NODE(first)));
}

LOX_METHOD(LinkedList, removeFirst) {
    ASSERT_ARG_COUNT("LinkedList::removeFirst()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value first = getObjProperty(vm, self, "first");
    RETURN_VAL(linkRemoveFirst(vm, self, IS_NIL(first) ? NULL : AS_NODE(first)));
}

LOX_METHOD(LinkedList, removeLast) {
    ASSERT_ARG_COUNT("LinkedList::removeLast()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    Value last = getObjProperty(vm, self, "last");
    RETURN_VAL(linkRemoveLast(vm, self, IS_NIL(last) ? NULL : AS_NODE(last)));
}

LOX_METHOD(LinkedList, toArray) {
    ASSERT_ARG_COUNT("LinkedList::toArray()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, AS_INSTANCE(receiver), "length"));
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));

    if (length > 0) {
        for (ObjNode* node = AS_NODE(getObjProperty(vm, self, "first")); node != NULL; node = node->next) {
            valueArrayWrite(vm, &array->elements, node->element);
        }
    }
    pop(vm);
    RETURN_OBJ(array);
}

LOX_METHOD(LinkedList, toString) {
    ASSERT_ARG_COUNT("LinkedList::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_OBJ(linkToString(vm, self));
}

LOX_METHOD(List, eachIndex) {
    ASSERT_ARG_COUNT("List::eachIndex(closure)", 1);
    ASSERT_ARG_TYPE("List::eachIndex(closure)", 0, Closure);
    ObjClosure* closure = AS_CLOSURE(args[0]);
    Value index = INT_VAL(0);
    Value nextMethod = getObjMethod(vm, receiver, "next");
    Value nextValueMethod = getObjMethod(vm, receiver, "nextValue");

    while (index != NIL_VAL) {
        push(vm, receiver);
        Value element = callReentrant(vm, nextValueMethod, index);
        callReentrant(vm, OBJ_VAL(closure), index, element);
        push(vm, receiver);
        index = callReentrant(vm, nextMethod, index);
    }
    RETURN_NIL;
}

LOX_METHOD(List, getAt) {
    ASSERT_ARG_COUNT("List::getAt(index)", 1);
    ASSERT_ARG_TYPE("List::getAt(index)", 0, Int);
    int position = AS_INT(args[0]);
    Value index = INT_VAL(0);
    Value nextMethod = getObjMethod(vm, receiver, "next");
    Value nextValueMethod = getObjMethod(vm, receiver, "nextValue");

    while (index != NIL_VAL) {
        push(vm, receiver);
        Value element = callReentrant(vm, nextValueMethod, index);
        if (index == position) RETURN_VAL(element);
        push(vm, receiver);
        index = callReentrant(vm, nextMethod, index);
    }
    RETURN_NIL;
}

LOX_METHOD(List, putAt) {
    raiseError(vm, "Not implemented, subclass responsibility.");
    RETURN_NIL;
}

LOX_METHOD(Node, clone) {
    ASSERT_ARG_COUNT("Node::clone()", 0);
    ObjNode* self = AS_NODE(receiver);
    ObjNode* node = newNode(vm, self->element, self->prev, self->next);
    RETURN_OBJ(node);
}

LOX_METHOD(Node, element) {
    ASSERT_ARG_COUNT("Node::element()", 0);
    RETURN_VAL(AS_NODE(receiver)->element);
}

LOX_METHOD(Node, init) {
    ASSERT_ARG_COUNT("Node::init(element, prev, next)", 3);
    ObjNode* self = AS_NODE(receiver);
    self->element = args[0];
    if (!IS_NIL(args[1])) self->prev = AS_NODE(args[1]);
    if (!IS_NIL(args[2])) self->next = AS_NODE(args[2]);
    RETURN_OBJ(self);
}

LOX_METHOD(Node, next) {
    ASSERT_ARG_COUNT("Node::next()", 0);
    RETURN_OBJ(AS_NODE(receiver)->next);
}

LOX_METHOD(Node, prev) {
    ASSERT_ARG_COUNT("Node::prev()", 0);
    RETURN_OBJ(AS_NODE(receiver)->prev);
}

LOX_METHOD(Node, toString) {
    ASSERT_ARG_COUNT("Node::toString()", 0);
    char nodeString[UINT8_MAX] = "";
    memcpy(nodeString, "Node: ", 6);

    Value nodeElement = AS_NODE(receiver)->element;
    char* nodeChars = valueToString(vm, nodeElement);
    size_t nodeLength = strlen(nodeChars);
    memcpy(nodeString + 6, nodeChars, nodeLength);
    nodeString[nodeLength + 6] = '\0';
    RETURN_STRING(nodeString, (int)nodeLength + 6);
}

LOX_METHOD(Queue, clear) {
    ASSERT_ARG_COUNT("Queue::clear()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "first", OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL)));
    setObjProperty(vm, self, "last", OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL)));
    setObjProperty(vm, self, "current", NIL_VAL);
    setObjProperty(vm, self, "length", INT_VAL(0));
    RETURN_NIL;
}

LOX_METHOD(Queue, contains) {
    ASSERT_ARG_COUNT("Queue::contains(element)", 1);
    RETURN_BOOL(linkFindIndex(vm, AS_INSTANCE(receiver), args[0]) != -1);
}

LOX_METHOD(Queue, dequeue) {
    ASSERT_ARG_COUNT("Queue::dequeue()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjNode* first = AS_NODE(getObjProperty(vm, self, "first"));
    int length = AS_INT(getObjProperty(vm, AS_INSTANCE(receiver), "length"));
    if (length == 0) RETURN_NIL;

    ObjNode* node = first;
    setObjProperty(vm, self, "first", OBJ_VAL(first->next));
    if (first->next == NULL) setObjProperty(vm, self, "last", OBJ_VAL(first->next));
    collectionLengthDecrement(vm, self);
    RETURN_VAL(first->element);
}

LOX_METHOD(Queue, enqueue) {
    ASSERT_ARG_COUNT("Queue::enqueue(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjNode* first = AS_NODE(getObjProperty(vm, self, "first"));
    ObjNode* last = AS_NODE(getObjProperty(vm, self, "last"));
    int length = AS_INT(getObjProperty(vm, AS_INSTANCE(receiver), "length"));

    ObjNode* new = newNode(vm, args[0], NULL, NULL);
    push(vm, OBJ_VAL(new));
    if (length == 0) {
        setObjProperty(vm, self, "first", OBJ_VAL(new));
        setObjProperty(vm, self, "last", OBJ_VAL(new));
    }
    else {
        last->next = new;
        setObjProperty(vm, self, "last", OBJ_VAL(new));
    }
    pop(vm);
    collectionLengthIncrement(vm, self);
    RETURN_VAL(args[0]);
}

LOX_METHOD(Queue, getFirst) {
    ASSERT_ARG_COUNT("Queue::getFirst()", 0);
    ObjNode* first = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "first"));
    RETURN_VAL(first->element);
}

LOX_METHOD(Queue, getLast) {
    ASSERT_ARG_COUNT("Queue::getLast()", 0);
    ObjNode* last = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "last"));
    RETURN_VAL(last->element);
}

LOX_METHOD(Queue, init) {
    ASSERT_ARG_COUNT("Queue::init()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "first", OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL)));
    setObjProperty(vm, self, "last", OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL)));
    setObjProperty(vm, self, "current", NIL_VAL);
    setObjProperty(vm, self, "length", INT_VAL(0));
    RETURN_OBJ(receiver);
}

LOX_METHOD(Queue, isEmpty) {
    ASSERT_ARG_COUNT("Queue::isEmpty()", 0);
    RETURN_BOOL(collectionIsEmpty(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Queue, length) {
    ASSERT_ARG_COUNT("Queue::length()", 0);
    Value length = getObjProperty(vm, AS_INSTANCE(receiver), "length");
    RETURN_INT(length);
}

LOX_METHOD(Queue, next) {
    ASSERT_ARG_COUNT("Queue::next(index)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    if (IS_NIL(args[0])) {
        if (length == 0) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("Stack::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index >= 0 && index < length - 1) {
        ObjNode* current = AS_NODE(getObjProperty(vm, self, (index == 0) ? "first" : "current"));
        setObjProperty(vm, self, "current", OBJ_VAL(current->next));
        RETURN_INT(index + 1);
    }
    else {
        setObjProperty(vm, self, "current", getObjProperty(vm, self, "first"));
        RETURN_NIL;
    }
}

LOX_METHOD(Queue, nextValue) {
    ASSERT_ARG_COUNT("Queue::nextValue(index)", 1);
    ASSERT_ARG_TYPE("Queue::nextValue(index)", 0, Int);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    int index = AS_INT(args[0]);
    if (index == 0) RETURN_VAL(getObjProperty(vm, self, "first"));
    if (index > 0 && index < length) RETURN_VAL(getObjProperty(vm, self, "current"));
    RETURN_NIL;
}

LOX_METHOD(Queue, peek) {
    ASSERT_ARG_COUNT("Queue::peek()", 0);
    ObjNode* first = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "first"));
    RETURN_VAL(first->element);
}

LOX_METHOD(Queue, search) {
    ASSERT_ARG_COUNT("Queue::search(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_INT(linkSearchElement(vm, self, args[0]));
}

LOX_METHOD(Queue, toArray) {
    ASSERT_ARG_COUNT("Queue::toArray()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    if (length > 0) {
        for (ObjNode* node = AS_NODE(getObjProperty(vm, self, "first")); node != NULL; node = node->next) {
            valueArrayWrite(vm, &array->elements, node->element);
        }
    }
    pop(vm);
    RETURN_OBJ(array);
}

LOX_METHOD(Queue, toString) {
    ASSERT_ARG_COUNT("Queue::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_OBJ(linkToString(vm, self));
}

LOX_METHOD(Range, add) {
    raiseError(vm, "Cannot add an element to instance of class Range.");
    RETURN_NIL;
}

LOX_METHOD(Range, addAll) {
    raiseError(vm, "Cannot add a collection to instance of class Range.");
    RETURN_NIL;
}

LOX_METHOD(Range, clone) {
    ASSERT_ARG_COUNT("Range::clone()", 0);
    ObjRange* self = AS_RANGE(receiver);
    ObjRange* range = newRange(vm, self->from, self->to);
    RETURN_OBJ(range);
}

LOX_METHOD(Range, contains) {
    ASSERT_ARG_COUNT("Range::contains(element)", 1);
    if (!IS_INT(args[0])) RETURN_FALSE;
    ObjRange* self = AS_RANGE(receiver);
    int element = AS_INT(args[0]);
    if (self->from < self->to) RETURN_BOOL(element >= self->from && element <= self->to);
    else RETURN_BOOL(element >= self->to && element <= self->from);
}

LOX_METHOD(Range, from) {
    ASSERT_ARG_COUNT("Range::from()", 0);
    ObjRange* self = AS_RANGE(receiver);
    RETURN_INT(self->from);
}

LOX_METHOD(Range, getAt) {
    ASSERT_ARG_COUNT("Range::getAt(index)", 1);
    ASSERT_ARG_TYPE("Range::getAt(index)", 0, Int);
    ObjRange* self = AS_RANGE(receiver);
    int index = AS_INT(args[0]);

    int min = (self->from < self->to) ? self->from : self->to;
    int max = (self->from < self->to) ? self->to : self->from;
    assertIntWithinRange(vm, "Range::getAt(index)", index, min, max, 0);
    RETURN_INT(self->from + index);
}

LOX_METHOD(Range, init) {
    ASSERT_ARG_COUNT("Range::init(from, to)", 2);
    ASSERT_ARG_TYPE("Range::init(from, to)", 0, Int);
    ASSERT_ARG_TYPE("Range::init(from, to)", 0, Int);
    int from = AS_INT(args[0]);
    int to = AS_INT(args[1]);

    ObjRange* self = AS_RANGE(receiver);
    self->from = from;
    self->to = to;
    RETURN_OBJ(self);
}

LOX_METHOD(Range, length) {
    ASSERT_ARG_COUNT("Range::length()", 0);
    ObjRange* self = AS_RANGE(receiver);
    RETURN_INT(abs(self->to - self->from) + 1);
}

LOX_METHOD(Range, max) {
    ASSERT_ARG_COUNT("Range::max()", 0);
    ObjRange* self = AS_RANGE(receiver);
    RETURN_INT((self->from < self->to) ? self->to : self->from);
}

LOX_METHOD(Range, min) {
    ASSERT_ARG_COUNT("Range::min()", 0);
    ObjRange* self = AS_RANGE(receiver);
    RETURN_INT((self->from < self->to) ? self->from : self->to);
}

LOX_METHOD(Range, next) {
    ASSERT_ARG_COUNT("Range::next(index)", 1);
    ObjRange* self = AS_RANGE(receiver);
    if (IS_NIL(args[0])) {
        if (self->from == self->to) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("Range::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index < 0 || index < abs(self->to - self->from)) RETURN_INT(index + 1);
    RETURN_NIL;
}

LOX_METHOD(Range, nextValue) {
    ASSERT_ARG_COUNT("Range::nextValue(index)", 1);
    ASSERT_ARG_TYPE("Range::nextValue(index)", 0, Int);
    ObjRange* self = AS_RANGE(receiver);
    int index = AS_INT(args[0]);

    int step = (self->from < self->to) ? index : -index;
    if (index > -1 && index < abs(self->to - self->from) + 1) RETURN_INT(self->from + step);
    RETURN_NIL;
}

LOX_METHOD(Range, step) {
    ASSERT_ARG_COUNT("Range::step(by, closure)", 2);
    ASSERT_ARG_TYPE("Range::step(by, closure)", 0, Number);
    ASSERT_ARG_TYPE("Range::step(by, closure)", 1, Closure);
    ObjRange* self = AS_RANGE(receiver);
    double from = self->from;
    double to = self->to;
    double by = AS_NUMBER(args[1]);
    ObjClosure* closure = AS_CLOSURE(args[2]);

    if (by == 0) raiseError(vm, "Step size cannot be 0");
    else {
        if (by > 0) {
            for (double num = from; num <= to; num += by) {
                callReentrant(vm, OBJ_VAL(closure), NUMBER_VAL(num));
            }
        }
        else {
            for (double num = from; num >= to; num += by) {
                callReentrant(vm, OBJ_VAL(closure), NUMBER_VAL(num));
            }
        }
    }
    RETURN_NIL;
}

LOX_METHOD(Range, to) {
    ASSERT_ARG_COUNT("Range::to()", 0);
    ObjRange* self = AS_RANGE(receiver);
    RETURN_INT(self->to);
}

LOX_METHOD(Range, toArray) {
    ASSERT_ARG_COUNT("Range::toArray()", 0);
    ObjRange* self = AS_RANGE(receiver);
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));

    if (self->from < self->to) {
        for (int i = self->from; i <= self->to; i++) {
            valueArrayWrite(vm, &array->elements, INT_VAL(i));
        }
    }
    else { 
        for (int i = self->to; i >= self->from; i--) {
            valueArrayWrite(vm, &array->elements, INT_VAL(i));
        }
    }

    pop(vm);
    RETURN_OBJ(array);
}

LOX_METHOD(Range, toString) {
    ASSERT_ARG_COUNT("Range::toString()", 0);
    ObjRange* self = AS_RANGE(receiver);
    RETURN_STRING_FMT("%d..%d", self->from, self->to);
}

LOX_METHOD(Set, add) {
    ASSERT_ARG_COUNT("Set::add(element)", 1);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    if(!IS_NIL(args[0])) dictSet(vm, dict, args[0], NIL_VAL);
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
    RETURN_INT(dict->count);
}

LOX_METHOD(Set, next) {
    ASSERT_ARG_COUNT("Set::next(index)", 1);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    if (dict->count == 0) RETURN_NIL;

    int index = 0;
    if (!IS_NIL(args[0])) {
        index = dictFindIndex(dict, args[0]);
        if (index < 0 || index >= dict->capacity) RETURN_NIL;
        index++;
    }

    for (; index < dict->capacity; index++) {
        if (!IS_UNDEFINED(dict->entries[index].key)) {
            RETURN_VAL(dict->entries[index].key);
        }
    }
    RETURN_NIL;
}

LOX_METHOD(Set, nextValue) {
    ASSERT_ARG_COUNT("Set::nextValue(index)", 1);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, AS_INSTANCE(receiver), "dict"));
    int index = dictFindIndex(dict, args[0]);
    RETURN_VAL(dict->entries[index].key);
}

LOX_METHOD(Set, remove) {
    ASSERT_ARG_COUNT("Set::remove(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, self, "dict"));
    Value key = args[0];
    Value value;

    bool keyExists = dictGet(dict, key, &value);
    if (!keyExists) RETURN_NIL;
    dictDelete(dict, key);
    RETURN_VAL(value);
}

LOX_METHOD(Set, toArray) {
    ASSERT_ARG_COUNT("Set::toArray()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjDictionary* dict = AS_DICTIONARY(getObjProperty(vm, self, "dict"));
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    for (int i = 0; i < dict->count; i++) {
        ObjEntry* entry = &dict->entries[i];
        if (!IS_UNDEFINED(entry->key)) valueArrayWrite(vm, &array->elements, entry->key);
    }
    pop(vm);
    RETURN_OBJ(array);
}

LOX_METHOD(Set, toString) {
    ASSERT_ARG_COUNT("Set::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_OBJ(setToString(vm, self));
}

LOX_METHOD(Stack, clear) {
    ASSERT_ARG_COUNT("Stack::clear()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "first", NIL_VAL);
    setObjProperty(vm, self, "current", NIL_VAL);
    setObjProperty(vm, self, "length", INT_VAL(0));
    RETURN_NIL;
}

LOX_METHOD(Stack, contains) {
    ASSERT_ARG_COUNT("Stack::contains(element)", 1);
    RETURN_BOOL(linkFindIndex(vm, AS_INSTANCE(receiver), args[0]) != -1);
}

LOX_METHOD(Stack, getFirst) { 
    ASSERT_ARG_COUNT("Stack::getFirst()", 0);
    ObjNode* first = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "first"));
    RETURN_VAL(first->element);
}

LOX_METHOD(Stack, init) {
    ASSERT_ARG_COUNT("Stack::init()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    setObjProperty(vm, self, "first", OBJ_VAL(newNode(vm, NIL_VAL, NULL, NULL)));
    setObjProperty(vm, self, "current", NIL_VAL);
    setObjProperty(vm, self, "length", INT_VAL(0));
    RETURN_OBJ(receiver);
}

LOX_METHOD(Stack, isEmpty) {
    ASSERT_ARG_COUNT("Stack::isEmpty()", 0);
    RETURN_BOOL(collectionIsEmpty(vm, AS_INSTANCE(receiver)));
}

LOX_METHOD(Stack, length) {
    ASSERT_ARG_COUNT("Stack::length()", 0);
    Value length = getObjProperty(vm, AS_INSTANCE(receiver), "length");
    RETURN_INT(length);
}

LOX_METHOD(Stack, next) {
    ASSERT_ARG_COUNT("Stack::next(index)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    if (IS_NIL(args[0])) {
        if (length == 0) RETURN_FALSE;
        RETURN_INT(0);
    }

    ASSERT_ARG_TYPE("Stack::next(index)", 0, Int);
    int index = AS_INT(args[0]);
    if (index >= 0 && index < length - 1) {
        ObjNode* current = AS_NODE(getObjProperty(vm, self, (index == 0) ? "first" : "current"));
        setObjProperty(vm, self, "current", OBJ_VAL(current->next));
        RETURN_INT(index + 1);
    }
    else {
        setObjProperty(vm, self, "current", getObjProperty(vm, self, "first"));
        RETURN_NIL;
    }
}

LOX_METHOD(Stack, nextValue) {
    ASSERT_ARG_COUNT("Stack::nextValue(index)", 1);
    ASSERT_ARG_TYPE("Stack::nextValue(index)", 0, Int);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    int index = AS_INT(args[0]);
    if (index == 0) RETURN_VAL(getObjProperty(vm, self, "first"));
    if (index > 0 && index < length) RETURN_VAL(getObjProperty(vm, self, "current"));
    RETURN_NIL;
}

LOX_METHOD(Stack, peek) {
    ASSERT_ARG_COUNT("Stack::peek()", 0);
    ObjNode* first = AS_NODE(getObjProperty(vm, AS_INSTANCE(receiver), "first"));
    RETURN_VAL(first->element);
}

LOX_METHOD(Stack, pop) {
    ASSERT_ARG_COUNT("Stack::pop()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjNode* first = AS_NODE(getObjProperty(vm, self, "first"));
    int length = AS_INT(getObjProperty(vm, AS_INSTANCE(receiver), "length"));
    if (length == 0) RETURN_NIL;
    else {
        Value element = first->element;
        setObjProperty(vm, self, "first", first->next == NULL ? NIL_VAL : OBJ_VAL(first->next));
        collectionLengthDecrement(vm, self);
        RETURN_VAL(element);
    }
}

LOX_METHOD(Stack, push) {
    ASSERT_ARG_COUNT("Stack::push(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    ObjNode* first = AS_NODE(getObjProperty(vm, self, "first"));
    int length = AS_INT(getObjProperty(vm, AS_INSTANCE(receiver), "length"));
    ObjNode* new = newNode(vm, args[0], NULL, NULL);

    push(vm, OBJ_VAL(new));
    if (length > 0) {
        new->next = first;
    }
    setObjProperty(vm, self, "first", OBJ_VAL(new));
    pop(vm);
    
    collectionLengthIncrement(vm, self);
    RETURN_VAL(args[0]);
}

LOX_METHOD(Stack, search) {
    ASSERT_ARG_COUNT("Stack::search(element)", 1);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_INT(linkSearchElement(vm, self, args[0]));
}

LOX_METHOD(Stack, toArray) {
    ASSERT_ARG_COUNT("Stack::toArray()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    int length = AS_INT(getObjProperty(vm, self, "length"));
    ObjArray* array = newArray(vm);
    push(vm, OBJ_VAL(array));
    if (length > 0) {
        for (ObjNode* node = AS_NODE(getObjProperty(vm, self, "first")); node != NULL; node = node->next) {
            valueArrayWrite(vm, &array->elements, node->element);
        }
    }
    pop(vm);
    RETURN_OBJ(array);
}

LOX_METHOD(Stack, toString) {
    ASSERT_ARG_COUNT("Stack::toString()", 0);
    ObjInstance* self = AS_INSTANCE(receiver);
    RETURN_OBJ(linkToString(vm, self));
}

void registerCollectionPackage(VM* vm) {
    loadSourceFile(vm, "src/std/collection.lox");
    ObjClass* collectionClass = getNativeClass(vm, "Collection");
    bindSuperclass(vm, collectionClass, vm->objectClass);

    ObjClass* listClass = defineNativeClass(vm, "List");
    bindSuperclass(vm, listClass, collectionClass);
    DEF_METHOD(listClass, List, eachIndex, 1);
    DEF_METHOD(listClass, List, getAt, 1);
    DEF_METHOD(listClass, List, putAt, 2);

    vm->arrayClass = defineNativeClass(vm, "Array");
    bindSuperclass(vm, vm->arrayClass, listClass);
    DEF_METHOD(vm->arrayClass, Array, add, 1);
    DEF_METHOD(vm->arrayClass, Array, addAll, 1);
    DEF_METHOD(vm->arrayClass, Array, clear, 0);
    DEF_METHOD(vm->arrayClass, Array, clone, 0);
    DEF_METHOD(vm->arrayClass, Array, collect, 1);
    DEF_METHOD(vm->arrayClass, Array, contains, 1);
    DEF_METHOD(vm->arrayClass, Array, detect, 1);
    DEF_METHOD(vm->arrayClass, Array, each, 1);
    DEF_METHOD(vm->arrayClass, Array, equals, 1);
    DEF_METHOD(vm->arrayClass, Array, getAt, 1);
    DEF_METHOD(vm->arrayClass, Array, indexOf, 1);
    DEF_METHOD(vm->arrayClass, Array, init, 0);
    DEF_METHOD(vm->arrayClass, Array, insertAt, 2);
    DEF_METHOD(vm->arrayClass, Array, isEmpty, 0);
    DEF_METHOD(vm->arrayClass, Array, lastIndexOf, 1);
    DEF_METHOD(vm->arrayClass, Array, length, 0);
    DEF_METHOD(vm->arrayClass, Array, next, 1);
    DEF_METHOD(vm->arrayClass, Array, nextValue, 1);
    DEF_METHOD(vm->arrayClass, Array, putAt, 2);
    DEF_METHOD(vm->arrayClass, Array, reject, 1);
    DEF_METHOD(vm->arrayClass, Array, remove, 1);
    DEF_METHOD(vm->arrayClass, Array, removeAt, 1);
    DEF_METHOD(vm->arrayClass, Array, select, 1);
    DEF_METHOD(vm->arrayClass, Array, slice, 2);
    DEF_METHOD(vm->arrayClass, Array, toString, 0);

    ObjClass* arrayMetaclass = vm->arrayClass->obj.klass;
    DEF_METHOD(arrayMetaclass, ArrayClass, fromElements, -1);

    ObjClass* linkedListClass = defineNativeClass(vm, "LinkedList");
    bindSuperclass(vm, linkedListClass, listClass);
    DEF_METHOD(linkedListClass, LinkedList, add, 1);
    DEF_METHOD(linkedListClass, LinkedList, addAt, 2);
    DEF_METHOD(linkedListClass, LinkedList, addFirst, 1);
    DEF_METHOD(linkedListClass, LinkedList, addLast, 1);
    DEF_METHOD(linkedListClass, LinkedList, clear, 0);
    DEF_METHOD(linkedListClass, LinkedList, contains, 1);
    DEF_METHOD(linkedListClass, LinkedList, getAt, 1);
    DEF_METHOD(linkedListClass, LinkedList, getFirst, 0);
    DEF_METHOD(linkedListClass, LinkedList, getLast, 0);
    DEF_METHOD(linkedListClass, LinkedList, indexOf, 1);
    DEF_METHOD(linkedListClass, LinkedList, init, 0);
    DEF_METHOD(linkedListClass, LinkedList, isEmpty, 0);
    DEF_METHOD(linkedListClass, LinkedList, lastIndexOf, 0);
    DEF_METHOD(linkedListClass, LinkedList, length, 0);
    DEF_METHOD(linkedListClass, LinkedList, next, 1);
    DEF_METHOD(linkedListClass, LinkedList, nextValue, 1);
    DEF_METHOD(linkedListClass, LinkedList, node, 1);
    DEF_METHOD(linkedListClass, LinkedList, peek, 0);
    DEF_METHOD(linkedListClass, LinkedList, putAt, 2);
    DEF_METHOD(linkedListClass, LinkedList, remove, 0);
    DEF_METHOD(linkedListClass, LinkedList, removeFirst, 0);
    DEF_METHOD(linkedListClass, LinkedList, removeLast, 0);
    DEF_METHOD(linkedListClass, LinkedList, toArray, 0);
    DEF_METHOD(linkedListClass, LinkedList, toString, 0);

    vm->nodeClass = defineNativeClass(vm, "Node");
    bindSuperclass(vm, vm->nodeClass, vm->objectClass);
    DEF_METHOD(vm->nodeClass, Node, clone, 0);
    DEF_METHOD(vm->nodeClass, Node, element, 0);
    DEF_METHOD(vm->nodeClass, Node, init, 3);
    DEF_METHOD(vm->nodeClass, Node, next, 0);
    DEF_METHOD(vm->nodeClass, Node, prev, 0);
    DEF_METHOD(vm->nodeClass, Node, toString, 0);

    vm->dictionaryClass = getNativeClass(vm, "Dictionary");
    DEF_METHOD(vm->dictionaryClass, Dictionary, clear, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, clone, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, containsKey, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, containsValue, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, entrySet, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, equals, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, getAt, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, init, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, isEmpty, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, length, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, keySet, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, next, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, nextValue, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, putAll, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, putAt, 2);
    DEF_METHOD(vm->dictionaryClass, Dictionary, removeAt, 1);
    DEF_METHOD(vm->dictionaryClass, Dictionary, toString, 0);
    DEF_METHOD(vm->dictionaryClass, Dictionary, valueSet, 0);

    vm->entryClass = defineNativeClass(vm, "Entry");
    bindSuperclass(vm, vm->entryClass, vm->objectClass);
    DEF_METHOD(vm->entryClass, Entry, clone, 0);
    DEF_METHOD(vm->entryClass, Entry, getKey, 0);
    DEF_METHOD(vm->entryClass, Entry, getValue, 0);
    DEF_METHOD(vm->entryClass, Entry, init, 2);
    DEF_METHOD(vm->entryClass, Entry, setValue, 1);
    DEF_METHOD(vm->entryClass, Entry, toString, 0);

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
    DEF_METHOD(setClass, Set, toArray, 0);
    DEF_METHOD(setClass, Set, toString, 0);

    vm->rangeClass = defineNativeClass(vm, "Range");
    bindSuperclass(vm, vm->rangeClass, listClass);
    DEF_METHOD(vm->rangeClass, Range, add, 1);
    DEF_METHOD(vm->rangeClass, Range, addAll, 1);
    DEF_METHOD(vm->rangeClass, Range, clone, 0);
    DEF_METHOD(vm->rangeClass, Range, contains, 1);
    DEF_METHOD(vm->rangeClass, Range, from, 0);
    DEF_METHOD(vm->rangeClass, Range, getAt, 1);
    DEF_METHOD(vm->rangeClass, Range, init, 2);
    DEF_METHOD(vm->rangeClass, Range, length, 0);
    DEF_METHOD(vm->rangeClass, Range, max, 0);
    DEF_METHOD(vm->rangeClass, Range, min, 0);
    DEF_METHOD(vm->rangeClass, Range, next, 1);
    DEF_METHOD(vm->rangeClass, Range, nextValue, 1);
    DEF_METHOD(vm->rangeClass, Range, step, 2);
    DEF_METHOD(vm->rangeClass, Range, to, 0);
    DEF_METHOD(vm->rangeClass, Range, toArray, 0);
    DEF_METHOD(vm->rangeClass, Range, toString, 0);

    ObjClass* stackClass = defineNativeClass(vm, "Stack");
    bindSuperclass(vm, stackClass, collectionClass);
    DEF_METHOD(stackClass, Stack, clear, 0);
    DEF_METHOD(stackClass, Stack, contains, 1);
    DEF_METHOD(stackClass, Stack, getFirst, 0);
    DEF_METHOD(stackClass, Stack, init, 0);
    DEF_METHOD(stackClass, Stack, isEmpty, 0);
    DEF_METHOD(stackClass, Stack, length, 0);
    DEF_METHOD(stackClass, Stack, next, 1);
    DEF_METHOD(stackClass, Stack, nextValue, 1);
    DEF_METHOD(stackClass, Stack, peek, 0);
    DEF_METHOD(stackClass, Stack, pop, 0);
    DEF_METHOD(stackClass, Stack, push, 1);
    DEF_METHOD(stackClass, Stack, search, 1);
    DEF_METHOD(stackClass, Stack, toArray, 0);
    DEF_METHOD(stackClass, Stack, toString, 0);

    ObjClass* queueClass = defineNativeClass(vm, "Queue");
    bindSuperclass(vm, queueClass, collectionClass);
    DEF_METHOD(queueClass, Queue, clear, 0);
    DEF_METHOD(queueClass, Queue, contains, 1);
    DEF_METHOD(queueClass, Queue, dequeue, 0);
    DEF_METHOD(queueClass, Queue, enqueue, 1);
    DEF_METHOD(queueClass, Queue, getFirst, 0);
    DEF_METHOD(queueClass, Queue, getLast, 0);
    DEF_METHOD(queueClass, Queue, init, 0);
    DEF_METHOD(queueClass, Queue, isEmpty, 0);
    DEF_METHOD(queueClass, Queue, length, 0);
    DEF_METHOD(queueClass, Queue, next, 1);
    DEF_METHOD(queueClass, Queue, nextValue, 1);
    DEF_METHOD(queueClass, Queue, peek, 0);
    DEF_METHOD(queueClass, Queue, search, 1);
    DEF_METHOD(queueClass, Queue, toArray, 0);
    DEF_METHOD(queueClass, Queue, toString, 0);
}