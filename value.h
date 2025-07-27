#ifndef PICO_VALUE_H
#define PICO_VALUE_H

#include <string.h>

#include "common.h"

typedef uint64_t Value;

#define VALUE_NULL      1
#define VALUE_FALSE     2
#define VALUE_TRUE      3
#define QNAN            ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT        ((uint64_t)0x8000000000000000)

#define IS_NULL(value)  ((value) == (QNAN | VALUE_NULL))
#define IS_BOOL(value)  (((value) & ~1) == (QNAN | VALUE_FALSE))
#define IS_NUM(value)   (((value) & QNAN) != QNAN)

#define AS_NUM(value)   valueToNum(value)
#define AS_BOOL(value)  ((value) == (QNAN | VALUE_TRUE))

#define NUM_VAL(num)    numToValue(num)
#define NULL_VAL()      ((Value)(uint64_t)(QNAN | VALUE_NULL))
#define BOOL_VAL(bool)  ((Value)(uint64_t)(QNAN | (bool ? VALUE_TRUE : VALUE_FALSE)))

static inline Value numToValue(double num) {
    Value bits;
    memcpy(&bits, &num, sizeof(double));
    return bits;
}

static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(double));
    return num;
}

typedef struct{
    Value* values;
    size_t count;
    size_t capacity;
}ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);
void printValue(Value value);

#endif  // PICO_VALUE_H