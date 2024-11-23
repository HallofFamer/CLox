#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbol.h"
#include "../common/buffer.h"
#include "../vm/object.h"

#define SYMBOL_TABLE_MAX_LOAD 0.75

SymbolItem* newSymbolItem(Token token, SymbolCategory category, SymbolState state, uint8_t index, bool isMutable) {
    SymbolItem* item = (SymbolItem*)malloc(sizeof(SymbolItem));
    if (item != NULL) {
        item->token = token;
        item->category = category;
        item->state = state;
        item->index = index;
        item->isMutable = isMutable;
    }
    return item;
}

void freeSymbolItem(SymbolItem* item) {
    if (item != NULL) {
        free(item);
    }
}

SymbolTable* newSymbolTable(int id, SymbolTable* parent, SymbolScope scope, uint8_t depth) {
    SymbolTable* symtab = (SymbolTable*)malloc(sizeof(SymbolTable));
    if (symtab != NULL) {
        symtab->id = id;
        symtab->parent = parent;
        symtab->scope = scope;
        symtab->depth = depth;
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

    if (symtab->entries != NULL) {
        free(symtab->entries);
    }
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

SymbolItem* symbolTableLookup(SymbolTable* symtab, ObjString* key) {
    SymbolTable* currentSymtab = symtab;
    SymbolItem* item = NULL;

    do {
        item = symbolTableGet(currentSymtab, key);
        if (item != NULL) break;
        currentSymtab = currentSymtab->parent;
    } while (currentSymtab != NULL);

    return item;
}

static void symbolTableOutputCategory(SymbolCategory category) {
    switch (category) {
        case SYMBOL_CATEGORY_LOCAL:
            printf("local");
            break;
        case SYMBOL_CATEGORY_UPVALUE:
            printf("upvalue");
            break;
        case SYMBOL_CATEGORY_GLOBAL:
            printf("global");
            break;
        default:
            printf("none");
    }
}

static void symbolTableOutputScope(SymbolScope scope) {
    switch (scope) {
        case SYMBOL_SCOPE_GLOBAL: 
            printf("global");
            break;
        case SYMBOL_SCOPE_MODULE: 
            printf("module");
            break;
        case SYMBOL_SCOPE_CLASS: 
            printf("class");
            break;
        case SYMBOL_SCOPE_TRAIT:
            printf("trait");
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

static void symbolTableOutputState(SymbolState state) {
    switch (state) {
        case SYMBOL_STATE_DECLARED:
            printf("declared");
            break;
        case SYMBOL_STATE_DEFINED:
            printf("defined");
            break;
        case SYMBOL_STATE_ACCESSED:
            printf("accessed");
            break;
        case SYMBOL_STATE_MODIFIED:
            printf("modified");
            break;
        default:
            printf("none");
    }
}

static void symbolTableOutputEntry(SymbolEntry* entry) {
    printf("  %s -> category: ", entry->key->chars);
    symbolTableOutputCategory(entry->value->category);
    printf(", index: %d, state: ", entry->value->index);
    symbolTableOutputState(entry->value->state);
    printf(", isMutable: %s\n", entry->value->isMutable ? "true" : "false");
}

void symbolTableOutput(SymbolTable* symtab) {
    printf("Symbol table -> id: %d, scope: ", symtab->id);
    symbolTableOutputScope(symtab->scope);
    printf(", depth: %d, count: %d\n", symtab->depth, symtab->count);

    for (int i = 0; i < symtab->capacity; i++) {
        SymbolEntry* entry = &symtab->entries[i];
        if (entry != NULL && entry->key != NULL) {
            symbolTableOutputEntry(entry);
        }
    }
    printf("\n");
}
