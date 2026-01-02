#ifndef PICO_DEBUG_H
#define PICO_DEBUG_H

#include "chunk.h"

void dasmChunk(const Chunk* chunk, const char* name);
int dasmInstruction(const Chunk* chunk, int offset);
static int dasmGlobal(const char* opName, const Chunk* chunk, int offset);
static int dasmLGlobal(const char* opName, const Chunk* chunk, int offset);
static int dasmLocal(const char* opName, const Chunk* chunk, int offset);
static int dasmLLocal(const char* opName, const Chunk* chunk, int offset);
static int dasmJump(const char* name, int sign, const Chunk* chunk, int offset); 

static int dasmConstant(const Chunk* chunk, int offset);
static int dasmLConstant(const Chunk* chunk, int offset);
int getLine(const Chunk* chunk, int offset);

#endif  // PICO_DEBUG_H