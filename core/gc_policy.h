#ifndef CIETO_GC_POLICY_H
#define CIETO_GC_POLICY_H

#include <stddef.h>
#include <stdbool.h>

#include "vm.h"
#include "gc_types.h"

typedef struct GCPolicy{
    const char* name;

    void (*init)(VM* vm);
    void (*shutdown)(VM*vm);
    
    bool (*shouldCollect)(VM* vm, size_t oldSize, size_t newSize);
    bool (*collect)(VM* vm, GCReason reason);

    void (*onAlloc)(VM* vm, void* ptr, size_t oldSize, size_t newSize);
    void (*writeBarrier)(VM* vm, Object* owner, Value value);
}GCPolicy;

void initGC(VM* vm);
void shutdownGC(VM* vm);

const GCPolicy* gcPolicyForMode(GCMode mode);
const char* gcModeName(GCMode mode);
bool gcModeFromString(const char* str, GCMode* mode);
void gcSetMode(VM* vm, GCMode mode);

void gcOnAlloc(VM* vm, void* ptr, size_t oldSize, size_t newSize);
bool gcCollect(VM* vm, GCReason reason);
void gcWriteBarrier(VM* vm, Object* owner, Value value);

bool gcMarkSweep(VM* vm, GCReason reason);

#endif // CIETO_GC_POLICY_H