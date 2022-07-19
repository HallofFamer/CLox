#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "util.h"
#include "../vm/assert.h"
#include "../vm/native.h"
#include "../vm/object.h"
#include "../vm/string.h"
#include "../vm/value.h"
#include "../vm/vm.h"

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

LOX_METHOD(List, add) {
	assertArgCount(vm, "List::add(element)", 1, argCount);
	writeValueArray(vm, &AS_LIST(receiver)->elements, args[0]);
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

LOX_METHOD(List, setAt) {
	assertArgCount(vm, "List::setAt(index, element)", 2, argCount);
	assertArgIsInt(vm, "List::setAt(index, element)", args, 0);
	ObjList* self = AS_LIST(receiver);
	int index = AS_INT(args[0]);
	assertIndexWithinRange(vm, "List::insertAt(index)", index, 0, self->elements.count, 0);
	self->elements.values[index] = args[1];
	if (index == self->elements.count) self->elements.count++;
	RETURN_OBJ(receiver);
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
	DEF_METHOD(vm->listClass, List, remove, 1);
	DEF_METHOD(vm->listClass, List, removeAt, 1);
	DEF_METHOD(vm->listClass, List, setAt, 2);
	DEF_METHOD(vm->listClass, List, subList, 2);
	DEF_METHOD(vm->listClass, List, toString, 0);
}