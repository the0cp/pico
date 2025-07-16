#ifndef PICO_VALUE_H
#define PICO_VALUE_H

#include "common.h"

typedef double Value;

typedef struct{
    Value* values;
    size_t count;
    size_t capacity;
}ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);


#endif  // PICO_VALUE_H