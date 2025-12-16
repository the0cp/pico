#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "mem.h"
#include "object.h"
#include "hashtable.h"
#include "value.h"
#include "xxhash.h"

#define TABLE_MAX_LOAD 0.75

void initHashTable(HashTable* table){
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

static uint64_t hashValue(Value value, uint64_t seed){
    if(IS_STRING(value)){
        return AS_STRING(value)->hash;
    }
    if(IS_NUM(value)){
        double num = AS_NUM(value);
        if(num == 0.0)    num = 0.0;
        //  0.0: 0x0000000000000000
        // -0.0: 0x8000000000000000
        return XXH3_64bits_withSeed(&num, sizeof(double), seed);
    }
    if(IS_OBJECT(value))    return (uint64_t)AS_OBJECT(value);
    if(IS_BOOL(value))  return AS_BOOL(value) ? 3 : 5;
    if(IS_NULL(value))  return 7;
    return (uint64_t)value;
}


void freeHashTable(VM* vm, HashTable* table){
    // free array
    reallocate(vm, table->entries, sizeof(Entry) * table->capacity, 0);
    initHashTable(table);
}

static uint32_t get_distance(int capacity, uint32_t ideal_index, uint32_t cur_index){
    if(cur_index >= ideal_index){
        return cur_index - ideal_index;
    }else{
        // wrap-around
        return capacity - ideal_index + cur_index;
    }
}

static Entry* findEntry(Entry* entries, int capacity, Value key, uint64_t seed){
    if(capacity == 0){
        return NULL;    
    }

    uint64_t hash = hashValue(key, seed);
    uint32_t ideal_index = hash & (capacity - 1);
    uint32_t probe_dist = 0;

    while(true){
        uint32_t cur_index = (ideal_index + probe_dist) & (capacity - 1);
        Entry* bucket = &entries[cur_index];

        if(IS_EMPTY(bucket->key)){
            return NULL;    
        }

        uint64_t bucket_hash = hashValue(bucket->key, seed);
        uint32_t bucket_ideal_index = bucket_hash & (capacity - 1);
        uint32_t bucket_dist = get_distance(capacity, bucket_ideal_index, cur_index);
        if(probe_dist > bucket_dist){
            return NULL;
        }

        if(isEqual(bucket->key, key)){
            return bucket;
        }
        probe_dist++;
    }
}

bool tableGet(VM* vm, HashTable* table, Value key, Value* value){
    if(table->count == 0){
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key, vm->hash_seed);
    if(entry == NULL || IS_EMPTY(entry->key)){
        return false;
    }

    *value = entry->value;
    return true;
}

static void adjustCapacity(VM* vm, HashTable* table, int capacity);

bool tableSet(VM* vm, HashTable* table, Value key, Value value){
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD){
        int new_capacity = (table->capacity) < 8 ? 8 : 2 * (table->capacity);
        adjustCapacity(vm, table, new_capacity);
    }

    Entry* existing = findEntry(table->entries, table->capacity, key, vm->hash_seed);
    if(existing != NULL){
        existing->value = value;
        return false;
    }

    Entry entryToInsert;
    entryToInsert.key = key;
    entryToInsert.value = value;

    uint64_t key_hash = hashValue(key, vm->hash_seed);
    uint32_t ideal_index = key_hash & (table->capacity - 1);
    uint32_t probe_dist = 0;

    while(true){
        uint32_t cur_index = (ideal_index + probe_dist) & (table->capacity -1);
        Entry* bucket = &table->entries[cur_index];

        if(IS_EMPTY(bucket->key)){
            *bucket = entryToInsert;
            table->count++;
            return true;
        }

        if(isEqual(bucket->key, key)){
            bucket->value = value;  // update
            return false;
        }

        // robbing
        uint64_t bucket_hash = hashValue(bucket->key, vm->hash_seed);
        uint32_t bucket_ideal_index = bucket_hash & (table->capacity - 1);
        uint32_t bucket_dist = get_distance(table->capacity, bucket_ideal_index, cur_index);

        if(probe_dist > bucket_dist){
            Entry tmp = *bucket;
            *bucket = entryToInsert;
            entryToInsert = tmp;
            
            key_hash = hashValue(entryToInsert.key, vm->hash_seed);
            ideal_index = key_hash & (table->capacity - 1);
            probe_dist = get_distance(table->capacity, ideal_index, cur_index);
        }
        probe_dist++;
    }
}

bool tableRemove(VM* vm, HashTable* table, Value key){
    if(table->count == 0){
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key, vm->hash_seed);
    if(entry == NULL || IS_EMPTY(entry->key)){
        return false;
    }

    table->count--;
    uint32_t bucket_index = (uint32_t)(entry - table->entries);
    uint32_t capacity = table->capacity;

    // move entries forward
    while(true){
        uint32_t next_bucket_index = (bucket_index + 1) & (capacity - 1);
        Entry* next_bucket = &table->entries[next_bucket_index];

        if(IS_EMPTY(next_bucket->key)){
            break;
        }

        uint64_t next_hash = hashValue(next_bucket->key, vm->hash_seed);
        uint32_t next_ideal = next_hash & (capacity - 1);
        if(get_distance(capacity, next_ideal, next_bucket_index) == 0){
            break;  // next entry already in its ideal bucket
        }

        table->entries[bucket_index] = *next_bucket;
        bucket_index = next_bucket_index;
    }

    table->entries[bucket_index].key = EMPTY_VAL;
    table->entries[bucket_index].value = NULL_VAL;

    return true;
}

static void adjustCapacity(VM* vm, HashTable* table, int capacity){
    Entry* entries = (Entry*)reallocate(vm, NULL, 0, sizeof(Entry) * capacity);
    for(int i = 0; i < capacity; i++){
        entries[i].key = EMPTY_VAL;
        entries[i].value = NULL_VAL;
    }

    table->count = 0;
    for(int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        if(IS_EMPTY(entry->key)){
            continue;
        }
        tableSet(vm, 
                &(HashTable){
                    .count = 0,
                    .capacity = capacity,
                    .entries = entries
                },
                entry->key,
                entry->value);
    }

    reallocate(vm, table->entries, sizeof(Entry) * table->capacity, 0);
    table->entries = entries;
    table->capacity = capacity;

    table->count = 0;
    for(int i = 0; i < capacity; i++){
        if(!IS_EMPTY(table->entries[i].key)){
            table->count++;
        }
    }
}

bool tableMerge(VM* vm, HashTable* from, HashTable* to){
    for(int i = 0; i < from->capacity; i++){
        Entry* entry = &from->entries[i];
        if(!IS_EMPTY(entry->key)){
            tableSet(vm, to, entry->key, entry->value);
        }
    }
    return true;
}

ObjectString* tableGetInternedString(VM* vm, HashTable* table, const char* chars, int len, uint64_t hash){
    if(table->count == 0){
        return NULL;
    }

    uint32_t capacity = table->capacity;
    uint32_t ideal_index = hash & (capacity - 1);
    uint32_t probe_dist = 0;

    while(true){
        uint32_t cur_index = (ideal_index + probe_dist) & (capacity - 1);
        Entry* entry = &table->entries[cur_index];

        if(IS_EMPTY(entry->key)){
            if(IS_NULL(entry->value)){
                return NULL;
            }
            probe_dist++;
            continue;
        }

        uint64_t entry_hash = hashValue(entry->key, vm->hash_seed);
        uint32_t entry_ideal_index = entry_hash & (capacity - 1);
        uint32_t entry_dist = get_distance(capacity, entry_ideal_index, cur_index);
        
        if(probe_dist > entry_dist){
            return NULL;
        }

        if(IS_STRING(entry->key)){
            ObjectString* keyStr = AS_STRING(entry->key);
            if(keyStr->length == len && 
                keyStr->hash == hash &&
                memcmp(keyStr->chars, chars, len) == 0){
                return keyStr;
            }
        }

        probe_dist++;
    }
}

void markTable(VM* vm, HashTable* table){
    if(table == NULL || table->count == 0) return;
    
    for(int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        if(!IS_EMPTY(entry->key)) {
            markValue(vm, entry->key);
            markValue(vm, entry->value);
        }
    }
}

void tableRemoveWhite(VM* vm, HashTable* table) {
    if(table->count == 0) return;

    HashTable newTable;
    initHashTable(&newTable);

    for(int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];

        if(IS_EMPTY(entry->key))    continue;

        if(IS_OBJECT(entry->key) && AS_OBJECT(entry->key)->isMarked){
            push(vm, entry->key); 
            push(vm, entry->value);           

            tableSet(vm, 
                     &newTable, 
                     peek(vm, 1), 
                     peek(vm, 0));
            
            pop(vm);
            pop(vm);
        }
    }

    freeHashTable(vm, table);

    *table = newTable;
}
