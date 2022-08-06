#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(VM* vm, Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int capacity, ObjString* key) {
    uint32_t index = key->hash & (capacity - 1);
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } 
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key) {
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tableContainsKey(Table* table, ObjString* key) {
    if (table->count == 0) return false;
    Entry* entry = findEntry(table->entries, table->capacity, key);
    return entry->key != NULL;
}

bool tableContainsValue(Table* table, Value value) {
    if (table->count == 0) return false;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        if (valuesEqual(entry->value, value)) return true;
    }
    return false;
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(VM* vm, Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(VM* vm, Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(vm, table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(VM* vm, Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(vm, to, entry->key, entry->value);
        }
    }
}

int tableLength(Table* table) {
    if (table->count == 0) return 0;
    int length = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL) length++;
    }
    return length;
}

int tableFindIndex(Table* table, ObjString* key) {
    int index = key->hash & (table->capacity - 1);
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return -1;
            }
            else {
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key) {
            return index;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) return NULL;
        } 
        else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
          return entry->key;
        }
        index = (index + 1) & (table->capacity - 1);
    }
}

void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

bool tablesEqual(Table* aTable, Table* bTable) {
    for (int i = 0; i < aTable->capacity; i++) {
        Entry* entry = &aTable->entries[i];
        if (entry->key == NULL) continue;
        Value bValue;
        bool keyExists = tableGet(bTable, entry->key, &bValue);
        if (!keyExists || entry->value != bValue) return false;
    }

    for (int i = 0; i < bTable->capacity; i++) {
        Entry* entry = &bTable->entries[i];
        if (entry->key == NULL) continue;
        Value aValue;
        bool keyExists = tableGet(aTable, entry->key, &aValue);
        if (!keyExists || entry->value != aValue) return false;
    }

    return true;
}

ObjString* tableToString(VM* vm, Table* table) {
    if (table->count == 0) return copyString(vm, "[]", 2);
    else {
        char string[UINT8_MAX] = "";
        string[0] = '[';
        size_t offset = 1;
        int startIndex = 0;

        for (int i = 0; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
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
            offset += valueLength;
            startIndex = i + 1;
            break;
        }

        for (int i = startIndex; i < table->capacity; i++) {
            Entry* entry = &table->entries[i];
            if (entry->key == NULL) continue;
            ObjString* key = entry->key;
            size_t keyLength = (size_t)key->length;
            Value value = entry->value;
            char* valueChars = valueToString(vm, value);
            size_t valueLength = strlen(valueChars);

            memcpy(string + offset, ", ", 2);
            offset += 2;
            memcpy(string + offset, key->chars, keyLength);
            offset += keyLength;
            memcpy(string + offset, ": ", 2);
            offset += 2;
            memcpy(string + offset, valueChars, valueLength);
            offset += valueLength;
        }

        string[offset] = ']';
        string[offset + 1] = '\0';
        return copyString(vm, string, (int)offset + 1);
    }
}

void markTable(VM* vm, Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject(vm, (Obj*)entry->key);
        markValue(vm, entry->value);
    }
}