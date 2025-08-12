#ifndef PICO_HASHTABLE_H
#define PICO_HASHTABLE_H

#include "common.h"
#include "value.h"

typedef struct ObjectString ObjectString;

typedef struct{
    ObjectString* key;
    Value value;
}Entry;

typedef struct{
    int count;
    int capacity;
    Entry* entries;
}HashTable;

void initHashTable(HashTable* table);
void freeHashTable(HashTable* table);

bool tableGet(HashTable* table, ObjectString* key, Value* value);
bool tableSet(HashTable* table, ObjectString* key, Value value);
bool tableRemove(HashTable* table, ObjectString *key);
bool tableMerge(HashTable* from, HashTable* to);

ObjectString* tableGetInternedString(HashTable* table, const char* chars, int len, uint64_t hash);
// void tableRemoveWhite(HashTable* table);
// void markTable(HashTable* table);

#endif