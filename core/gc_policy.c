#include <stddef.h>
#include <string.h>

#include "gc_policy.h"
#include "common.h"

static void noopInit(VM* vm){
    // No initialization needed for noop policy
    (void)vm;
}

static void noopshutdown(VM* vm){
    // No shutdown needed for noop policy
    (void)vm;
}

static void noopWriteBarrier(VM* vm, Object* owner, Value value){
    // No write barrier needed for noop policy
    (void)vm;
    (void)owner;
    (void)value;
}

static bool autoShouldCollect(VM* vm, size_t oldSize, size_t newSize){
    (void)vm;

    if(newSize <= oldSize){
        return false;
    }

#ifdef DEBUG_STRESS_GC
    (void)vm;
    return true;
#else
    return vm->bytesAllocated > vm->nextGC;
#endif
}

static bool neverShouldCollect(VM* vm, size_t oldSize, size_t newSize){
    (void)vm;
    (void)oldSize;
    (void)newSize;
    return false;
}

static bool offCollect(VM* vm, GCReason reason){
    (void)vm;
    (void)reason;
    return false;    
    // GC is disabled, so we never collect
}

static void defaultOnAlloc(VM* vm, void* ptr, size_t oldSize, size_t newSize){
    (void)ptr;

    if(vm == NULL|| vm->gcPolicy == NULL || vm->gcPolicy->shouldCollect == NULL){
        return;
    }

    if(vm->gcRunning){
        return;
    }

    if(vm->gcPolicy->shouldCollect(vm, oldSize, newSize)){
#ifdef DEBUG_STRESS_GC
        gcCollect(vm, GC_REASON_STRESS);
#else
        gcCollect(vm, GC_REASON_THRESHOLD);
#endif
    }
}

static const GCPolicy AUTO_POLICY = {
    .name = "auto",
    .init = noopInit,
    .shutdown = noopshutdown,
    .shouldCollect = autoShouldCollect,
    .collect = gcMarkSweep,
    .onAlloc = defaultOnAlloc,
    .writeBarrier = noopWriteBarrier
};

static const GCPolicy MANUAL_POLICY = {
    .name = "manual",
    .init = noopInit,
    .shutdown = noopshutdown,
    .shouldCollect = neverShouldCollect,
    .collect = gcMarkSweep,
    .onAlloc = defaultOnAlloc,
    .writeBarrier = noopWriteBarrier
};

static const GCPolicy OFF_POLICY = {
    .name = "off",
    .init = noopInit,
    .shutdown= noopshutdown,
    .shouldCollect = neverShouldCollect,
    .collect = offCollect,
    .onAlloc = defaultOnAlloc,
    .writeBarrier = noopWriteBarrier
};

void initGC(VM* vm){
    // Initialize GC policy based on current mode
    vm->bytesAllocated = 0;
    vm->gcThreshold = 1024 * 1024 * 10; // 10MB
    vm->nextGC = vm->gcThreshold;
    vm->gcMode = GC_MODE_AUTO;
    vm->gcPolicy = &AUTO_POLICY;
    vm->gcRunning = false;
    memset(&vm->gcStats, 0, sizeof(vm->gcStats));

    if(vm->gcPolicy->init != NULL){
        vm->gcPolicy->init(vm);
    }
}

void shutdownGC(VM* vm){
    if(vm->gcPolicy != NULL && vm->gcPolicy->shutdown != NULL){
        vm->gcPolicy->shutdown(vm);
    }

    vm->gcPolicy = NULL;
    vm->gcRunning = false;
}

const GCPolicy* gcPolicyForMode(GCMode mode){
    switch(mode){
        case GC_MODE_AUTO:
            return &AUTO_POLICY;
        case GC_MODE_MANUAL:
            return &MANUAL_POLICY;
        case GC_MODE_OFF:
            return &OFF_POLICY;
        default:
            return &AUTO_POLICY; // Fallback to auto policy
    }
}

const char* gcModeName(GCMode mode){
    return gcPolicyForMode(mode)->name;
}

bool gcModeFromString(const char* name, GCMode* mode){
    if(strcmp(name, "auto") == 0){
        *mode = GC_MODE_AUTO;
        return true;
    }

    if(strcmp(name, "manual") == 0){
        *mode = GC_MODE_MANUAL;
        return true;
    }

    if(strcmp(name, "off") == 0){
        *mode = GC_MODE_OFF;
        return true;
    }

    return false;
}

void gcSetMode(VM* vm, GCMode mode){
    const GCPolicy* oldPolicy = vm->gcPolicy;
    const GCPolicy* newPolicy = gcPolicyForMode(mode);

    if(oldPolicy != NULL && oldPolicy->shutdown != NULL && oldPolicy != newPolicy){
        oldPolicy->shutdown(vm);
    }

    vm->gcMode = mode;
    vm->gcPolicy = newPolicy;

    if(newPolicy->init != NULL && oldPolicy != newPolicy){
        newPolicy->init(vm);
    }
}

void gcOnAlloc(VM* vm, void* ptr, size_t oldSize, size_t newSize){
    if(vm == NULL || vm->gcPolicy == NULL || vm->gcPolicy->onAlloc == NULL){
        return;
    }

    vm->gcPolicy->onAlloc(vm, ptr, oldSize, newSize);
}

bool gcCollect(VM* vm, GCReason reason){
    if(vm == NULL || vm->gcPolicy == NULL || vm->gcPolicy->collect == NULL){
        return false;
    }

    if(vm->gcRunning){
        return false;   // Prevent reentrant GC
    }

    vm->gcRunning = true;
    bool result = vm->gcPolicy->collect(vm, reason);
    vm->gcRunning = false;

    return result;
}

void gcWriteBarrier(VM* vm, Object* owner, Value value){
    if(vm == NULL || vm->gcPolicy == NULL || vm->gcPolicy->writeBarrier == NULL){
        return;
    }

    vm->gcPolicy->writeBarrier(vm, owner, value);
}
