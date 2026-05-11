#include <string.h>

#include "module_loader.h"
#include "object.h"
#include "value.h"
#include "registry.h"
#include "compiler.h"
#include "file.h"

#ifdef _WIN32
    #define PATH_SEP '\\'
    #define PATH_SEP_STR "\\"
#else
    #define PATH_SEP '/'
    #define PATH_SEP_STR "/"
#endif

static bool hasSep(const char* path){
    return strchr(path, PATH_SEP) != NULL;
}

static bool endWith(const char* str, const char* suffix){
    size_t len = strlen(str);
    size_t suffixLen = strlen(suffix);

    if(suffixLen > len) return false;

    return memcmp(str + len - suffixLen, suffix, suffixLen) == 0;
}

static bool isScriptModule(const char* str){
    return hasSep(str) || endWith(str, ".pcs");
}

static ObjectString* getModuleName(VM* vm, const char* spec){
    const char* start = spec;

    for(const char* p = spec; *p != '\0'; p++){
        if(*p == PATH_SEP){
            start = p + 1;
        }
    }

    const char* end = start + strlen(start);

    if(end - start >= 4 && strcmp(end - 4, ".pcs") == 0){
        end -= 4;
    }else{
        for(const char* p = end; p > start; p--){
            if(*(p - 1) == '.'){
                end = p - 1;
                break;
            }
        }
    }

    if(end <= start){
        return copyString(vm, spec, (int)strlen(spec));
    }

    return copyString(vm, start, (int)(end - start));
}

static InterpreterStatus loadNativeModule(
    VM* vm, 
    const NativeModuleDef* nativeDef,
    ImportResult* result
){
    ObjectString* key = copyString(vm, nativeDef->name, (int)strlen(nativeDef->name));
    push(vm, OBJECT_VAL(key));

    Value cached;
    if(tableGet(vm, &vm->modCache, OBJECT_VAL(key), &cached)){
        result->module = AS_MODULE(cached);
        result->closure = NULL;
        pop(vm);    // key
        return VM_OK;
    }

    ObjectModule* module = newModule(vm, key, NULL, MODULE_NATIVE);
    push(vm, OBJECT_VAL(module));

    tableSet(vm, &vm->modCache, OBJECT_VAL(key), OBJECT_VAL(module));

    nativeDef->initFunc(vm, module);

    result->module = module;
    result->closure = NULL;

    pop(vm);    // module
    pop(vm);    // key

    return VM_OK;
}

static InterpreterStatus loadScriptModule(
    VM* vm,
    ObjectString* spec,
    ImportResult* result
){
    push(vm, OBJECT_VAL(spec));

    Value cache;
    if(tableGet(vm, &vm->modCache, OBJECT_VAL(spec), &cache)){
        result->module = AS_MODULE(cache);
        result->closure = NULL;
        pop(vm);    // spec
        return VM_OK;
    }

    char* source = readScript(spec->chars);
    if(source == NULL){
        runtimeError(vm, "Failed to read module file '%s'.", spec->chars);
        pop(vm);    // spec
        return VM_RUNTIME_ERROR;
    }

    ObjectFunc* func = compile(vm, source, spec->chars);
    free(source);

    if(func == NULL){
        pop(vm);    // spec
        return VM_COMPILE_ERROR;
    }

    func->type = TYPE_MODULE;
    push(vm, OBJECT_VAL(func));

    ObjectString* moduleName = getModuleName(vm, spec->chars);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName, spec, MODULE_SCRIPT);
    push(vm, OBJECT_VAL(module));

    tableSet(vm, &vm->modCache, OBJECT_VAL(spec), OBJECT_VAL(module));

    ObjectClosure* closure = newClosure(vm, func);
    if(closure == NULL){
        runtimeError(vm, "Failed to create closure for module '%s'.", spec->chars);
        pop(vm);    // module
        pop(vm);    // moduleName
        pop(vm);    // func
        pop(vm);    // spec
        return VM_RUNTIME_ERROR;
    }

    result->module = module;
    result->closure = closure;

    pop(vm);    // module
    pop(vm);    // moduleName
    pop(vm);    // func
    pop(vm);    // spec

    return VM_OK;
}

InterpreterStatus importModule(
    VM* vm,
    ObjectString* spec,
    const char* requester,
    ImportResult* result
){
    (void)requester;

    result->module = NULL;
    result->closure = NULL;

    if(!isScriptModule(spec->chars)){
        const NativeModuleDef* nativeDef = findNativeModule(spec->chars);

        if(nativeDef != NULL){
            return loadNativeModule(vm, nativeDef, result);
        }

        runtimeError(vm, "Module '%s' not found.", spec->chars);
        return VM_RUNTIME_ERROR;
    }

    return loadScriptModule(vm, spec, result);
}

