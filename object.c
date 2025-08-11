#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "object.h"
#include "value.h"

static uint32_t hashString(const char* key, int len){
    uint32_t hash = 2166136261u;
    for(int i = 0; i < len; i++){
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

static ObjectString* allocString(char* chars, int len, uint32_t hash){
    ObjectString* str = (ObjectString*)resize(NULL, 0, sizeof(ObjectString));
    str -> obj.type = OBJECT_STRING;
    str -> length = len;
    str -> chars = chars;
    str -> hash = hash;
    return str;
}

ObjectString* copyString(const char* chars, int len){
    uint32_t hash = hashString(chars, len);
    char* heapChars = (char*)resize(NULL, 0, len + 1);
    memcpy(heapChars, chars, len);
    heapChars[len] = '\0';
    return allocString(heapChars, len, hash);
}

void printObject(Value value){
    switch(OBJECT_TYPE(value)){
        case OBJECT_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}