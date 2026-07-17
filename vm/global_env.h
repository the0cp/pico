#ifndef CIETO_GLOBAL_ENV_H
#define CIETO_GLOBAL_ENV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "value.h"

typedef struct VM VM;
typedef struct ObjectString ObjectString;

typedef struct{
    ObjectString* name;
    uint32_t slot;
}GlobalNameEntry;

typedef struct{
    size_t count;
    size_t capacity;
    GlobalNameEntry* entries;
}GlobalNameMap;

typedef struct{
    GlobalNameMap names;
    Value* values;
    size_t count;
    size_t capacity;
}GlobalEnv;

void initGlobalEnv(GlobalEnv* env);
void freeGlobalEnv(VM* vm, GlobalEnv* env);

bool globalResolveSlot(GlobalEnv* env, ObjectString* name, uint32_t* slot);
bool globalEnsureSlot(VM* vm, GlobalEnv* env, ObjectString* name, uint32_t* slot);

bool globalGetSlot(GlobalEnv* env, uint32_t slot, Value* out);
bool globalSetSlot(GlobalEnv* env, uint32_t slot, Value value);

bool globalGetName(GlobalEnv* env, ObjectString* name, Value* out);
bool globalSetName(VM* vm, GlobalEnv* env, ObjectString* name, Value value);

void markGlobalEnv(VM* vm, GlobalEnv* env);


#endif // CIETO_GLOBAL_ENV_H