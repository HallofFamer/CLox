#pragma once
#ifndef clox_symbol_h
#define clox_symbol_h

#include "token.h"
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
    SYMBOL_CATEGORY_LOCAL,
    SYMBOL_CATEGORY_UPVALUE,
    SYMBOL_CATEGORY_GLOBAL,
    SYMBOL_CATEGORY_FUNCTION,
    SYMBOL_CATEGORY_METHOD
} SymbolCategory;

typedef struct {
    Token token;
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

SymbolItem* newSymbolItem(Token token, SymbolCategory category, uint8_t index);
void freeSymbolItem(SymbolItem* item);
SymbolTable* newSymbolTable(SymbolTable* parent, SymbolScope scope);
void freeSymbolTable(SymbolTable* symTab);
SymbolItem* symbolTableGet(SymbolTable* symtab, ObjString* key);
bool symbolTableSet(SymbolTable* symtab, ObjString* key, SymbolItem* value);
SymbolItem* symbolTableLookup(SymbolTable* symtab, ObjString* key);
void symbolTableOutput(SymbolTable* symtab);

#endif // !clox_symbol_h