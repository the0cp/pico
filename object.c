#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "mem.h"
#include "object.h"
#include "value.h"
#include "hashtable.h"

#include "xxhash.h"

static uint64_t g_hash_seed = 0;

// FNV-1a Hash
/*
static uint64_t hashString(const char* key, int len){
    uint64_t hash = 2166136261u;
    for(int i = 0; i < len; i++){
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}
*/

static void initHashSeed(){
    srand((unsigned int)time(NULL));
    uint64_t p1 = (uint64_t)rand();
    uint64_t p2 = (uint64_t)rand();
    g_hash_seed = (p1 << 32) | p2;

    if(g_hash_seed == 0){
        g_hash_seed = 1;
    }
}

static uint64_t hashString(const char* key, int len){
    if(g_hash_seed == 0){
        initHashSeed();
    }
    return XXH3_64bits_withSeed(key, (size_t)len, g_hash_seed);
}

static ObjectString* allocString(int len, uint64_t hash){
    ObjectString* str = (ObjectString*)resize(NULL, 0, sizeof(ObjectString) + len + 1);
    str -> obj.type = OBJECT_STRING;
    str -> length = len;
    str -> hash = hash;
    tableSet(&vm.strings, str, NULL_VAL);
    return str;
}

ObjectString* copyString(const char* chars, int len){
    uint64_t hash = hashString(chars, len);
    /*
    ObjectString* str = allocString(len, hash);
    memcpy(str -> chars, chars, len);
    str -> chars[len] = '\0';
    return str;
    */

    //ObjectString* interned = tableGetInternedString()
}

void printObject(Value value){
    switch(OBJECT_TYPE(value)){
        case OBJECT_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}