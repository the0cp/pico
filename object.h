#ifndef PICO_OBJECT_H
#define PICO_OBJECT_H

#include <stddef.h>

#include "chunk.h"

typedef struct VM VM;

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)
#define IS_STRING(value) (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_STRING)
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)
#define IS_FUNC(value) (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_FUNC)
#define AS_FUNC(value) ((ObjectFunc*)AS_OBJECT(value))

typedef enum{
    OBJECT_STRING,
    OBJECT_FUNC,
}ObjectType;

typedef struct Object{
    ObjectType type;
    struct Object* next;
}Object;

typedef struct ObjectString{
    Object obj;
    size_t length;
    // char* chars;  // Pointer to the string characters
    uint64_t hash;
    char chars[];   // Flexible array member
}ObjectString;

ObjectString* copyString(VM* vm, const char* chars, int len);

typedef struct ObjectFunc{
    Object obj;
    int arity;
    Chunk chunk;
    ObjectString* name;
}ObjectFunc;

ObjectFunc* newFunction(VM* vm);

#endif // PICO_OBJECT_H
