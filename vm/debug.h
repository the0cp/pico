#ifndef CIETO_DEBUG_H
#define CIETO_DEBUG_H

#include "chunk.h"
#include "global_env.h"

typedef struct ObjectFunc ObjectFunc;

void dasmChunk(Chunk* chunk, const char* name, GlobalEnv* globals);
void dasmInstruction(Chunk* chunk, int offset, GlobalEnv* globals);

void dasmFunction(ObjectFunc* func, GlobalEnv* globals);
int getLine(const Chunk* chunk, int offset);

static void dasmABC(const char* name, Instruction instruction);
static void dasmABx(const char* name, Instruction instruction);
static void dasmAsBx(const char* name, Instruction instruction);
static void dasmLoadK(const char* name, const Chunk* chunk, Instruction instruction);
static void dasmField(const char* name, const Chunk* chunk, Instruction instruction);
static void dasmGlobal(const char* name, GlobalEnv* globals, Instruction instruction);

#endif  // CIETO_DEBUG_H