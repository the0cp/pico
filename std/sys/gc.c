#include <string.h>

#include "registry.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "mem.h"

static const char* gcModeToStr(GCMode mode){
    switch(mode){
        case GC_MODE_AUTO: 
            return "auto";
        case GC_MODE_MANUAL: 
            return "manual";
        case GC_MODE_OFF: 
            return "off";
        default: 
            return "unknown";
    }
}

static bool getGcMode(const char* modeStr, GCMode* mode){
    if(strcmp(modeStr, "auto") == 0){
        *mode = GC_MODE_AUTO;
        return true;
    }else if(strcmp(modeStr, "manual") == 0){
        *mode = GC_MODE_MANUAL;
        return true;
    }else if(strcmp(modeStr, "off") == 0){
        *mode = GC_MODE_OFF;
        return true;
    }
    return false;
}

static Value gc_mode(VM* vm, int argCount, Value* args){
    if(argCount == 0){
        const char* modeStr = gcModeToStr(vm->gcMode);
        return OBJECT_VAL(copyString(vm, modeStr, (int)strlen(modeStr)));
    }

    if(argCount != 1 || !IS_STRING(args[0])){
        runtimeError(vm, "gc.mode expects a single string argument.\n");
        return NULL_VAL;
    }

    GCMode mode;
    const char* modeStr = AS_CSTRING(args[0]);

    if(!getGcMode(modeStr, &mode)){
        runtimeError(vm, "Invalid GC mode: %s. Valid modes are 'auto', 'manual', 'off'.\n", modeStr);
        return NULL_VAL;
    }

    vm->gcMode = mode;
    return args[0];
}

static Value gc_collect(VM* vm, int argCount, Value* args){
    if(argCount != 0){
        runtimeError(vm, "gc.collect does not take any arguments.\n");
        return NULL_VAL;
    }

    if(vm->gcMode == GC_MODE_OFF){
        return BOOL_VAL(false);
    }

    collectGarbage(vm);
    return BOOL_VAL(true);
}

static Value gc_threshold(VM* vm, int argCount, Value* args){
    if(argCount == 0){
        return NUM_VAL((double)vm->gcThreshold);
    }

    if(argCount != 1 || !IS_NUM(args[0])){
        runtimeError(vm, "gc.threshold expects a single numeric argument.\n");
        return NULL_VAL;
    }

    double threshold = AS_NUM(args[0]);

    if(threshold < 0){
        runtimeError(vm, "gc.threshold expects a non-negative number.\n");
        return NULL_VAL;
    }

    vm->gcThreshold = (size_t)threshold;
    vm->nextGC = vm->gcThreshold;

    return NUM_VAL((double)vm->gcThreshold);
}

static int countObjects(VM* vm){
    int count = 0;
    Object* obj = vm->objects;
    while(obj){
        count++;
        obj = obj->next;
    }
    return count;
}

static void mapCString(VM* vm, ObjectMap* map, const char* key, Value value){
    ObjectString* keyStr = copyString(vm, key, (int)strlen(key));
    tableSet(vm, &map->table, OBJECT_VAL(keyStr), value);
}

static Value gc_stats(VM* vm, int argCount, Value* args){
    if(argCount != 0){
        runtimeError(vm, "gc.stats does not take any arguments.\n");
        return NULL_VAL;
    }

    ObjectMap* statsMap = newMap(vm);
    push(vm, OBJECT_VAL(statsMap));

    const char* modeName = gcModeToStr(vm->gcMode);
    ObjectString* modeStr = copyString(vm, modeName, (int)strlen(modeName));

    mapCString(vm, statsMap, "mode", OBJECT_VAL(modeStr));
    mapCString(vm, statsMap, "bytes", NUM_VAL((double)vm->bytesAllocated));
    mapCString(vm, statsMap, "next", NUM_VAL((double)vm->nextGC));
    mapCString(vm, statsMap, "threshold", NUM_VAL((double)vm->gcThreshold));
    mapCString(vm, statsMap, "objects", NUM_VAL((double)countObjects(vm)));

    pop(vm);
    return OBJECT_VAL(statsMap);
}

void registerGcModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "gc", 2);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineCFunc(vm, &module->members, "mode", gc_mode);
    defineCFunc(vm, &module->members, "collect", gc_collect);
    defineCFunc(vm, &module->members, "threshold", gc_threshold);
    defineCFunc(vm, &module->members, "stats", gc_stats);

    tableSet(vm, &vm->modules, OBJECT_VAL(moduleName), OBJECT_VAL(module));

    pop(vm);
    pop(vm);
}