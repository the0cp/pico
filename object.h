#ifndef PICO_OBJECT_H
#define PICO_OBJECT_H

#include <stddef.h>

#define OBJECT_TYPE(value) (AS_OBJECT(value) -> type)
#define IS_STRING(value) (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_STRING)
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value)) -> chars)

typedef enum{
    OBJECT_STRING,

}ObjectType;

typedef struct{
    ObjectType type;
}Object;

typedef struct{
    Object obj;
    size_t length;
    // char* chars;  // Pointer to the string characters
    uint64_t hash;
    char chars[];   // Flexible array member
}ObjectString;

ObjectString* copyString(const char* chars, int len);

#endif // PICO_OBJECT_H
