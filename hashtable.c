#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mem.h"
#include "object.h"
#include "hashtable.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initHashTable(HashTable* table){
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
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

static Entry* findEntry(Entry* entries, int capacity, ObjectString* key){
    if(capacity == 0){
        return NULL;    
    }

    uint32_t ideal_index = key->hash & (capacity - 1);
    uint32_t probe_dist = 0;

    while(true){
        uint32_t cur_index = (ideal_index + probe_dist) & (capacity - 1);
        Entry* bucket = &entries[cur_index];

        if(bucket->key == NULL){
            return NULL;    
        }

        uint32_t bucket_ideal_index = bucket->key->hash & (capacity - 1);
        uint32_t bucket_dist = get_distance(capacity, bucket_ideal_index, cur_index);
        if(probe_dist > bucket_dist){
            return NULL;
        }

        if(bucket->key == key){
            return bucket;
        }
        probe_dist++;
    }
}

bool tableGet(HashTable* table, ObjectString* key, Value* value){
    if(table->count == 0){
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry == NULL || entry->key == NULL){
        return false;
    }

    *value = entry->value;
    return true;
}

static void adjustCapacity(VM* vm, HashTable* table, int capacity);

bool tableSet(VM* vm, HashTable* table, ObjectString* key, Value value){
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD){
        int new_capacity = (table->capacity) < 8 ? 8 : 2 * (table->capacity);
        adjustCapacity(vm, table, new_capacity);
    }

    Entry entryToInsert;
    entryToInsert.key = key;
    entryToInsert.value = value;

    uint32_t ideal_index = entryToInsert.key->hash & (table->capacity - 1);
    uint32_t probe_dist = 0;

    while(true){
        uint32_t cur_index = (ideal_index + probe_dist) & (table->capacity -1);
        Entry* bucket = &table->entries[cur_index];

        if(bucket->key == NULL){
            *bucket = entryToInsert;
            table->count++;
            return true;
        }

        if(bucket->key == key){
            bucket->value = value;  // update
            return false;
        }

        // robbing
        uint32_t bucket_ideal_index = bucket->key->hash & (table->capacity - 1);
        uint32_t bucket_dist = get_distance(table->capacity, bucket_ideal_index, cur_index);

        if(probe_dist > bucket_dist){
            Entry tmp = *bucket;
            *bucket = entryToInsert;
            entryToInsert = tmp;
            
            ideal_index = entryToInsert.key->hash & (table->capacity - 1);
            probe_dist = get_distance(table->capacity, ideal_index, cur_index);
        }
        probe_dist++;
    }
}

bool tableRemove(VM* vm, HashTable* table, ObjectString* key){
    if(table->count == 0){
        return false;
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry == NULL || entry->key == NULL){
        return false;
    }

    table->count--;
    uint32_t bucket_index = (uint32_t)(entry - table->entries);

    // move entries forward
    while(true){
        uint32_t next_bucket_index = (bucket_index + 1) & (table->capacity - 1);
        Entry* next_bucket = &table->entries[next_bucket_index];

        if(next_bucket->key == NULL){
            break;
        }

        uint32_t next_ideal = next_bucket->key->hash & (table->capacity - 1);
        if(get_distance(table->capacity, next_ideal, next_bucket_index) == 0){
            break;  // next entry already in its ideal bucket
        }

        table->entries[bucket_index] = *next_bucket;
        bucket_index = next_bucket_index;
    }

    table->entries[bucket_index].key = NULL;
    table->entries[bucket_index].value = NULL_VAL;

    return true;
}

static void adjustCapacity(VM* vm, HashTable* table, int capacity){
    Entry* entries = (Entry*)reallocate(vm, NULL, 0, sizeof(Entry) * capacity);
    for(int i = 0; i < capacity; i++){
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    table->count = 0;
    for(int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        if(entry->key == NULL){
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

    for(int i = 0; i < capacity; i++){
        if(table->entries[i].key != NULL){
            table->count++;
        }
    }
}

bool tableMerge(VM* vm, HashTable* from, HashTable* to){
    for(int i = 0; i < from->capacity; i++){
        Entry* entry = &from->entries[i];
        if(entry->key != NULL){
            tableSet(vm, to, entry->key, entry->value);
        }
    }
    return true;
}

ObjectString* tableGetInternedString(HashTable* table, const char* chars, int len, uint64_t hash){
    if(table->count == 0){
        return NULL;
    }

    uint32_t ideal_index = hash & (table->capacity - 1);
    uint32_t probe_dist = 0;

    while(true){
        uint32_t cur_index = (ideal_index + probe_dist) & (table->capacity - 1);
        Entry* entry = &table->entries[cur_index];

        if(entry->key == NULL){
            if(IS_NULL(entry->value)){
                return NULL;
            }
            probe_dist++;
            continue;
        }

        uint32_t entry_ideal_index = entry->key->hash & (table->capacity - 1);
        uint32_t entry_dist = get_distance(table->capacity, entry_ideal_index, cur_index);
        if(probe_dist > entry_dist){
            return NULL;
        }

        if(entry->key->length == len && 
           entry->key->hash == hash && 
           memcmp(entry->key->chars, chars, len) == 0){
            return entry->key;
        }
        probe_dist++;
    }
}

void markTable(VM* vm, HashTable* table){
    if(table == NULL || table->count == 0) return;
    
    for(int i = 0; i < table->capacity; i++){
        Entry* entry = &table->entries[i];
        if(entry->key != NULL) {
            markObject(vm, (Object*)entry->key);
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
        if(entry->key != NULL && entry->key->obj.isMarked){
            
            push(vm, OBJECT_VAL(entry->key)); 
            push(vm, entry->value);           

            tableSet(vm, 
                     &newTable, 
                     AS_STRING(peek(vm, 1)), 
                     peek(vm, 0));
            
            pop(vm);
            pop(vm);
        }
    }

    freeHashTable(vm, table);

    *table = newTable;
}
