#ifndef PICO_VALUE_H
#define PICO_VALUE_H

#include <string.h>

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef uint64_t Value;
typedef struct VM VM;

typedef enum{
    VALUE_NULL,
    VALUE_BOOL,
    VALUE_NUM,
    VALUE_OBJECT,
    VALUE_UNKNOWN,
} ValueType;

#define TAG_NULL        1
#define TAG_FALSE       2
#define TAG_TRUE        3
#define QNAN            ((uint64_t)0x7ffc000000000000)
#define SIGN_BIT        ((uint64_t)0x8000000000000000)

#define IS_NULL(value)  ((value) == (QNAN | TAG_NULL))
#define IS_BOOL(value)  (((value) & ~1) == (QNAN | TAG_FALSE))
#define IS_NUM(value)   (((value) & QNAN) != QNAN)
#define IS_OBJECT(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_NUM(value)   valueToNum(value)
#define AS_BOOL(value)  ((value) == (QNAN | TAG_TRUE))
#define AS_OBJECT(value)  ((Object*)(value & ~(QNAN | SIGN_BIT)))

#define NUM_VAL(num)    numToValue(num)
#define NULL_VAL        ((Value)(uint64_t)(QNAN | TAG_NULL))
#define BOOL_VAL(bool)  ((Value)(uint64_t)(QNAN | (bool ? TAG_TRUE : TAG_FALSE)))
#define OBJECT_VAL(obj) ((Value)(QNAN | SIGN_BIT | (uintptr_t)(obj)))


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
void writeValueArray(VM* vm, ValueArray* array, Value value);
void freeValueArray(VM* vm, ValueArray* array);
void printValue(Value value);

char* valueToString(Value value);

void printObject(Value value);

ValueType getValueType(Value value);
bool isEqual(Value a, Value b);

#endif  // PICO_VALUE_H