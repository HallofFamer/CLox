#pragma once
#ifndef clox_table_h
#define clox_table_h

#include "string.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    int count;
    int capacity;
    GCGenerationType generation;
    Entry* entries;
} Table;

void initTable(Table* table, GCGenerationType generation);
void freeTable(VM* vm, Table* table);
bool tableGet(Table* table, ObjString* key, Value* value);
bool tableSet(VM* vm, Table* table, ObjString* key, Value value);
bool tableDelete(Table* table, ObjString* key);
void tableAddAll(VM* vm, Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);
void markTable(VM* vm, Table* table, GCGenerationType generation);

#endif // !clox_table_h