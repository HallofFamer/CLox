#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol.h"
#include "../common/buffer.h"
#include "../vm/object.h"

#define SYMBOL_TABLE_MAX_LOAD 0.75

SymbolItem* newSymbolItem(Token token, SymbolCategory category, uint8_t index) {
    SymbolItem* item = (SymbolItem*)malloc(sizeof(SymbolItem));
    if (item != NULL) {
        item->token = token;
        item->category = category;
        item->index = index;
    }
    return item;
}

void freeSymbolItem(SymbolItem* item) {
    if (item != NULL) {
        free(item);
    }
}

SymbolTable* newSymbolTable(SymbolTable* parent, SymbolScope scope) {
    SymbolTable* symtab = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (symtab != NULL) {
        symtab->parent = parent;
        symtab->scope = scope;
        symtab->count = 0;
        symtab->capacity = 0;
        symtab->entries = NULL;
    }
    return symtab;
}

void freeSymbolTable(SymbolTable* symtab) {
    for (int i = 0; i < symtab->capacity; i++) {
        SymbolEntry* entry = &symtab->entries[i];
        if (entry != NULL) {
            freeSymbolItem(entry->value);
        }
    }
    if (symtab->entries != NULL) free(symtab->entries);
    free(symtab);
}

static SymbolEntry* findSymbolEntry(SymbolEntry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    for (;;) {
        SymbolEntry* entry = &entries[index];
        if (entry->key == key || entry->key == NULL) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

static void symbolTableAdjustCapacity(SymbolTable* symtab, int capacity) {
    int oldCapacity = symtab->capacity;
    SymbolEntry* entries = (SymbolEntry*)realloc(symtab->entries, sizeof(SymbolTable) * capacity);
    if (entries == NULL) exit(1);
    
    for (int i = oldCapacity; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NULL;
    }
    symtab->capacity = capacity;
    symtab->entries = entries;
}

SymbolItem* symbolTableGet(SymbolTable* symtab, ObjString* key) {
    if (symtab->count == 0) return NULL;
    SymbolEntry* entry = findSymbolEntry(symtab->entries, symtab->capacity, key);
    if (entry->key == NULL) return NULL;
    return entry->value;
}

bool symbolTableSet(SymbolTable* symtab, ObjString* key, SymbolItem* value) {
    if (symtab->count + 1 > symtab->capacity * SYMBOL_TABLE_MAX_LOAD) {
        int capacity = bufferGrowCapacity(symtab->capacity);
        symbolTableAdjustCapacity(symtab, capacity);
    }

    SymbolEntry* entry = findSymbolEntry(symtab->entries, symtab->capacity, key);
    if (entry->key != NULL) return false;
    symtab->count++;
    entry->key = key;
    entry->value = value;
    return true;
}

static void symbolTableOutputScope(SymbolScope scope) {
    switch (scope) {
        case SYMBOL_SCOPE_GLOBAL: 
            printf("global");
            break;
        case SYMBOL_SCOPE_MODULE: 
            printf("module");
            break;
        case SYMBOL_SCOPE_BEHAVIOR: 
            printf("behavior");
            break;
        case SYMBOL_SCOPE_FUNCTION: 
            printf("function");
            break;
        case SYMBOL_SCOPE_METHOD: 
            printf("method");
            break;
        case SYMBOL_SCOPE_BLOCK: 
            printf("block");
            break;
        default: 
            printf("none");
    }
}

static void symbolTableOutputEntry(SymbolEntry* entry) {
    printf("  symbol %s - category: %d, index: %d\n", entry->key->chars, entry->value->category, entry->value->index);
}

void symbolTableOutput(SymbolTable* symtab) {
    printf("Symbol table - scope: ");
    symbolTableOutputScope(symtab->scope);
    printf("\n");

    for (int i = 0; i < symtab->capacity; i++) {
        SymbolEntry* entry = &symtab->entries[i];
        if (entry != NULL && entry->key != NULL) {
            symbolTableOutputEntry(entry);
        }
    }
}
