#include <string.h>

#include "gc_policy.h"
#include "common.h"

static void noopInit(VM* vm){
    // No initialization needed for noop policy
    (void)vm;
}

static void noopShutnom(VM* vm){
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
    return true;    // Always succeed, but never collect
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
    .shutnom = noopShutnom,
    .shouldCollect = autoShouldCollect,
    .collect = gcMarkAndSweep,
    .onAlloc = defaultOnAlloc,
    .writeBarrier = noopWriteBarrier
};

static const GCPolicy MANUAL_POLICY = {
    .name = "manual",
    .init = noopInit,
    .shutnom = noopShutnom,
    .shouldCollect = neverShouldCollect,
    .collect = gcMarkAndSweep,
    .onAlloc = defaultOnAlloc,
    .writeBarrier = noopWriteBarrier
};

static const GCPolicy OFF_POLICY = {
    .name = "off",
    .init = noopInit,
    .shutnom = noopShutnom,
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

    if(vm->gcPolicy->init != NULL){
        vm->gcPolicy->init(vm);
    }
}