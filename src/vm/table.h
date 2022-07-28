#pragma once
#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(VM* vm, Table* table);
bool tableContainsKey(Table* table, ObjString* key);
bool tableContainsValue(Table* table, Value value);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(VM* vm, Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(VM* vm, Table* from, Table* to);
int tableLength(Table* table);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);
bool tablesEqual(Table* aTable, Table* bTable);
ObjString* tableToString(VM* vm, Table* table);
void markTable(VM* vm, Table* table);

#endif // !clox_table_h