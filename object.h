#ifndef PICO_OBJECT_H
#define PICO_OBJECT_H

#include <stddef.h>
#include <stdio.h>

#include "chunk.h"
#include "hashtable.h"

typedef struct VM VM;

typedef Value (*CFunc)(VM* vm, int argCount, Value* args);

#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)

#define IS_STRING(value)        (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_STRING)
#define AS_STRING(value)        ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value)       (((ObjectString*)AS_OBJECT(value))->chars)

#define IS_LIST(value)          (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_LIST)
#define AS_LIST(value)          ((ObjectList*)AS_OBJECT(value))

#define IS_MAP(value)           (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_MAP)
#define AS_MAP(value)           ((ObjectMap*)AS_OBJECT(value))

#define IS_FUNC(value)          (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_FUNC)
#define AS_FUNC(value)          ((ObjectFunc*)AS_OBJECT(value))

#define IS_CFUNC(value)         (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_CFUNC)
#define AS_CFUNC(value)         (((ObjectCFunc*)AS_OBJECT(value))->func)

#define IS_MODULE(value)        (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_MODULE)
#define AS_MODULE(value)        ((ObjectModule*)AS_OBJECT(value))

#define IS_CLOSURE(value)       (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_CLOSURE)
#define AS_CLOSURE(value)       ((ObjectClosure*)AS_OBJECT(value))

#define IS_CLASS(value)         (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_CLASS)
#define AS_CLASS(value)         ((ObjectClass*)AS_OBJECT(value))

#define IS_INSTANCE(value)      (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_INSTANCE)
#define AS_INSTANCE(value)      ((ObjectInstance*)AS_OBJECT(value))

#define IS_BOUND_METHOD(value)  (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_BOUND_METHOD)
#define AS_BOUND_METHOD(value)  ((ObjectBoundMethod*)AS_OBJECT(value))

#define IS_FILE(value)          (IS_OBJECT(value) && OBJECT_TYPE(value) == OBJECT_FILE)
#define AS_FILE(value)          ((ObjectFile*)AS_OBJECT(value))

typedef enum{
    OBJECT_STRING,
    OBJECT_LIST,
    OBJECT_MAP,
    OBJECT_FUNC,
    OBJECT_CFUNC,
    OBJECT_MODULE,
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
    OBJECT_CLASS,
    OBJECT_INSTANCE,
    OBJECT_BOUND_METHOD,
    OBJECT_FILE,
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
ObjectString* takeString(VM* vm, char* chars, int length);

typedef struct ObjectList{
    Object obj;
    int count;
    int capacity;
    Value* items;
}ObjectList;

ObjectList* newList(VM* vm);
void appendToList(VM* vm, ObjectList* list, Value value);
void insertToList(VM* vm, ObjectList* list, int index, Value value);
Value getListItem(VM* vm, ObjectList* list, int index);
void deleteListItem(VM* vm, ObjectList* list, int index);

typedef struct ObjectMap{
    Object obj;
    HashTable table;
}ObjectMap;

ObjectMap* newMap(VM* vm);

typedef enum{
    TYPE_FUNC,
    TYPE_SCRIPT,
    TYPE_MODULE,
    TYPE_METHOD,
}FuncType;

typedef struct ObjectFunc{
    Object obj;
    int arity;
    int upvalueCnt;
    Chunk chunk;
    ObjectString* name;
    ObjectString* srcName;
    FuncType type;
    struct ObjectClass* fieldOwner;
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

typedef struct ObjectClass{
    Object obj;
    ObjectString* name;
    HashTable methods;
    HashTable fields;
}ObjectClass;

typedef struct ObjectInstance{
    Object obj;
    ObjectClass* klass;
    HashTable fields;
}ObjectInstance;

ObjectClass* newClass(VM* vm, ObjectString* name);
ObjectInstance* newInstance(VM* vm, ObjectClass* klass);

typedef struct ObjectBoundMethod{
    Object obj;
    Value receiver;
    Object* method;
}ObjectBoundMethod;

ObjectBoundMethod* newBoundMethod(VM* vm, Value receiver, Object* method);

typedef struct ObjectFile{
    Object obj;
    FILE* handle;
    char* mode;
    bool isOpen;
}ObjectFile;

ObjectFile* newFile(VM* vm, FILE* file);

void freeObject(VM* vm, Object* object);
void freeObjects(VM* vm);

#endif // PICO_OBJECT_H
