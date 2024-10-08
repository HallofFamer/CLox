#pragma once
#ifndef clox_symbol_h
#define clox_symbol_h

#include "../vm/value.h"

typedef struct SymbolTable SymbolTable;

typedef enum {
    SYMBOL_SCOPE_GLOBAL,
    SYMBOL_SCOPE_MODULE,
    SYMBOL_SCOPE_BEHAVIOR,
    SYMBOL_SCOPE_FUNCTION,
    SYMBOL_SCOPE_METHOD,
    SYMBOL_SCOPE_BLOCK
} SymbolScope;

typedef enum {
    SYMBOL_ITEM_LOCAL,
    SYMBOL_ITEM_UPVALUE,
    SYMBOL_ITEM_GLOBAL,
    SYMBOL_ITEM_FUNCTION,
    SYMBOL_ITEM_METHOD
} SymbolCategory;

typedef struct {
    ObjString* symbol;
    SymbolCategory category;
    uint8_t index;
    //TypeInfo type;
} SymbolItem;

typedef struct {
    ObjString* key;
    SymbolItem* value;
} SymbolEntry;

struct SymbolTable {
    struct SymbolTable* parent;
    SymbolScope scope;
    int count;
    int capacity;
    SymbolEntry* entries;
};

SymbolItem* newSymbolItem(ObjString* symbol, SymbolCategory category);
void freeSymbolItem(SymbolItem* item);
SymbolTable* newSymbolTable(SymbolTable* parent, SymbolScope scope);
void freeSymbolTable(SymbolTable* symTab);
SymbolItem* symbolTableGet(SymbolTable* symtab, ObjString* key);
bool symbolTableSet(SymbolTable* symtab, ObjString* key, SymbolItem* value);

#endif // !clox_symbol_h