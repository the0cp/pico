#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "common.h"
#include "mem.h"
#include "vm.h"
#include "object.h"
#include "value.h"
#include "list.h"
#include "modules.h"
#include "string.h"

Value string_len(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(!IS_STRING(receiver)){
        runtimeError(vm, ".len() expect a string.\n");
        return NULL_VAL;
    }
    return NUM_VAL((double)AS_STRING(receiver)->length);
}

Value string_sub(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(argCount < 1 || !IS_NUM(args[0])){
        runtimeError(vm, ".sub() expects (start, [end]).\n");
        return NULL_VAL;
    }

    ObjectString* strObj = AS_STRING(receiver);
    int len = (int)strObj->length;
    int st = (int)AS_NUM(args[0]);
    int end = len;

    if(argCount > 1 && IS_NUM(args[1])){
        end = (int)AS_NUM(args[1]);
    }

    if(st < 0)  st = len + st;
    if(end < 0) end = len + end;

    if(st < 0) st = 0;
    if(end > len) end = len;

    if(st >= end) return OBJECT_VAL(copyString(vm, "", 0));

    return OBJECT_VAL(copyString(vm, strObj->chars + st, end - st));
}

Value string_trim(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    ObjectString* strObj = AS_STRING(receiver);
    
    char* st = strObj->chars;
    char* end = st + strObj->length - 1;

    while(*st && isspace((unsigned char)*st)) st++;
    while(end > st && isspace((unsigned char)*end)) end--;

    int len = (int)(end - st + 1);
    if(len <= 0) return OBJECT_VAL(copyString(vm, "", 0));
    
    return OBJECT_VAL(copyString(vm, st, len));
}

Value string_upper(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    ObjectString* strObj = AS_STRING(receiver);
    
    char* chars = (char*)malloc(strObj->length + 1);
    for(int i = 0; i < strObj->length; i++){
        chars[i] = toupper((unsigned char)strObj->chars[i]);
    }
    chars[strObj->length] = '\0';
    return OBJECT_VAL(takeString(vm, chars, strObj->length));
}

Value string_lower(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    ObjectString* strObj = AS_STRING(receiver);
    
    char* chars = (char*)malloc(strObj->length + 1);
    for(int i = 0; i < strObj->length; i++){
        chars[i] = tolower((unsigned char)strObj->chars[i]);
    }
    chars[strObj->length] = '\0';
    return OBJECT_VAL(takeString(vm, chars, strObj->length));
}

Value string_find(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(argCount != 1 || !IS_STRING(args[0])){
        return NUM_VAL(-1);
    }
    ObjectString* str = AS_STRING(receiver);
    ObjectString* sub = AS_STRING(args[0]);

    char* pos = strstr(str->chars, sub->chars);
    if(pos == NULL) return NUM_VAL(-1);
    return NUM_VAL((double)(pos - str->chars));
}

Value string_split(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(argCount != 1 || !IS_STRING(args[0])){
        runtimeError(vm, ".split() expects a delimiter string.\n");
        return NULL_VAL;
    }

    ObjectString* strObj = AS_STRING(receiver);
    ObjectString* delimObj = AS_STRING(args[0]);
    
    char* str = strObj->chars;
    char* delim = delimObj->chars;
    int delimLen = (int)delimObj->length;

    ObjectList* list = newList(vm);
    push(vm, OBJECT_VAL(list));

    if(delimLen == 0){
        for(int i = 0; i < strObj->length; i++){
            ObjectString* charStr = copyString(vm, str + i, 1);
            push(vm, OBJECT_VAL(charStr));
            appendToList(vm, list, OBJECT_VAL(charStr));
            pop(vm);
        }
    }else{
        char* ptr = str;
        char* nextMatch;
        while((nextMatch = strstr(ptr, delim)) != NULL){
            int len = (int)(nextMatch - ptr);
            ObjectString* segment = copyString(vm, ptr, len);
            push(vm, OBJECT_VAL(segment));
            appendToList(vm, list, OBJECT_VAL(segment));
            pop(vm);
            ptr = nextMatch + delimLen;
        }
        ObjectString* last = copyString(vm, ptr, (int)strlen(ptr));
        push(vm, OBJECT_VAL(last));
        appendToList(vm, list, OBJECT_VAL(last));
        pop(vm);
    }
    pop(vm); // list
    return OBJECT_VAL(list);
}

Value string_replace(VM* vm, int argCount, Value* args){
    Value receiver = args[-1];
    if(argCount != 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])){
        return receiver;
    }

    ObjectString* str = AS_STRING(receiver);
    ObjectString* oldStr = AS_STRING(args[0]);
    ObjectString* newStr = AS_STRING(args[1]);

    if(oldStr->length == 0) return receiver;

    int count = 0;
    const char* tmp = str->chars;
    while((tmp = strstr(tmp, oldStr->chars)) != NULL){
        count++;
        tmp += oldStr->length;
    }

    if(count == 0) return receiver;

    size_t newLen = str->length + count * (newStr->length - oldStr->length);
    char* result = (char*)malloc(newLen + 1);
    char* dst = result;
    const char* src = str->chars;
    const char* p;

    while((p = strstr(src, oldStr->chars)) != NULL){
        size_t len = p - src;
        memcpy(dst, src, len);
        dst += len;
        memcpy(dst, newStr->chars, newStr->length);
        dst += newStr->length;
        src = p + oldStr->length;
    }
    strcpy(dst, src);

    return OBJECT_VAL(takeString(vm, result, (int)newLen));
}

static Value stringModule_ascii(VM* vm, int argCount, Value* args){
    if(argCount != 1 || !IS_NUM(args[0]))   return NULL_VAL;
    int code = (int)AS_NUM(args[0]);
    char c = (char)code;
    return OBJECT_VAL(copyString(vm, &c, 1));
}

static void defineCFunc(VM* vm, HashTable* table, const char* name, CFunc func){
    push(vm, OBJECT_VAL(copyString(vm, name, (int)strlen(name))));
    push(vm, OBJECT_VAL(newCFunc(vm, func)));
    tableSet(vm, table, peek(vm, 1), peek(vm, 0));
    pop(vm);
    pop(vm);
}

void registerStringModule(VM* vm){
    ObjectString* moduleName = copyString(vm, "string", 6);
    push(vm, OBJECT_VAL(moduleName));

    ObjectModule* module = newModule(vm, moduleName);
    push(vm, OBJECT_VAL(module));

    defineCFunc(vm, &module->members, "ascii", stringModule_ascii);

    tableSet(vm, &vm->modules, OBJECT_VAL(moduleName), OBJECT_VAL(module));
    pop(vm);
    pop(vm);
}