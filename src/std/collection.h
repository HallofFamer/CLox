#pragma once
#ifndef clox_std_collection_h
#define clox_std_collection_h

#include "../vm/common.h"

bool dictGet(ObjDictionary* dict, Value key, Value* value);
bool dictSet(VM* vm, ObjDictionary* dict, Value key, Value value);
void registerCollectionPackage(VM* vm);

#endif // clox_std_collection_h