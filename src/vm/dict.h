#pragma once
#ifndef clox_dict_h
#define clox_dict_h

#include "object.h"
#include "value.h"

ObjEntry* dictFindEntry(ObjEntry* entries, int capacity, Value key);
void dictAdjustCapacity(VM* vm, ObjDictionary* dict, int capacity);
bool dictGet(ObjDictionary* dict, Value key, Value* value);
bool dictSet(VM* vm, ObjDictionary* dict, Value key, Value value);
void dictAddAll(VM* vm, ObjDictionary* from, ObjDictionary* to);
bool dictDelete(ObjDictionary* dict, Value key);

#endif // !clox_dict_h
