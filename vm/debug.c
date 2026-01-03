#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "instruction.h"

static const char* opNames[] = {
    "MOVE",
    "LOADK",
    "LOADBOOL",
    "LOADNULL",
    "GET_GLOBAL",
    "SET_GLOBAL",
    "GET_UPVAL",
    "SET_UPVAL",
    "GET_TABLE",
    "SET_TABLE",
    "GET_FIELD",
    "SET_FIELD",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "NOT",
    "NEG",
    "EQ",
    "LT",
    "LE",
    "JMP",
    "CALL",
    "TAILCALL",
    "RETURN",
    "CLOSURE",
    "CLASS",
    "METHOD",
    "NEW_LIST",
    "NEW_MAP",
    "INIT_LIST",    
    "IMPORT",
    "FOREACH",
    "PRINT",
};

void dasmChunk(Chunk* chunk, const char* name){
    printf("== %s ==\n", name);
    for(size_t offset = 0; offset < chunk->count; ){
        offset = disassembleInstruction(chunk, offset);
    }
}

static void dasmABC(const char* name, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int b = GET_ARG_B(instruction);
    int c = GET_ARG_C(instruction);
    printf("%-16s %4d %4d %4d\n", name, a, b, c);
}

static void dasmABx(const char* name, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int bx = GET_ARG_Bx(instruction);
    printf("%-16s %4d %4d\n", name, a, bx);
}

static void dasmAsBx(const char* name, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int sbx = GET_ARG_sBx(instruction);
    printf("%-16s %4d %4d\n", name, a, sbx);
}

static void dasmLoadK(const char* name, const Chunk* chunk, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int bx = GET_ARG_Bx(instruction);
    Value constant = chunk->constants.values[bx];
    printf("%-16s %4d %4d '", name, a, bx);
    printValue(constant);
}

static dasmGlobal(const char* name, Chunk* chunk, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int bx = GET_ARG_Bx(instruction);
    Value constant = chunk->constants.values[bx];
    printf("%-16s %4d %4d '", name, a, bx);
    printValue(constant);
}

static void dasmField(const char* name, const Chunk* chunk, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int b = GET_ARG_B(instruction);
    int c = GET_ARG_C(instruction);
    Value constant = chunk->constants.values[c];
    printf("%-16s %4d %4d %4d '", name, a, b, c);
    printValue(constant);
}

int dasmInstruction(const Chunk* chunk, int offset){
    print("offset: %04d ", offset);
    int line = chunk->lines[offset];
    if(offset > 0 && line == chunk->lines[offset - 1]){
        printf("\t| ");
    }else{
        printf("%4d ", line);
    }

    Instruction instruction = chunk->code[offset];
    OpCode op = GET_OPCODE(instruction);

    int opCnt = sizeof(opNames) / sizeof(opNames[0]);
    const char* opName = (op < opCnt) ? opNames[op] : "UNKNOWN";

    switch(op){
        // iABC
        case OP_MOVE:
        case OP_LOADBOOL:
        case OP_LOADNULL:
        case OP_GET_UPVAL:
        case OP_SET_UPVAL:
        case OP_GET_TABLE:
        case OP_SET_TABLE:

        case OP_ADD:
        case OP_SUB:
        case OP_MUL:
        case OP_DIV:
        case OP_MOD: 

        case OP_EQ: 
        case OP_LT: 
        case OP_LE:

        case OP_CALL: 
        case OP_TAILCALL: 
        case OP_RETURN:

        case OP_METHOD:

        case OP_NEW_LIST: 
        case OP_NEW_MAP: 
        case OP_INIT_LIST:
            dasmABC(opName, instruction);
            break;

        // iAB(C==0)
        case OP_NEG:
        case OP_NOT:
            dasmABC(opName, instruction);
            break;

        // iABx
        case OP_CLOSURE:
        case OP_IMPORT:
        case OP_CLASS:
            dasmABx(opName, instruction);
            break;
        
        // iABx (with constant & global)
        case OP_LOADK:
            dasmLoadK(opName, chunk, instruction);
            break;

        case OP_GET_GLOBAL:
        case OP_SET_GLOBAL:
            dasmGlobal(opName, chunk, instruction);
            break;

        case OP_GET_FIELD:
        case OP_SET_FIELD:
            dasmField(opName, chunk, instruction);
            break;

        // iAsBx
        case OP_JMP:
        case OP_FOREACH:
            dasmAsBx(opName, instruction);
            break;

        default:
            printf("Unknown opcode %d\n", op);
            break;
    }

    printf("\n");
    return offset + 1;
}
