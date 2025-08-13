#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

Parser parser;
Chunk* curChunk;
VM* curVm;

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

typedef void (*ParseFunc)(VM* vm);
typedef struct{
    ParseFunc prefix;  // Function to handle prefix parsing
    ParseFunc infix;   // Function to handle infix parsing
    Precedence precedence;  // Precedence of the operator
}ParseRule;

static void handleLiteral(VM* vm);
static void handleGrouping(VM* vm);
static void handleUnary(VM* vm);
static void handleBinary(VM* vm);
static void handleNum(VM* vm);
static void handleString(VM* vm);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]              = {handleGrouping,  NULL,           PREC_NONE},
    [TOKEN_RIGHT_PAREN]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_LEFT_BRACE]              = {NULL,            NULL,           PREC_NONE},
    [TOKEN_RIGHT_BRACE]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_COMMA]                   = {NULL,            NULL,           PREC_NONE},

    [TOKEN_PLUS]                    = {NULL,            handleBinary,   PREC_TERM},
    [TOKEN_MINUS]                   = {handleUnary,     handleBinary,   PREC_TERM},
    [TOKEN_STAR]                    = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_SLASH]                   = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_NUMBER]                  = {handleNum,       NULL,           PREC_NONE},

    [TOKEN_STRING_START]            = {handleString,    NULL,           PREC_NONE},
    [TOKEN_STRING_END]              = {NULL,            NULL,           PREC_NONE},
    [TOKEN_INTERPOLATION_START]     = {NULL,            NULL,           PREC_NONE},
    [TOKEN_INTERPOLATION_END]       = {NULL,            NULL,           PREC_NONE},
    [TOKEN_INTERPOLATION_CONTENT]   = {NULL,            NULL,           PREC_NONE},

    [TOKEN_NULL]                    = {handleLiteral,   NULL,           PREC_NONE},
    [TOKEN_TRUE]                    = {handleLiteral,   NULL,           PREC_NONE},
    [TOKEN_FALSE]                   = {handleLiteral,   NULL,           PREC_NONE},

    [TOKEN_NOT]                     = {handleUnary,     NULL,           PREC_UNARY},
    [TOKEN_NOT_EQUAL]               = {NULL,            handleBinary,   PREC_EQUALITY},
    [TOKEN_EQUAL]                   = {NULL,            handleBinary,   PREC_EQUALITY},
    [TOKEN_GREATER]                 = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_LESS]                    = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]           = {NULL,            handleBinary,   PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]              = {NULL,            handleBinary,   PREC_COMPARISON},

    [TOKEN_EOF]                     = {NULL,            NULL,           PREC_NONE},
};

static inline ParseRule* getRule(TokenType type){return &rules[type];}
static void parsePrecedence(VM* vm, Precedence precedence);

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


bool compile(VM* vm, const char* code, Chunk* chunk){
    initScanner(code);
    curChunk = chunk;
    curVm = vm;
    parser.hadError = false;
    parser.panic = false;

    advance();  // Initialize the first token
    if(parser.cur.type == TOKEN_EOF){
        return true;  // No code to compile
    }
    expression(vm); // Start parsing the expression

    consume(TOKEN_EOF, "Expected end of file");
    stopCompiler(vm);
    return !parser.hadError;
}

static void expression(VM* vm){
    parsePrecedence(vm, PREC_ASSIGNMENT);
}

static void parsePrecedence(VM* vm, Precedence precedence){
    advance();
    ParseFunc preRule = getRule(parser.pre.type)->prefix;
    if(preRule == NULL){
        errorAt(&parser.pre, "Expect expression");
        return;
    }
    preRule(vm);
    while(precedence <= getRule(parser.cur.type)->precedence){
        advance();
        ParseFunc inRule = getRule(parser.pre.type)->infix;
        inRule(vm);
    }
}

static void emitByte(uint8_t byte){
    writeChunk(getCurChunk(), byte, parser.pre.line);
}

static void emitPair(uint8_t byte1, uint8_t byte2){
    emitByte(byte1);
    emitByte(byte2);
}

static void stopCompiler(VM* vm){
    emitByte(OP_RETURN);

    #ifdef DEBUG_PRINT_CODE
    if(!parser.hadError){
        printf("== Compiled code ==\n");
        dasmChunk(getCurChunk(), "code");
    }
    #endif
}

static void handleNum(VM* vm){
    double value = strtod(parser.pre.head, NULL);
    emitConstant(vm, NUM_VAL(value));
}

static void emitConstant(VM* vm, Value value){
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

static void handleGrouping(VM* vm){
    expression(vm);
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void handleUnary(VM* vm){
    TokenType type = parser.pre.type;
    parsePrecedence(vm, PREC_UNARY);

    switch(type){
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_NOT: emitByte(OP_NOT); break;
        default: return;
    }
}

static void handleBinary(VM* vm){
    TokenType type = parser.pre.type;
    ParseRule* rule = getRule(type);
    parsePrecedence(vm, (Precedence)(rule->precedence + 1));  // parse the right-hand side, parse only if precedence is higher
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

static void handleLiteral(VM* vm){
    (void)vm;
    TokenType type = parser.pre.type;
    switch(type){
        case TOKEN_NULL: emitByte(OP_NULL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        default: return;  // Should not reach here
    }
}

static void handleString(VM* vm){
    int partCnt = 0;

    while(parser.cur.type != TOKEN_STRING_END){
            if(parser.cur.type == TOKEN_INTERPOLATION_CONTENT){
                Token* token = &parser.cur;

                char* unescaped_chars = malloc(token->len + 1);
                if(unescaped_chars == NULL){
                    errorAt(token, "Memory Error while processing string.");
                    advance();
                    continue;
                }

                int unescaped_len = 0;
                for(int i = 0; i < token->len; i++){
                    if(token->head[i] == '\\' && i + 1 < token->len){
                        i++;
                        switch(token->head[i]){
                            case 'n': unescaped_chars[unescaped_len++] = '\n'; break;
                            case 't': unescaped_chars[unescaped_len++] = '\t'; break;
                            case '\\': unescaped_chars[unescaped_len++] = '\\'; break;
                            case '"': unescaped_chars[unescaped_len++] = '"'; break;
                            case '$': unescaped_chars[unescaped_len++] = '$'; break;
                            default: unescaped_chars[unescaped_len++] = token->head[i]; break;
                        }
                    }else{
                        unescaped_chars[unescaped_len++] = token->head[i];
                    }
                }

                ObjectString* str = copyString(vm, token->head, token->len);
                free(unescaped_chars);
                emitConstant(vm, OBJECT_VAL(str));
                advance();
            }else{
                consume(TOKEN_INTERPOLATION_START, "Expect string or interpolation.");
                expression(vm);
                emitByte(OP_TO_STRING);
                consume(TOKEN_INTERPOLATION_END, "Expect '}' after expression.");
            }
            partCnt++;
    }

    consume(TOKEN_STRING_END, "Unterminated string.");

    if(partCnt == 0){
        emitConstant(vm, OBJECT_VAL(copyString(vm, "", 0)));
    }else{
        for(int i = 1; i < partCnt; i++){
            emitByte(OP_ADD);
        }
    }
}

static void consume(TokenType type, const char* errMsg){
    if(parser.cur.type == type){
        advance();
        return;
    }
    errorAt(&parser.cur, errMsg);
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