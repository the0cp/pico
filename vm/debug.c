#include <stdio.h>

#include "debug.h"
#include "value.h"
#include "instruction.h"

#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_GRAY    "\033[90m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_CYAN    "\033[36m"
#define CLR_MAGENTA "\033[35m"

static const char* opNames[] = {
    "OP_MOVE",        // R[A] <= R[B]
    "OP_LOADK",       // R[A] <= K[Bx]
    "OP_LOADBOOL",    // R[A] <= (B != 0)
    "OP_LOADNULL",    // R[A], R[A+1], ..., R[A+B-1] <= null

    "OP_GET_GLOBAL", // R[A] <= Gbl[K[Bx]]
    "OP_SET_GLOBAL", // Gbl[K[Bx]] <= R[A]

    "OP_GET_UPVAL",  // R[A] <= Upv[B]
    "OP_SET_UPVAL",  // Upv[B] <= R[A]
    "OP_CLOSE_UPVAL",

    "OP_GET_INDEX", // R[A] <= R[B][R[C]]
    "OP_SET_INDEX", // R[A][R[B]] <= R[C]

    "OP_GET_PROPERTY", // R[A] <= R[B].K[C]
    "OP_SET_PROPERTY", // R[A].K[B] <= R[C]

    "OP_FIELD",

    "OP_ADD", 
    "OP_SUB", 
    "OP_MUL", 
    "OP_DIV", 
    "OP_MOD",

    "OP_NOT", 
    "OP_NEG",

    "OP_EQ", 
    "OP_LT", 
    "OP_LE",

    "OP_JMP",
    "OP_JMP_IF_FALSE",  // R[A] is condition
    "OP_JMP_IF_TRUE",   // R[A] is condition
    "OP_CALL",
    "OP_TAILCALL",
    "OP_DEFER",
    "OP_SYSTEM",
    "OP_RETURN",

    "OP_CLOSURE",

    "OP_CLASS",
    "OP_METHOD",

    "OP_BUILD_LIST",
    "OP_BUILD_MAP",
    "OP_INIT_LIST",
    "OP_FILL_LIST",
    "OP_SLICE",
    "OP_TO_STRING",

    "OP_IMPORT",

    "OP_FOREACH",

    "OP_PRINT",
};

int getLine(const Chunk* chunk, int offset){
    if(offset < 0 || offset >= chunk->count) return -1;
    return chunk->lines[offset];
}

void dasmChunk(Chunk* chunk, const char* name){
    printf("== %s ==\n", name);
    for(size_t offset = 0; offset < chunk->count; offset++){
        dasmInstruction(chunk, offset);
    }
}

static void dasmABC(const char* name, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int b = GET_ARG_B(instruction);
    int c = GET_ARG_C(instruction);
    printf(
        CLR_CYAN "%-16s" CLR_GRAY " | " \
        CLR_MAGENTA "%-5s" CLR_GRAY " | " \
        CLR_YELLOW "%4d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_RESET, name, "iABC", a, b, c
    );
}

static void dasmABx(const char* name, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int bx = GET_ARG_Bx(instruction);
    printf(
        CLR_CYAN "%-16s" CLR_GRAY " | " \
        CLR_MAGENTA "%-5s" CLR_GRAY " | " \
        CLR_YELLOW "%4d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_GRAY "%5s" CLR_GRAY " | " \
        CLR_RESET, name, "iABx", a, bx, "-"
    );
}

static void dasmAsBx(const char* name, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int sbx = GET_ARG_sBx(instruction);
    printf(
        CLR_CYAN "%-16s" CLR_GRAY " | " \
        CLR_MAGENTA "%-5s" CLR_GRAY " | " \
        CLR_YELLOW "%4d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_GRAY "%5s" CLR_GRAY " | " \
        CLR_RESET, name, "iAsBx", a, sbx, "-"
    );
}

static void dasmLoadK(const char* name, const Chunk* chunk, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int bx = GET_ARG_Bx(instruction);
    Value constant = chunk->constants.values[bx];
    printf(
        CLR_CYAN "%-16s" CLR_GRAY " | " \
        CLR_MAGENTA "%-5s" CLR_GRAY " | " \
        CLR_YELLOW "%4d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_GRAY "%5s" CLR_GRAY " | " \
        CLR_GRAY "'", name, "iABx", a, bx, "-"
    );
    printValue(constant);
    printf(CLR_RESET);
}

static void dasmGlobal(const char* name, Chunk* chunk, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int bx = GET_ARG_Bx(instruction);
    Value constant = chunk->constants.values[bx];
    printf(
        CLR_CYAN "%-16s" CLR_GRAY " | " \
        CLR_MAGENTA "%-5s" CLR_GRAY " | " \
        CLR_YELLOW "%4d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_GRAY "%5s" CLR_GRAY " | " \
        CLR_GRAY "'", name, "iABx", a, bx, "-"
    );
    printValue(constant);
    printf(CLR_RESET);
}

static void dasmField(const char* name, const Chunk* chunk, Instruction instruction){
    int a = GET_ARG_A(instruction);
    int b = GET_ARG_B(instruction);
    int c = GET_ARG_C(instruction);
    Value constant = chunk->constants.values[c];
    printf(
        CLR_CYAN "%-16s" CLR_GRAY " | " \
        CLR_MAGENTA "%-5s" CLR_GRAY " | " \
        CLR_YELLOW "%4d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_YELLOW "%5d" CLR_GRAY " | " \
        CLR_GRAY "'", name, "iABC", a, b, c
    );
    printValue(constant);
    printf(CLR_RESET);
}

void dasmInstruction(Chunk* chunk, int offset){
    printf("offset: %04d ", offset);
    int line = chunk->lines[offset];

    if(offset > 0 && line == chunk->lines[offset - 1]){
        printf(CLR_GRAY "   ~ | " CLR_RESET);
    }else{
        printf(CLR_GREEN "%4d | " CLR_RESET, line);
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
        case OP_CLOSE_UPVAL:

        case OP_GET_INDEX:
        case OP_SET_INDEX:

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

        case OP_BUILD_LIST: 
        case OP_BUILD_MAP: 
        case OP_INIT_LIST:
        case OP_FILL_LIST:
        case OP_SLICE:
        case OP_TO_STRING:
        case OP_DEFER:
        case OP_SYSTEM:
        case OP_PRINT:
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

        case OP_FIELD:
        case OP_GET_PROPERTY:
        case OP_SET_PROPERTY:
            dasmField(opName, chunk, instruction);
            break;

        // iAsBx
        case OP_JMP:
        case OP_JMP_IF_FALSE:
        case OP_JMP_IF_TRUE:
        case OP_FOREACH:
            dasmAsBx(opName, instruction);
            break;

        default:
            printf(
                CLR_RED "%-16s" CLR_GRAY " | " \
                CLR_RED "%-5s" CLR_GRAY " | " \
                CLR_RED "%4s" CLR_GRAY " | " \
                CLR_RED "%5s" CLR_GRAY " | " \
                CLR_RED "%5s" CLR_GRAY " | " \
                CLR_RESET, "UNKNOWN", "?", "?", "?", "?"
            );
            break;
    }

    printf("\n");
}
