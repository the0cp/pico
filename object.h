#ifndef PICO_OBJECT_H
#define PICO_OBJECT_H

#include <stddef.h>

#include "chunk.h"
#include "hashtable.h"

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

#define IS_MODULE(value)    (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_MODULE)
#define AS_MODULE(value)    ((ObjectModule*)AS_OBJECT(value))

#define IS_CLOSURE(value)   (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_CLOSURE)
#define AS_CLOSURE(value)   ((ObjectClosure*)AS_OBJECT(value))


typedef enum{
    OBJECT_STRING,
    OBJECT_FUNC,
    OBJECT_CFUNC,
    OBJECT_MODULE,
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
}ObjectType;

typedef struct Object{
    ObjectType type;
    bool isMarked;
    struct Object* next;
}Object;

typedef struct ObjectString{
    Object obj;
    size_t length;
    uint64_t hash;
    char chars[];   // Flexible array member
}ObjectString;

ObjectString* copyString(VM* vm, const char* chars, int len);

typedef enum{
    TYPE_FUNC,
    TYPE_SCRIPT,
    TYPE_MODULE,
}FuncType;

typedef struct ObjectFunc{
    Object obj;
    int arity;
    int upvalueCnt;
    Chunk chunk;
    ObjectString* name;
    ObjectString* srcName;
    FuncType type;
}ObjectFunc;

ObjectFunc* newFunction(VM* vm);

typedef struct ObjectCFunc{
    Object obj;
    CFunc func;
}ObjectCFunc;

ObjectCFunc* newCFunc(VM* vm, CFunc func);

typedef struct ObjectModule{
    Object obj;
    ObjectString* name;
    HashTable members;
}ObjectModule;

ObjectModule* newModule(VM* vm, ObjectString* name);

typedef struct ObjectUpvalue{
    Object obj;
    Value* location; // pointing to local on stack when upvalue is opened
    Value closed;    // value when upvalue is closed
    struct ObjectUpvalue* next;
}ObjectUpvalue;

typedef struct ObjectClosure{
    Object obj;
    ObjectFunc* func; // point to func template
    int upvalueCnt;
    ObjectUpvalue* upvalues[]; // pointer array
}ObjectClosure;

ObjectUpvalue* newUpvalue(VM* vm, Value* slot);
ObjectClosure* newClosure(VM* vm, ObjectFunc* func);

void freeObject(VM* vm, Object* object);
void freeObjects(VM* vm);

#endif // PICO_OBJECT_H
