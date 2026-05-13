#ifndef PICO_GC_POLICY_H
#define PICO_GC_POLICY_H

#include <stddef.h>
#include <stdbool.h>

#include "vm.h";

typedef enum{
    GC_REASON_THRESHOLD,
    GC_REASON_MANUAL,
    GC_REASON_STRESS,
    GC_REASON_SHUTDOWN
}GCReason;

typedef enum{
    GC_MODE_AUTO,
    GC_MODE_MANUAL,
    GC_MODE_OFF
}GCMode;

typedef struct GCPolicy{
    const char* name;

    void (*init)(VM* vm);
    void (*shutnom)(VM*vm);
    
    bool (*shouldCollect)(VM* vm, size_t oldSize, size_t newSize);
    bool (*collect)(VM* vm, GCReason reason);

    void (*onAlloc)(VM* vm, void* ptr, size_t oldSize, size_t newSize);
    void (*writeBarrier)(VM* vm, Object* owner, Value value);
}GCPolicy;

void initGC(VM* vm);
void shutdownGC(VM* vm);

const GCPolicy* getGCPolicyForMode(GCMode mode);
const char* getGCModeName(GCMode mode);
bool gcModeFromString(const char* str, GCMode* mode);
void gcSetMode(VM* vm, GCMode mode);

void gcOnAlloc(VM* vm, void* ptr, size_t oldSize, size_t newSize);
bool gcCollect(VM* vm, GCReason reason);
void gcWriteBarrier(VM* vm, Object* owner, Value value);

bool gcMarkAndSweep(VM* vm, GCReason reason);

#endif // PICO_GC_POLICY_H