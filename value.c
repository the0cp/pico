#include <stdio.h>

#include "mem.h"
#include "value.h"

void initValueArray(ValueArray* array){
    array -> values = NULL;
    array -> count = 0;
    array -> capacity = 0;
}

void writeValueArray(ValueArray* array, Value value){
    if(array -> count + 1 > array -> capacity){
        size_t oldCapacity = array -> capacity;
        array -> capacity = oldCapacity < 8 ? 8 : oldCapacity * 2;
        array -> values = (Value*)resize(
            array -> values,
            sizeof(Value) * oldCapacity,
            sizeof(Value) * array -> capacity
        );
    }
    array -> values[array -> count++] = value;
}

void freeValueArray(ValueArray* array){
    resize(array -> values, sizeof(Value) * array -> capacity, 0);
    initValueArray(array);
}

void printValue(Value value){
    if(IS_NULL(value)){
        printf("null");
    }else if(IS_BOOL(value)){
        printf(AS_BOOL(value) ? "true" : "false");
    }else if(IS_NUM(value)){
        printf("%g", AS_NUM(value));
    }else{
        printf("Unknown value type");
    }
}
