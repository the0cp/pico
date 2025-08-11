#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "object.h"
#include "value.h"

// FNV-1a Hash
static uint32_t hashString(const char* key, int len){
    uint32_t hash = 2166136261u;
    for(int i = 0; i < len; i++){
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

static ObjectString* allocString(int len, uint32_t hash){
    ObjectString* str = (ObjectString*)resize(NULL, 0, sizeof(ObjectString) + len + 1);
    str -> obj.type = OBJECT_STRING;
    str -> length = len;
    str -> hash = hash;
    return str;
}

ObjectString* copyString(const char* chars, int len){
    uint32_t hash = hashString(chars, len);
    ObjectString* str = allocString(len, hash);
    memcpy(str -> chars, chars, len);
    str -> chars[len] = '\0';
    return str;
}

void printObject(Value value){
    switch(OBJECT_TYPE(value)){
        case OBJECT_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}