#include <stdio.h>

#include "mem.h"
#include "value.h"
#include "object.h"

char* valueToString(Value value){
    if(IS_BOOL(value)){
        return AS_BOOL(value) ? "true" : "false";
    }else if(IS_NULL(value)){
        return "null";
    }else if(IS_NUM(value)){
        static char num_str[32];
        snprintf(num_str, sizeof(num_str), "%.14g", AS_NUM(value));
        return num_str;
    }else if(IS_STRING(value)){
        return AS_CSTRING(value);
    }
    return "Unknown";
}

static void appendString(VM* vm, char** buffer, int* length, int* capacity, const char* str, int len){
    if(*length + len > *capacity){
        int oldCapacity = *capacity;
        *capacity = (*capacity < 8 ? 8 : *capacity * 2) + len;
        *buffer = (char*)reallocate(vm, *buffer, oldCapacity, *capacity);
    }
    memcpy(*buffer + *length, str, len);
    *length += len;
    (*buffer)[*length] = '\0';
}

ObjectString* toString(VM* vm, Value value){
    if(IS_STRING(value)){
        return AS_STRING(value);
    }

    if(IS_LIST(value)){
        ObjectList* list = AS_LIST(value);
        char* buffer = NULL;
        int length = 0;
        int capacity = 0;

        appendString(vm, &buffer, &length, &capacity, "[", 1);

        for(int i = 0; i < list->count; i++){
            Value item = list->items[i];
            
            if(IS_STRING(item)){
                ObjectString* s = AS_STRING(item);
                appendString(vm, &buffer, &length, &capacity, "\"", 1);
                appendString(vm, &buffer, &length, &capacity, s->chars, s->length);
                appendString(vm, &buffer, &length, &capacity, "\"", 1);
            }else if(IS_OBJECT(item)){
                ObjectString* s = toString(vm, item); 
                appendString(vm, &buffer, &length, &capacity, s->chars, s->length);
            }else{
                char* s = valueToString(item);
                appendString(vm, &buffer, &length, &capacity, s, (int)strlen(s));
            }

            if(i < list->count - 1){
                appendString(vm, &buffer, &length, &capacity, ", ", 2);
            }
        }

        appendString(vm, &buffer, &length, &capacity, "]", 1);

        ObjectString* result = copyString(vm, buffer, length);
        reallocate(vm, buffer, capacity, 0);
        return result;
    }
    char* chars = valueToString(value);
    return copyString(vm, chars, (int)strlen(chars));
}

void initValueArray(ValueArray* array){
    array->values = NULL;
    array->count = 0;
    array->capacity = 0;
}

void writeValueArray(VM* vm, ValueArray* array, Value value){
    if(array->count + 1 > array->capacity){
        size_t oldCapacity = array->capacity;
        array->capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
        array->values = (Value*)reallocate(
            vm,
            array->values,
            sizeof(Value) * oldCapacity,
            sizeof(Value) * array->capacity
        );
    }
    array->values[array->count++] = value;
}

void freeValueArray(VM* vm, ValueArray* array){
    reallocate(vm, array->values, sizeof(Value) * array->capacity, 0);
    initValueArray(array);
}

void printValue(Value value){
    if(IS_NULL(value)){
        printf("null");
    }else if(IS_BOOL(value)){
        printf(AS_BOOL(value) ? "true" : "false");
    }else if(IS_NUM(value)){
        printf("%.14g", AS_NUM(value));
    }else if(IS_OBJECT(value)){
        printObject(value);
    }else{
        printf("Unknown value type");
    }
}

ValueType getValueType(Value value){
    if(IS_NULL(value))   return VALUE_NULL;
    if(IS_BOOL(value))   return VALUE_BOOL;
    if(IS_NUM(value))    return VALUE_NUM;
    if(IS_OBJECT(value)) return VALUE_OBJECT;
    return VALUE_UNKNOWN;  // For unsupported types
}

bool isEqual(Value a, Value b){
    if(getValueType(a) != getValueType(b)) return false;
    switch(getValueType(a)){
        case VALUE_NULL:    return true;  // Both are null
        case VALUE_BOOL:    return AS_BOOL(a) == AS_BOOL(b);
        case VALUE_NUM:     return AS_NUM(a) == AS_NUM(b);
        case VALUE_OBJECT: {
            if(IS_STRING(a) && IS_STRING(b)){
                ObjectString* strA = AS_STRING(a);
                ObjectString* strB = AS_STRING(b);
                return strA->length == strB->length &&
                        memcmp(strA->chars, strB->chars, strA->length) == 0;
            }

            if(IS_LIST(a) && IS_LIST(b)){
                ObjectList* listA = AS_LIST(a);
                ObjectList* listB = AS_LIST(b);

                if(listA->count != listB->count) return false;

                for(int i = 0; i < listA->count; i++){
                    if(!isEqual(listA->items[i], listB->items[i])){
                        return false;
                    }
                }
                return true;
            }
            return AS_OBJECT(a) == AS_OBJECT(b);
        }
        default:            return false;  // Unsupported type comparison
    }
}
