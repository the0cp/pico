#ifndef PICO_DEBUG_H
#define PICO_DEBUG_H

#include "chunk.h"

void dasmChunk(Chunk* chunk, const char* name);
void dasmInstruction(Chunk* chunk, int offset);
int getLine(const Chunk* chunk, int offset);

static void dasmABC(const char* name, Instruction instruction);
static void dasmABx(const char* name, Instruction instruction);
static void dasmAsBx(const char* name, Instruction instruction);
static void dasmLoadK(const char* name, const Chunk* chunk, Instruction instruction);
static void dasmField(const char* name, const Chunk* chunk, Instruction instruction);
static void dasmGlobal(const char* name, Chunk* chunk, Instruction instruction);

#endif  // PICO_DEBUG_H