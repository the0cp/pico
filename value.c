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
        snprintf(num_str, sizeof(num_str), "%g", AS_NUM(value));
        return num_str;
    }else if(IS_STRING(value)){
        return AS_CSTRING(value);
    }
    return "Unknown";
}

ObjectString* toString(VM* vm, Value value){
    if(IS_STRING(value)){
        return AS_STRING(value);
    }
    char* str = valueToString(value);
    return copyString(vm, str, (int)strlen(str));
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
        printf("%g", AS_NUM(value));
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
            return false;
        }
        default:            return false;  // Unsupported type comparison
    }
}
