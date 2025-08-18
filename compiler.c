#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

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

typedef void (*ParseFunc)(Compiler* compiler, bool canAssign);
typedef struct{
    ParseFunc prefix;  // Function to handle prefix parsing
    ParseFunc infix;   // Function to handle infix parsing
    Precedence precedence;  // Precedence of the operator
}ParseRule;

static void handleLiteral(Compiler* compiler, bool canAssign);
static void handleGrouping(Compiler* compiler, bool canAssign);
static void handleUnary(Compiler* compiler, bool canAssign);
static void handleBinary(Compiler* compiler, bool canAssign);
static void handleNum(Compiler* compiler, bool canAssign);
static void handleString(Compiler* compiler, bool canAssign);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]              = {handleGrouping,  NULL,           PREC_NONE},
    [TOKEN_RIGHT_PAREN]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_LEFT_BRACE]              = {NULL,            NULL,           PREC_NONE},
    [TOKEN_RIGHT_BRACE]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_COMMA]                   = {NULL,            NULL,           PREC_NONE},
    [TOKEN_SEMICOLON]               = {NULL,            NULL,           PREC_NONE},

    [TOKEN_PLUS]                    = {NULL,            handleBinary,   PREC_TERM},
    [TOKEN_MINUS]                   = {handleUnary,     handleBinary,   PREC_TERM},
    [TOKEN_STAR]                    = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_SLASH]                   = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_NUMBER]                  = {handleNum,       NULL,           PREC_NONE},
    [TOKEN_IDENTIFIER]              = {handleVar,       NULL,           PREC_NONE},

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
static void parsePrecedence(Compiler* compiler, Precedence precedence);
static int parseVar(Compiler* compiler, const char* err);

static void advance(Compiler* compiler){
    compiler->parser.pre = compiler->parser.cur;
    while(true){
        compiler->parser.cur = scan();
        if(compiler->parser.cur.type != TOKEN_ERROR){
            break;
        }
        errorAt(compiler, &compiler->parser.cur, "Unexpected token: %.*s");
    }
}


bool compile(VM* vm, const char* code, Chunk* chunk){
    Compiler compiler;
    initScanner(code);
    compiler.chunk = chunk;
    compiler.vm = vm;
    compiler.parser.hadError = false;
    compiler.parser.panic = false;

    advance(&compiler);  // Initialize the first token
    if(compiler.parser.cur.type == TOKEN_EOF){
        return true;  // No code to compile
    }
    // expression(&compiler); // Start parsing the expression

    while(!match(&compiler, TOKEN_EOF)){
        decl(&compiler);
    }
    consume(&compiler, TOKEN_EOF, "Expected end of file");
    stopCompiler(&compiler);
    return !compiler.parser.hadError;
}

static void expression(Compiler* compiler){
    parsePrecedence(compiler, PREC_ASSIGNMENT);
}

static void decl(Compiler* compiler){
    if(match(compiler, TOKEN_VAR)){
        varDecl(compiler);
    }else{
        stmt(compiler);
    }
    if(compiler->parser.panic){
        sync(compiler);
    }
}

static void varDecl(Compiler* compiler){
    int global = parseVar(compiler, "Expect variable name.");

    if(match(compiler, TOKEN_ASSIGN)){
        expression(compiler);
    }else{
        emitByte(compiler, OP_NULL);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after declaration.");

    defineVar(compiler, global);
}

static void stmt(Compiler* compiler){
    if(match(compiler, TOKEN_PRINT)){
        printStmt(compiler);
    }else{
        expressionStmt(compiler);
    }
}

static void expressionStmt(Compiler* compiler){
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(compiler, OP_POP);  // drop result
}

static bool checkType(Compiler* compiler, TokenType type){
    return compiler->parser.cur.type == type;
}

static bool match(Compiler* compiler, TokenType type){
    if(!checkType(compiler, type)){
        return false;
    }
    advance(compiler);
    return true;
}

static void sync(Compiler* compiler){
    compiler->parser.panic = false;
    while(compiler->parser.cur.type != TOKEN_EOF){
        if(compiler->parser.pre.type == TOKEN_SEMICOLON){
            return;
        }
        switch(compiler->parser.cur.type){
            case TOKEN_CLASS:
            case TOKEN_FUNC:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ;
        }
        advance(compiler);
    }
}

static void printStmt(Compiler* compiler){
    expression(compiler);
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after a expression");
    emitByte(compiler, OP_PRINT);
}

static void parsePrecedence(Compiler* compiler, Precedence precedence){
    advance(compiler);
    ParseFunc preRule = getRule(compiler->parser.pre.type)->prefix;
    if(preRule == NULL){
        errorAt(compiler, &compiler->parser.pre, "Expect expression");
        return;
    }
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    preRule(compiler, canAssign);
    while(precedence <= getRule(compiler->parser.cur.type)->precedence){
        advance(compiler);
        ParseFunc inRule = getRule(compiler->parser.pre.type)->infix;
        inRule(compiler, canAssign);
    }

    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        errorAt(compiler, &compiler->parser.pre, "Invalid assignment.");
    }
}

static int identifierConst(Compiler* compiler){
    Token* name = &compiler->parser.pre;
    Value strVal = OBJECT_VAL(copyString(compiler->vm, name->head, name->len));
    addConstant(compiler->chunk, strVal);
}

static int parseVar(Compiler* compiler, const char* err){
    consume(compiler, TOKEN_IDENTIFIER, err);
    return identifierConst(compiler);
}

static void defineVar(Compiler* compiler, int global){
    if(global <= 0xff){
        emitByte(compiler, OP_DEFINE_GLOBAL);
        emitByte(compiler, (uint8_t)global);
    }else if(global <= 0xffffff){
        emitByte(compiler, OP_DEFINE_GLOBAL);
        emitByte(compiler, (uint8_t)(global & 0xff));
        emitByte(compiler, (uint8_t)((global >> 8) & 0xff));
        emitByte(compiler, (uint8_t)((global >> 16) & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many variables declared");
    }
}

static void emitByte(Compiler* compiler, uint8_t byte){
    writeChunk(compiler->chunk, byte, compiler->parser.pre.line);
}

static void emitPair(Compiler* compiler, uint8_t byte1, uint8_t byte2){
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static void stopCompiler(Compiler* compiler){
    emitByte(compiler, OP_RETURN);

    #ifdef DEBUG_PRINT_CODE
    if(!compiler->parser.hadError){
        printf("== Compiled code ==\n");
        dasmChunk(compiler->chunk, "code");
    }
    #endif
}

static void handleNum(Compiler* compiler, bool canAssign){
    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        errorAt(compiler, &compiler->parser.pre, "Invalid assignment target.");
        return;
    }
    double value = strtod(compiler->parser.pre.head, NULL);
    emitConstant(compiler, NUM_VAL(value));
}

static void emitConstant(Compiler* compiler, Value value){
    int constantIndex = addConstant(compiler->chunk, value);
    if(constantIndex < 0){
        fprintf(stderr, "Failed to add constant\n");
        return;
    }else if(constantIndex <= 0xff){
        emitPair(compiler, OP_CONSTANT, (uint8_t)constantIndex);
        return;
    }else if(constantIndex <= 0xffffff){
        emitByte(compiler, OP_LCONSTANT);
        emitByte(compiler, (uint8_t)(constantIndex & 0xff));
        emitByte(compiler, (uint8_t)((constantIndex >> 8) & 0xff));
        emitByte(compiler, (uint8_t)((constantIndex >> 16) & 0xff));
        return;
    }else{
        fprintf(stderr, "Constant index out of range: %d\n", constantIndex);
        return;
    }
}

static void handleVar(Compiler* compiler, bool canAssign){
    int index = identifierConst(compiler);
    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        expression(compiler);
        if(index <= 0xff){
            emitByte(compiler, OP_SET_GLOBAL);
            emitByte(compiler, (uint8_t)index);
        }else if(index <= 0xffffff){
            emitByte(compiler, OP_SET_LGLOBAL);
            emitByte(compiler, (uint8_t)(index & 0xff));
            emitByte(compiler, (uint8_t)((index >> 8) & 0xff));
            emitByte(compiler, (uint8_t)((index >> 16) & 0xff));
        }else{
            errorAt(compiler, &compiler->parser.pre, "Too many variables.");
        }
    }else{
        if(index <= 0xff){
            emitByte(compiler, OP_GET_GLOBAL);
            emitByte(compiler, (uint8_t)index);
        }else if(index <= 0xffffff){
            emitByte(compiler, OP_GET_LGLOBAL);
            emitByte(compiler, (uint8_t)(index & 0xff));
            emitByte(compiler, (uint8_t)((index >> 8) & 0xff));
            emitByte(compiler, (uint8_t)((index >> 16) & 0xff));
        }else{
            errorAt(compiler, &compiler->parser.pre, "Too many variables");
        }
    }
}

static void handleGrouping(Compiler* compiler, bool canAssign){
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void handleUnary(Compiler* compiler, bool canAssign){
    TokenType type = compiler->parser.pre.type;
    parsePrecedence(compiler, PREC_UNARY);

    switch(type){
        case TOKEN_MINUS: emitByte(compiler, OP_NEGATE); break;
        case TOKEN_NOT: emitByte(compiler, OP_NOT); break;
        default: return;
    }
}

static void handleBinary(Compiler* compiler, bool canAssign){
    TokenType type = compiler->parser.pre.type;
    ParseRule* rule = getRule(type);
    parsePrecedence(compiler, (Precedence)(rule->precedence + 1));  // parse the right-hand side, parse only if precedence is higher
    switch(type){
        case TOKEN_PLUS:            emitByte(compiler, OP_ADD); break;
        case TOKEN_MINUS:           emitByte(compiler, OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(compiler, OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(compiler, OP_DIVIDE); break;
        case TOKEN_EQUAL:           emitByte(compiler, OP_EQUAL); break;
        case TOKEN_NOT_EQUAL:       emitByte(compiler, OP_NOT_EQUAL); break;
        case TOKEN_GREATER:         emitByte(compiler, OP_GREATER); break;
        case TOKEN_LESS:            emitByte(compiler, OP_LESS); break;
        case TOKEN_GREATER_EQUAL:   emitByte(compiler, OP_GREATER_EQUAL); break;
        case TOKEN_LESS_EQUAL:      emitByte(compiler, OP_LESS_EQUAL); break;
        default: return;
    }
}

static void handleLiteral(Compiler* compiler, bool canAssign){
    TokenType type = compiler->parser.pre.type;
    switch(type){
        case TOKEN_NULL: emitByte(compiler, OP_NULL); break;
        case TOKEN_TRUE: emitByte(compiler, OP_TRUE); break;
        case TOKEN_FALSE: emitByte(compiler, OP_FALSE); break;
        default: return;  // Should not reach here
    }
}

static void handleString(Compiler* compiler, bool canAssign){
    int partCnt = 0;

    while(compiler->parser.cur.type != TOKEN_STRING_END && compiler->parser.cur.type != TOKEN_EOF){
            if(compiler->parser.cur.type == TOKEN_INTERPOLATION_CONTENT){
                Token* token = &compiler->parser.cur;

                char* unescaped_chars = malloc(token->len + 1);
                if(unescaped_chars == NULL){
                    errorAt(compiler, token, "Memory Error while processing string.");
                    advance(compiler);
                    continue;
                }

                int unescaped_len = 0;
                for(int i = 0; i < token->len; i++){
                    if(token->head[i] == '\\' && i + 1 < token->len){
                        i++;
                        switch(token->head[i]){
                            case 'a': unescaped_chars[unescaped_len++] = '\a'; break;
                            case 'b': unescaped_chars[unescaped_len++] = '\b'; break;
                            case 'f': unescaped_chars[unescaped_len++] = '\f'; break;
                            case 'r': unescaped_chars[unescaped_len++] = '\r'; break;
                            case 'n': unescaped_chars[unescaped_len++] = '\n'; break;
                            case 'v': unescaped_chars[unescaped_len++] = '\v'; break;
                            case 't': unescaped_chars[unescaped_len++] = '\t'; break;
                            case '\\': unescaped_chars[unescaped_len++] = '\\'; break;
                            case '"': unescaped_chars[unescaped_len++] = '"'; break;
                            case '$': unescaped_chars[unescaped_len++] = '$'; break;
                            case '0': unescaped_chars[unescaped_len++] = '\0'; break;
                            default: {
                                char err_msg[30];
                                snprintf(err_msg, sizeof(err_msg), "Invalid escape character '\\%c'.", token->head[i]);
                                errorAt(compiler, token, err_msg);
                                unescaped_chars[unescaped_len++] = token->head[i]; 
                                break;
                            }
                        }
                    }else{
                        unescaped_chars[unescaped_len++] = token->head[i];
                    }
                }

                ObjectString* str = copyString(compiler->vm, unescaped_chars, unescaped_len);
                free(unescaped_chars);
                emitConstant(compiler, OBJECT_VAL(str));
                advance(compiler);
            }else{
                consume(compiler, TOKEN_INTERPOLATION_START, "Expect string or interpolation.");
                expression(compiler);
                emitByte(compiler, OP_TO_STRING);
                consume(compiler, TOKEN_INTERPOLATION_END, "Expect '}' after expression.");
            }
            partCnt++;
    }

    consume(compiler, TOKEN_STRING_END, "Unterminated string.");

    if(partCnt == 0){
        emitConstant(compiler, OBJECT_VAL(copyString(compiler->vm, "", 0)));
    }else{
        for(int i = 1; i < partCnt; i++){
            emitByte(compiler, OP_ADD);
        }
    }
}

static void consume(Compiler* compiler, TokenType type, const char* errMsg){
    if(compiler->parser.cur.type == type){
        advance(compiler);
        return;
    }
    errorAt(compiler, &compiler->parser.cur, errMsg);
}

static void errorAt(Compiler* compiler, Token* token, const char* message){
    fprintf(stderr, "Error [line %d] ", compiler->parser.cur.line);
    if(token->type != TOKEN_EOF){
        fprintf(stderr, "at '%.*s': ", token->len, token->head);
    }else{
        fprintf(stderr, "at end: ");
    }
    fprintf(stderr, "%s", message);
    fprintf(stderr, "\n");
    compiler->parser.hadError = true;
}