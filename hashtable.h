#ifndef PICO_HASHTABLE_H
#define PICO_HASHTABLE_H

#include "common.h"
#include "value.h"

typedef struct ObjectString ObjectString;

typedef struct{
    Value key;
    Value value;
}Entry;

typedef struct{
    int count;
    int capacity;
    Entry* entries;
}HashTable;

void initHashTable(HashTable* table);
void freeHashTable(VM* vm, HashTable* table);

bool tableGet(HashTable* table, Value key, Value* value);
bool tableSet(VM* vm, HashTable* table, Value key, Value value);
bool tableRemove(VM* vm, HashTable* table, Value key);
bool tableMerge(VM* vm, HashTable* from, HashTable* to);

ObjectString* tableGetInternedString(HashTable* table, const char* chars, int len, uint64_t hash);
void tableRemoveWhite(VM* vm, HashTable* table);
void markTable(VM* vm, HashTable* table);

static uint64_t hashValue(Value value);

#endif