#ifndef PICO_OBJECT_H
#define PICO_OBJECT_H

#include <stddef.h>

#include "chunk.h"

typedef struct VM VM;

typedef Value (*CFunc)(int argCount, Value* args);

#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)
#define IS_STRING(value)    (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_STRING)
#define AS_STRING(value)    ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((ObjectString*)AS_OBJECT(value))->chars)
#define IS_FUNC(value)      (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_FUNC)
#define AS_FUNC(value)      ((ObjectFunc*)AS_OBJECT(value))
#define IS_CFUNC(value)     (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_CFUNC)
#define AS_CFUNC(value)     (((ObjectCFunc*)AS_OBJECT(value))->func)

typedef enum{
    OBJECT_STRING,
    OBJECT_FUNC,
    OBJECT_CFUNC,
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

typedef struct ObjectCFunc{
    Object obj;
    CFunc func;
}ObjectCFunc;

ObjectCFunc* newCFunc(VM* vm, CFunc* func);

#endif // PICO_OBJECT_H
