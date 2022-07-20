#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

static ObjString* dictionaryToString(VM* vm, ObjDictionary* dictionary) {
	if(dictionary->table.count == 0) return copyString(vm, "[]", 2);
	else {
		char string[UINT8_MAX] = "";
		string[0] = '[';
		size_t offset = 1;
		for (int i = 0; i < dictionary->table.capacity; i++) {
			Entry* entry = &dictionary->table.entries[i];
			if (entry->key == NULL) continue;

			ObjString* key = entry->key;
			size_t keyLength = (size_t)key->length;
			Value value = entry->value;
			char* valueChars = valueToString(vm, value);
			size_t valueLength = strlen(valueChars);

			memcpy(string + offset, key->chars, keyLength);
			offset += keyLength;
			memcpy(string + offset, ": ", 2);
			offset += 2;
			memcpy(string + offset, valueChars, valueLength);

			memcpy(string + offset + valueLength, "; ", 2);
		    offset += valueLength + 2;
		}

		string[offset] = ']';
		string[offset + 1] = '\0';
		return copyString(vm, string, (int)offset + 1);
	}
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
	RETURN_OBJ(dictionaryToString(vm, AS_DICTIONARY(receiver)));
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
	assertIndexWithinRange(vm, "List::getAt(index)", index, 0, self->elements.count - 1, 0);
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
	assertIndexWithinRange(vm, "List::insertAt(index)", index, 0, self->elements.count, 0);
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
	assertIndexWithinRange(vm, "List::putAt(index)", index, 0, self->elements.count, 0);
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
	assertIndexWithinRange(vm, "List::removeAt(index)", AS_INT(args[0]), 0, self->elements.count - 1, 0);
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

	assertIndexWithinRange(vm, "List::subList(from, to)", fromIndex, 0, self->elements.count, 0);
	assertIndexWithinRange(vm, "List::subList(from, to", toIndex, fromIndex, self->elements.count, 1);
	RETURN_OBJ(copyList(vm, self->elements, fromIndex, toIndex));
}

LOX_METHOD(List, toString) {
	assertArgCount(vm, "List::toString()", 0, argCount);
	RETURN_OBJ(listToString(vm, AS_LIST(receiver)));
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
}