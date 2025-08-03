#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

Parser parser;
Chunk* curChunk;

typedef enum{
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! - (unary minus)
    PREC_CALL,       // . () []
    PREC_PRIMARY     // identifiers, literals, grouping
}Precedence;

typedef void (*ParseFunc)(void);
typedef struct{
    ParseFunc prefix;  // Function to handle prefix parsing
    ParseFunc infix;   // Function to handle infix parsing
    Precedence precedence;  // Precedence of the operator
}ParseRule;

static void handleLiteral(void);
static void handleGrouping(void);
static void handleUnary(void);
static void handleBinary(void);
static void handleNum(void);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = {handleGrouping,  NULL,           PREC_NONE},
    [TOKEN_RIGHT_PAREN]     = {NULL,            NULL,           PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,            NULL,           PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,            NULL,           PREC_NONE},
    [TOKEN_COMMA]           = {NULL,            NULL,           PREC_NONE},
    [TOKEN_PLUS]            = {NULL,            handleBinary,   PREC_TERM},
    [TOKEN_MINUS]           = {handleUnary,     handleBinary,   PREC_TERM},
    [TOKEN_STAR]            = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_SLASH]           = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_NUMBER]          = {handleNum,       NULL,           PREC_NONE},
    [TOKEN_NULL]            = {handleLiteral,   NULL,           PREC_NONE},
    [TOKEN_TRUE]            = {handleLiteral,   NULL,           PREC_NONE},
    [TOKEN_FALSE]           = {handleLiteral,   NULL,           PREC_NONE},
    [TOKEN_NOT]             = {handleUnary,     NULL,           PREC_UNARY},
    [TOKEN_NOT_EQUAL]       = {NULL,            handleBinary,   PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,            handleBinary,   PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_EOF]             = {NULL,            NULL,           PREC_NONE},
};

static inline ParseRule* getRule(TokenType type){return &rules[type];}
static void parsePrecedence(Precedence precedence);

static void advance(){
    parser.pre = parser.cur;
    while(true){
        parser.cur = scan();
        if(parser.cur.type != TOKEN_ERROR){
            break;
        }
        errorAt(&parser.cur, "Unexpected token: %.*s");
    }
}


bool compile(const char* code, Chunk* chunk){
    initScanner(code);
    curChunk = chunk;
    parser.hadError = false;
    parser.panic = false;

    advance();  // Initialize the first token
    if(parser.cur.type == TOKEN_EOF){
        return true;  // No code to compile
    }
    expression(); // Start parsing the expression

    consume(TOKEN_EOF, "Expected end of file");
    stopCompiler();
    return !parser.hadError;
}

static void expression(){
    parsePrecedence(PREC_ASSIGNMENT);
}

static void parsePrecedence(Precedence precedence){
    advance();
    ParseFunc preRule = getRule(parser.pre.type) -> prefix;
    if(preRule == NULL){
        errorAt(&parser.pre, "Expect expression");
        return;
    }
    preRule();
    while(precedence <= getRule(parser.cur.type) -> precedence){
        advance();
        ParseFunc inRule = getRule(parser.pre.type) -> infix;
        inRule();
    }
}

static void emitByte(uint8_t byte){
    writeChunk(getCurChunk(), byte, parser.pre.line);
}

static void emitPair(uint8_t byte1, uint8_t byte2){
    emitByte(byte1);
    emitByte(byte2);
}

static void stopCompiler(){
    emitByte(OP_RETURN);

    #ifdef DEBUG_PRINT_CODE
    if(!parser.hadError){
        printf("== Compiled code ==\n");
        dasmChunk(getCurChunk(), "code");
    }
    #endif
}

static void handleNum(){
    double value = strtod(parser.pre.head, NULL);
    emitConstant(NUM_VAL(value));
}

static void emitConstant(Value value){
    int constantIndex = addConstant(getCurChunk(), value);
    if(constantIndex < 0){
        fprintf(stderr, "Failed to add constant\n");
        return;
    }else if(constantIndex <= 0xff){
        emitPair(OP_CONSTANT, (uint8_t)constantIndex);
        return;
    }else if(constantIndex <= 0xffffff){
        emitByte(OP_LCONSTANT);
        emitByte((uint8_t)(constantIndex & 0xff));
        emitByte((uint8_t)((constantIndex >> 8) & 0xff));
        emitByte((uint8_t)((constantIndex >> 16) & 0xff));
        return;
    }else{
        fprintf(stderr, "Constant index out of range: %d\n", constantIndex);
        return;
    }
}

static void handleGrouping(){
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void handleUnary(){
    TokenType type = parser.pre.type;
    parsePrecedence(PREC_UNARY);

    switch(type){
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_NOT: emitByte(OP_NOT); break;
        default: return;
    }
}

static void handleBinary(){
    TokenType type = parser.pre.type;
    ParseRule* rule = getRule(type);
    parsePrecedence((Precedence)(rule -> precedence + 1));  // parse the right-hand side, parse only if precedence is higher
    switch(type){
        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;
        case TOKEN_EQUAL:           emitByte(OP_EQUAL); break;
        case TOKEN_NOT_EQUAL:       emitByte(OP_NOT_EQUAL); break;
        case TOKEN_GREATER:         emitByte(OP_GREATER); break;
        case TOKEN_LESS:            emitByte(OP_LESS); break;
        case TOKEN_GREATER_EQUAL:   emitByte(OP_GREATER_EQUAL); break;
        case TOKEN_LESS_EQUAL:      emitByte(OP_LESS_EQUAL); break;
        default: return;
    }
}

static void handleLiteral(){
    TokenType type = parser.pre.type;
    switch(type){
        case TOKEN_NULL: emitByte(OP_NULL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        default: return;  // Should not reach here
    }
}

static void consume(TokenType type, const char* message){
    if(parser.cur.type == type){
        advance();
        return;
    }
    errorAt(&parser.cur, message);
}

static Chunk* getCurChunk(){
    if(curChunk == NULL){
        fprintf(stderr, "No current chunk available\n");
        return NULL;
    }
    return curChunk;
}

static void errorAt(Token* token, const char* message){
    fprintf(stderr, "Error [line %d] ", parser.cur.line);
    if(token->type != TOKEN_EOF){
        fprintf(stderr, "at '%.*s': ", token->len, token->head);
    }else{
        fprintf(stderr, "at end: ");
    }
    fprintf(stderr, message);
    fprintf(stderr, "\n");
    parser.hadError = true;
}