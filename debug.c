#include <stdio.h>

#include "debug.h"
#include "value.h"

void dasmChunk(const Chunk* chunk, const char* name){
    printf("=== Chunk: %s ===\n", name);
    for(int offset = 0; offset < chunk->count;){
        offset = dasmInstruction(chunk, offset);
    }
}

int dasmInstruction(const Chunk* chunk, int offset){
    printf("%04d ", offset);
    if(offset > 0 && getLine(chunk, offset) == getLine(chunk, offset - 1)){
        printf("\t    ");
    }else{
        printf("(Line:%4d) ", getLine(chunk, offset));
    }

    uint8_t instruction = chunk->code[offset];
    switch(instruction){
        case OP_CONSTANT:
            return dasmConstant(chunk, offset);
        case OP_LCONSTANT:
            return dasmLConstant(chunk, offset);
        case OP_NULL:
            printf("OP_NULL\n");            return offset + 1;
        case OP_TRUE:
            printf("OP_TRUE\n");            return offset + 1;
        case OP_FALSE:
            printf("OP_FALSE\n");           return offset + 1;
        case OP_NOT:
            printf("OP_NOT\n");             return offset + 1;
        case OP_NOT_EQUAL:
            printf("OP_NOT_EQUAL\n");       return offset + 1;
        case OP_EQUAL:
            printf("OP_EQUAL\n");           return offset + 1;
        case OP_GREATER:
            printf("OP_GREATER\n");         return offset + 1;
        case OP_LESS:
            printf("OP_LESS\n");            return offset + 1;
        case OP_GREATER_EQUAL:
            printf("OP_GREATER_EQUAL\n");   return offset + 1;
        case OP_LESS_EQUAL:
            printf("OP_LESS_EQUAL\n");      return offset + 1;
        case OP_ADD:
            printf("OP_ADD\n");             return offset + 1;
        case OP_SUBTRACT:
            printf("OP_SUBTRACT\n");        return offset + 1;
        case OP_MULTIPLY:
            printf("OP_MULTIPLY\n");        return offset + 1;
        case OP_DIVIDE:
            printf("OP_DIVIDE\n");          return offset + 1;
        case OP_NEGATE:
            printf("OP_NEGATE\n");          return offset + 1;
        case OP_PRINT:
            printf("OP_PRINT\n");           return offset + 1;
        case OP_POP:
            printf("OP_POP\n");             return offset + 1;
        case OP_RETURN:
            printf("OP_RETURN\n");          return offset + 1;

        case OP_DEFINE_GLOBAL:
            return dasmGlobal("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_DEFINE_LGLOBAL:
            return dasmLGlobal("OP_DEFINE_LGLOBAL", chunk, offset);
        case OP_GET_GLOBAL:
            return dasmGlobal("OP_GET_GLOBAL", chunk, offset);
        case OP_GET_LGLOBAL:
            return dasmLGlobal("OP_GET_LGLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return dasmGlobal("OP_SET_GLOBAL", chunk, offset);
        case OP_SET_LGLOBAL:
            return dasmLGlobal("OP_SET_LGLOBAL", chunk, offset);
        case OP_GET_LOCAL:
            return dasmLocal("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return dasmLocal("OP_SET_LOCAL", chunk, offset);
        case OP_GET_LLOCAL:
            return dasmLLocal("OP_GET_LLOCAL", chunk, offset);
        case OP_SET_LLOCAL:
            return dasmLLocal("OP_SET_LLOCAL", chunk, offset);
        case OP_JUMP:
            return dasmJump("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return dasmJump("OP_JUMP_IF_FALSE", 1, chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

static int dasmConstant(const Chunk* chunk, int offset){
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("OP_CONSTANT %d ", constantIndex);
    if(constantIndex < chunk->constants.count){
        printValue(chunk->constants.values[constantIndex]);
    }else{
        printf("Unknown constant");
    }
    printf("\n");
    return offset + 2; 
}

static int dasmLConstant(const Chunk* chunk, int offset){
    uint32_t constantIndex = (uint32_t)chunk->code[offset + 1] | 
                                      (chunk->code[offset + 2] << 8) |
                                      (chunk->code[offset + 3] << 16);
    printf("OP_LCONSTANT %d '", constantIndex);
    if(constantIndex < chunk->constants.count){
        printValue(chunk->constants.values[constantIndex]);
    }else{
        printf("Unknown long constant");
    }
    printf("'\n");
    return offset + 4; 
    // 1 byte for opcode + 1 byte for long constant index
}

static int dasmGlobal(const char* opName, const Chunk* chunk, int offset) {
    uint8_t constantIndex = chunk->code[offset + 1];
    printf("%-22s %d '", opName, constantIndex); // Adjusted alignment for longer names
    if(constantIndex < chunk->constants.count) {
        printValue(chunk->constants.values[constantIndex]);
    }else{
        printf("Unknown variable");
    }
    printf("'\n");
    return offset + 2;
}

static int dasmLGlobal(const char* opName, const Chunk* chunk, int offset) {
    uint32_t constantIndex = (uint32_t)chunk->code[offset + 1] |
                                    (chunk->code[offset + 2] << 8) |
                                    (chunk->code[offset + 3] << 16);
    printf("%-22s %d '", opName, constantIndex); // Adjusted alignment
    if(constantIndex < chunk->constants.count){
        printValue(chunk->constants.values[constantIndex]);
    }else{
        printf("Unknown long variable");
    }
    printf("'\n");
    return offset + 4;
}

static int dasmLocal(const char* opName, const Chunk* chunk, int offset){
    uint8_t slot = chunk->code[offset+1];
    printf("%-16s %4d\n", opName, slot);
    return offset + 2;
}

static int dasmLLocal(const char* opName, const Chunk* chunk, int offset){
    uint16_t slot = (uint16_t)(chunk->code[offset+1] | (chunk->code[offset+2] << 8));
    printf("%-16s %4d\n", opName, slot);
    return offset + 3;
}

static int dasmJump(const char* name, int sign, const Chunk* chunk, int offset){
    uint16_t jump = (uint16_t)(chunk->code[offset+1] << 8) | chunk->code[offset+2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int getLine(const Chunk* chunk, int offset){
    int low = 0;
    int high = chunk->lineCount - 1;
    int match = -1;
    while(low <= high){
        int mid = low + ((high - low) >> 1);
        if(chunk->lines[mid * 2] <= offset){
            match = mid;
            low = mid + 1;
        }else{
            high = mid - 1;
        }
    }

    if(match != -1){
        return chunk->lines[match * 2 + 1];
    }

    return -1;  // No matching line found
}