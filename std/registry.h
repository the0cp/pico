#ifndef PICO_MODULES_H
#define PICO_MODULES_H

#include "vm.h"

typedef void (*NativeModuleInit)(VM* vm, ObjectModule* module);

typedef struct NativeModuleDef{
    const char* name;
    NativeModuleInit initFunc;
}NativeModuleDef;

void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func);

const NativeModuleDef* findNativeModule(const char* name);

void registerPrelude(VM* vm);

void initFsModule(VM* vm, ObjectModule* module);
void initTimeModule(VM* vm, ObjectModule* module);
void initOsModule(VM* vm, ObjectModule* module);
void initPathModule(VM* vm, ObjectModule* module);
void initGlobModule(VM* vm, ObjectModule* module);
void initGcModule(VM* vm, ObjectModule* module);
void initStringModule(VM* vm, ObjectModule* module);

Value iterNative(VM* vm, int argCount, Value* args);
Value nextNative(VM* vm, int argCount, Value* args);

// String Functions
Value string_len(VM* vm, int argCount, Value* args);
Value string_sub(VM* vm, int argCount, Value* args);
Value string_trim(VM* vm, int argCount, Value* args);
Value string_upper(VM* vm, int argCount, Value* args);
Value string_lower(VM* vm, int argCount, Value* args);
Value string_find(VM* vm, int argCount, Value* args);
Value string_split(VM* vm, int argCount, Value* args);
Value string_replace(VM* vm, int argCount, Value* args);

// List Functions
Value list_push(VM* vm, int argCount, Value* args);
Value list_pop(VM* vm, int argCount, Value* args);
Value list_size(VM* vm, int argCount, Value* args);

// File Functions
Value file_read(VM* vm, int argCount, Value* args);
Value file_write(VM* vm, int argCount, Value* args);
Value file_close(VM* vm, int argCount, Value* args);
Value file_readLine(VM* vm, int argCount, Value* args);

#endif // PICO_MODULES_H