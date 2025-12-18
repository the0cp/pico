#ifndef PICO_COMPILER_H
#define PICO_COMPILER_H

#include "vm.h"
#include "scanner.h"
#include "common.h"

#define LOCAL_MAX (UINT16_MAX + 1)
#define LOOP_MAX 16
#define CASE_MAX 32

typedef struct{
    Token name;
    int depth;
}Local;

typedef struct{
    Token pre;  // Previous token
    Token cur;  // Current token
    bool hadError;  // Flag to indicate if there was a compilation error
    bool panic; // Flag to indicate if we are in panic mode
}Parser;

typedef struct{
    int start;
    int scopeDepth;
    int breakJump[LOOP_MAX];
    int breakCnt;
}Loop;

typedef struct{
    uint16_t index; // pointing to local or upvalue
    bool isLocal;   // T: local; F: upvalue
}Upvalue;

typedef struct Compiler{
    struct Compiler* enclosing;  // Enclosing compiler for nested functions
    Parser parser;
    VM* vm;
    Local locals[LOCAL_MAX];
    int localCnt;
    Upvalue upvalues[LOCAL_MAX];
    int upvalueCnt;
    int scopeDepth;
    Loop loops[LOOP_MAX];
    int loopCnt;
    FuncType type;
    ObjectFunc* func;
}Compiler;


ObjectFunc* compile(VM* vm, const char* code, const char* srcName);
void markCompilerRoots(VM* vm);
static ObjectFunc* stopCompiler(Compiler* compiler);
static void emitByte(Compiler* compiler, uint8_t byte);
static void emitPair(Compiler* compiler, uint8_t byte1, uint8_t byte2);
static int emitJump(Compiler* compiler, uint8_t instruction);
static void patchJump(Compiler* compiler, int offset);
static void emitLoop(Compiler* compiler, int loopStart);
static void consume(Compiler* compiler, TokenType type, const char* errMsg);
static void errorAt(Compiler* compiler, Token* token, const char* message);

static void handleNum(Compiler* compiler, bool canAssign);
static void emitConstant(Compiler* compiler, Value value);
static void handleVar(Compiler* compiler, bool canAssign);

static void handleGrouping(Compiler* compiler, bool canAssign);

static void handleUnary(Compiler* compiler, bool canAssign);
static void handleBinary(Compiler* compiler, bool canAssign);

static void handleLiteral(Compiler* compiler, bool canAssign);

static void handleString(Compiler* compiler, bool canAssign);

static void handleAnd(Compiler* compiler, bool canAssign);
static void handleOr(Compiler* compiler, bool canAssign);

static uint8_t argList(Compiler* compiler);
static void handleCall(Compiler* compiler, bool canAssign);

static void handleImport(Compiler* compiler, bool canAssign);
static void handleDot(Compiler* compiler, bool canAssign);

static void handleList(Compiler* compiler, bool canAssign);
static void handleIndex(Compiler* compiler, bool canAssign);

static void handleMap(Compiler* compiler, bool canAssign);

static void handleThis(Compiler* compiler, bool canAssign);

static void advance(Compiler* compiler);

static void expression(Compiler* compiler);

static void decl(Compiler* compiler);
static void varDecl(Compiler* compiler);
static void defineVar(Compiler* compiler, int global);
static void funcDecl(Compiler* compiler);
static void classDecl(Compiler* compiler);
static void methodDecl(Compiler* compiler);
static void importDecl(Compiler* compiler);

static void beginScope(Compiler* compiler);
static void block(Compiler* compiler);
static void endScope(Compiler* compiler);

static void stmt(Compiler* compiler);

static void expressionStmt(Compiler* compiler);

static bool checkType(Compiler* compiler, TokenType type);
static bool match(Compiler* compiler, TokenType type);
static void sync(Compiler* compiler);

static void printStmt(Compiler* compiler);

static void ifStmt(Compiler* compiler);

static void whileStmt(Compiler* compiler);
static void forStmt(Compiler* compiler);
static void breakStmt(Compiler* compiler);
static void continueStmt(Compiler* compiler);
static void switchStmt(Compiler* compiler);
static void systemStmt(Compiler* compiler);
static void deferStmt(Compiler* compiler);
static void returnStmt(Compiler* compiler);

#endif // PICO_COMPILER_H