#include <stdio.h>

#include "debug.h"
#include "value.h"

void dasmChunk(const Chunk* chunk, const char* name){
    printf("=== Chunk: %s ===\n", name);
    for(int offset = 0; offset < chunk -> count;){
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

    uint8_t instruction = chunk -> code[offset];
    switch(instruction){
        case OP_CONSTANT:
            return dasmConstant(chunk, offset);
        case OP_LCONSTANT:
            return dasmLConstant(chunk, offset);
        case OP_ADD:
            printf("OP_ADD\n");
            return offset + 1;
        case OP_SUBTRACT:
            printf("OP_SUBTRACT\n");
            return offset + 1;
        case OP_MULTIPLY:
            printf("OP_MULTIPLY\n");
            return offset + 1;
        case OP_DIVIDE:
            printf("OP_DIVIDE\n"); 
            return offset + 1;
        case OP_NEGATE:
            printf("OP_NEGATE\n");
            return offset + 1;
        case OP_RETURN:
            printf("OP_RETURN\n");
            return offset + 1;
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

static int dasmConstant(const Chunk* chunk, int offset){
    uint8_t constantIndex = chunk -> code[offset + 1];
    printf("OP_CONSTANT %d '", constantIndex);
    if(constantIndex < chunk -> constants.count){
        printf("%g", chunk -> constants.values[constantIndex]);
    }else{
        printf("Unknown constant");
    }
    printf("'\n");
    return offset + 2; 
}

static int dasmLConstant(const Chunk* chunk, int offset){
    uint32_t constantIndex = (uint32_t)chunk -> code[offset + 1] | 
                                      (chunk -> code[offset + 2] << 8) |
                                      (chunk -> code[offset + 3] << 16);
    printf("OP_LCONSTANT %d '", constantIndex);
    if(constantIndex < chunk -> constants.count){
        printf("%g", chunk -> constants.values[constantIndex]);
    }else{
        printf("Unknown long constant");
    }
    printf("'\n");
    return offset + 4; 
    // 1 byte for opcode + 1 byte for long constant index
}

static int getLine(const Chunk* chunk, int offset){
    int low = 0;
    int high = chunk -> lineCount - 1;
    int match = -1;
    while(low <= high){
        int mid = low + ((high - low) >> 1);
        if(chunk -> lines[mid * 2] <= offset){
            match = mid;
            low = mid + 1;
        }else{
            high = mid - 1;
        }
    }

    if(match != -1){
        return chunk -> lines[match * 2 + 1];
    }

    return -1;  // No matching line found
}