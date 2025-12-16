#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "chunk.h"
#include "mem.h"

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
static void handleAnd(Compiler* compiler, bool canAssign);
static void handleOr(Compiler* compiler, bool canAssign);
static void handleCall(Compiler* compiler, bool canAssign);
static void handleImport(Compiler* compiler, bool canAssign);
static void handleDot(Compiler* compiler, bool canAssign);
static void handleList(Compiler* compiler, bool canAssign);
static void handleMap(Compiler* compiler, bool canAssign);
static void handleIndex(Compiler* compiler, bool canAssign);
static void handleThis(Compiler* compiler, bool canAssign);

static void addLocal(Compiler* compiler, Token name);
static int resolveLocal(Compiler* compiler, Token* name);
static int resolveUpvalue(Compiler* compiler, Token* name);
static int identifierConst(Compiler* compiler);

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]              = {handleGrouping,  handleCall,     PREC_CALL},
    [TOKEN_RIGHT_PAREN]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_LEFT_BRACE]              = {handleMap,       NULL,           PREC_NONE},
    [TOKEN_RIGHT_BRACE]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_LEFT_BRACKET]            = {handleList,      handleIndex,    PREC_CALL},
    [TOKEN_COMMA]                   = {NULL,            NULL,           PREC_NONE},
    [TOKEN_SEMICOLON]               = {NULL,            NULL,           PREC_NONE},

    [TOKEN_PLUS]                    = {NULL,            handleBinary,   PREC_TERM},
    [TOKEN_MINUS]                   = {handleUnary,     handleBinary,   PREC_TERM},
    [TOKEN_STAR]                    = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_SLASH]                   = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_PERCENT]                 = {NULL,            handleBinary,   PREC_FACTOR},
    [TOKEN_PLUS_EQUAL]              = {NULL,            NULL,           PREC_NONE},
    [TOKEN_MINUS_EQUAL]             = {NULL,            NULL,           PREC_NONE},
    [TOKEN_PLUS_PLUS]               = {handleUnary,     NULL,           PREC_NONE},
    [TOKEN_MINUS_MINUS]             = {handleUnary,     NULL,           PREC_NONE},

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

    [TOKEN_AND]                     = {NULL,            handleAnd,      PREC_AND},
    [TOKEN_OR]                      = {NULL,            handleOr,       PREC_OR},

    [TOKEN_IMPORT]                  = {handleImport,    NULL,           PREC_NONE},
    [TOKEN_DOT]                     = {NULL,            handleDot,      PREC_CALL},

    [TOKEN_THIS]                    = {handleThis,      NULL,           PREC_NONE},  

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

static void initCompiler(Compiler* compiler, VM* vm, Compiler* enclosing, FuncType type, ObjectString* srcName){
    compiler->enclosing = enclosing;
    compiler->vm = vm;
    compiler->type = type;
    compiler->func = newFunction(vm);
    compiler->func->srcName = srcName;
    compiler->func->type = type;

    if(type != TYPE_SCRIPT){
        compiler->func->name = copyString(vm, compiler->parser.pre.head, compiler->parser.pre.len);
    }

    compiler->upvalueCnt = 0;
    compiler->localCnt = 0;
    compiler->scopeDepth = 0;
    compiler->loopCnt = 0;
    compiler->parser.hadError = false;
    compiler->parser.panic = false;

    Local *local = &compiler->locals[compiler->localCnt++];
    local->depth = 0;
    if(type != TYPE_SCRIPT){
        local->name.head = compiler->parser.pre.head;
        local->name.len = compiler->parser.pre.len;
    }else{
        local->name.head = "";
        local->name.len = 0;
    }
}

ObjectFunc* compile(VM* vm, const char* code, const char* srcNameStr){
    Compiler* compiler = (Compiler*)reallocate(vm, NULL, 0, sizeof(Compiler));
    if(compiler == NULL){
        fprintf(stderr, "Not enough memory to compile.\n");
        return false;
    }
    initScanner(code);
    ObjectString* srcName = copyString(vm, srcNameStr, (int)strlen(srcNameStr));

    push(vm, OBJECT_VAL(srcName));
    Compiler* enclosing = vm->compiler;
    compiler->vm = vm;
    compiler->func = NULL;
    vm->compiler = compiler;
    initCompiler(compiler, vm, enclosing, TYPE_SCRIPT, srcName);
    pop(vm);    // pop srcName

    advance(compiler);  // Initialize the first token
    if(compiler->parser.cur.type == TOKEN_EOF){
        return NULL;  // No code to compile
    }
    // expression(&compiler); // Start parsing the expression

    while(!match(compiler, TOKEN_EOF)){
        decl(compiler);
    }
    consume(compiler, TOKEN_EOF, "Expected end of file");
    ObjectFunc* func = stopCompiler(compiler);

    bool hadError = compiler->parser.hadError;
    vm->compiler = compiler->enclosing;
    reallocate(vm, compiler, sizeof(Compiler), 0);
    
    return hadError ? NULL : func;
}

void markCompilerRoots(VM* vm){
    Compiler* compiler = vm->compiler;
    
    while(compiler != NULL){
        if(compiler->func != NULL){
            markObject(vm, (Object*)compiler->func); 
            markArray(vm, &compiler->func->chunk.constants);
        }
        compiler = compiler->enclosing;
    }
}

static void expression(Compiler* compiler){
    parsePrecedence(compiler, PREC_ASSIGNMENT);
}

static void decl(Compiler* compiler){
    if(match(compiler, TOKEN_VAR))
        varDecl(compiler);
    else if(match(compiler, TOKEN_FUNC))
        funcDecl(compiler);
    else if(match(compiler, TOKEN_CLASS))
        classDecl(compiler);
    else if(match(compiler, TOKEN_METHOD))
        methodDecl(compiler);
    else if(match(compiler, TOKEN_IMPORT))
        importDecl(compiler);
    else
        stmt(compiler);
    
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

static void compileFunc(Compiler* compiler, FuncType type){
    Compiler* funcCompiler = (Compiler*)reallocate(compiler->vm, NULL, 0, sizeof(Compiler));    
    if(funcCompiler == NULL){
        errorAt(compiler, &compiler->parser.pre, "Not enough memory to compile function.");
        return;
    }
    funcCompiler->parser = compiler->parser;

    funcCompiler->enclosing = compiler;
    funcCompiler->vm = compiler->vm;
    funcCompiler->func = NULL;  
    compiler->vm->compiler = funcCompiler;

    initCompiler(funcCompiler, compiler->vm, compiler, type, compiler->func->srcName);


    beginScope(funcCompiler);
    consume(funcCompiler, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if(!checkType(funcCompiler, TOKEN_RIGHT_PAREN)){
        do{
            funcCompiler->func->arity++;
            if(funcCompiler->func->arity > 255){
                errorAt(funcCompiler, &funcCompiler->parser.cur, "Too many function args.");
            }
            int constant = parseVar(funcCompiler, "Expect param name.");
            defineVar(funcCompiler, constant);
        }while(match(funcCompiler, TOKEN_COMMA));
    }
    consume(funcCompiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(funcCompiler, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block(funcCompiler);

    ObjectFunc* func = stopCompiler(funcCompiler);
    
    push(compiler->vm, OBJECT_VAL(func));
    int constIndex = addConstant(compiler->vm, &compiler->func->chunk, OBJECT_VAL(func));
    pop(compiler->vm);

    if(constIndex < 0){
        errorAt(compiler, &compiler->parser.pre, "Failed to add function constant.");
        compiler->parser = funcCompiler->parser;
        compiler->vm->compiler = compiler;
        reallocate(compiler->vm, funcCompiler, sizeof(Compiler), 0);
        return;
    }

    if(constIndex <= 0xff){
        emitPair(compiler, OP_CLOSURE, (uint8_t)constIndex);
    }else if(constIndex <= 0xffff){
        emitByte(compiler, OP_LCLOSURE);
        emitPair(compiler, (uint8_t)((constIndex >> 8) & 0xff), (uint8_t)(constIndex & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many constants (index exceeds 16-bit limit).");
    }
    
    for(int i = 0; i < func->upvalueCnt; i++){
        emitByte(compiler, funcCompiler->upvalues[i].isLocal ? 1 : 0); // 1-byte flag
        emitPair(compiler, (uint8_t)((funcCompiler->upvalues[i].index >> 8) & 0xff), // 2-byte index
                           (uint8_t)(funcCompiler->upvalues[i].index & 0xff));
    }

    compiler->parser = funcCompiler->parser;
    compiler->vm->compiler = compiler;
    reallocate(compiler->vm, funcCompiler, sizeof(Compiler), 0);
}

static void funcDecl(Compiler* compiler){
    int global = parseVar(compiler, "Expect function name.");
    compileFunc(compiler, TYPE_FUNC);
    
    defineVar(compiler, global);
}

static void compileMethod(Compiler* compiler, Token recvName, FuncType type){
    Compiler* methodCompiler = (Compiler*)reallocate(compiler->vm, NULL, 0, sizeof(Compiler));    
    if(methodCompiler == NULL){
        errorAt(compiler, &compiler->parser.pre, "Not enough memory to compile method.");
        return;
    }

    methodCompiler->parser = compiler->parser;
    methodCompiler->enclosing = compiler;
    methodCompiler->vm = compiler->vm;
    methodCompiler->func = NULL;  
    compiler->vm->compiler = methodCompiler;

    initCompiler(methodCompiler, compiler->vm, compiler, type, compiler->func->srcName);

    Local *local = &methodCompiler->locals[0]; 
    local->name = recvName; 
    local->depth = 0;

    beginScope(methodCompiler);
    consume(methodCompiler, TOKEN_LEFT_PAREN, "Expect '(' after method name.");
    if(!checkType(methodCompiler, TOKEN_RIGHT_PAREN)){
        do{
            methodCompiler->func->arity++;
            if(methodCompiler->func->arity > 255){
                errorAt(methodCompiler, &methodCompiler->parser.cur, "Too many method args.");
            }
            int constant = parseVar(methodCompiler, "Expect param name.");
            defineVar(methodCompiler, constant);
        }while(match(methodCompiler, TOKEN_COMMA));
    }

    consume(methodCompiler, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(methodCompiler, TOKEN_LEFT_BRACE, "Expect '{' before method body.");
    block(methodCompiler);

    ObjectFunc* func = stopCompiler(methodCompiler);

    push(compiler->vm, OBJECT_VAL(func));
    int constIndex = addConstant(compiler->vm, &compiler->func->chunk, OBJECT_VAL(func));
    pop(compiler->vm);

    if(constIndex < 0){
        errorAt(compiler, &compiler->parser.pre, "Failed to add method constant.");
        compiler->parser = methodCompiler->parser;
        compiler->vm->compiler = compiler;
        reallocate(compiler->vm, methodCompiler, sizeof(Compiler), 0);
        return;
    }

    if(constIndex <= 0xff){
        emitPair(compiler, OP_CLOSURE, (uint8_t)constIndex);
    }else if(constIndex <= 0xffff){
        emitByte(compiler, OP_LCLOSURE);
        emitPair(compiler, (uint8_t)((constIndex >> 8) & 0xff), (uint8_t)(constIndex & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many constants (index exceeds 16-bit limit).");
    }

    for(int i = 0; i < func->upvalueCnt; i++){
        emitByte(compiler, methodCompiler->upvalues[i].isLocal ? 1 : 0); // 1-byte flag
        emitPair(compiler, (uint8_t)((methodCompiler->upvalues[i].index >> 8) & 0xff), // 2-byte index
                           (uint8_t)(methodCompiler->upvalues[i].index & 0xff));
    }

    compiler->parser = methodCompiler->parser;
    compiler->vm->compiler = compiler;
    reallocate(compiler->vm, methodCompiler, sizeof(Compiler), 0);
}

static void classDecl(Compiler* compiler){
    consume(compiler, TOKEN_IDENTIFIER, "Expect class name.");
    Token className = compiler->parser.pre;

    Value nameValue = OBJECT_VAL(copyString(compiler->vm, className.head, className.len));
    push(compiler->vm, nameValue);
    uint16_t nameConst = addConstant(compiler->vm, &compiler->func->chunk, nameValue);
    pop(compiler->vm);
    if(nameConst < 0){
        errorAt(compiler, &compiler->parser.pre, "Failed to add class name constant.");
        return;
    }
    if(nameConst <= 0xff){
        emitPair(compiler, OP_CLASS, (uint8_t)nameConst);
    }else if(nameConst <= 0xffff){
        emitByte(compiler, OP_LCLASS);
        emitPair(compiler, (uint8_t)((nameConst >> 8) & 0xff), (uint8_t)(nameConst & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many constants.");
        return;
    }

    defineVar(compiler, nameConst);

    if(compiler->scopeDepth > 0){
        int index = compiler->localCnt - 1;
        if(index < 0){
            errorAt(compiler, &compiler->parser.pre, "No local variable to store class instance.");
            return;
        }
        if(index <= 0xff){
            emitPair(compiler, OP_GET_LOCAL, (uint8_t)index);
        }else if(index <= 0xffff){
            emitByte(compiler, OP_GET_LLOCAL);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }else{
            errorAt(compiler, &compiler->parser.pre, "Too many local variables.");
            return;
        }
    }else{
        if(nameConst <= 0xff){
            emitPair(compiler, OP_GET_GLOBAL, (uint8_t)nameConst);
        }else if(nameConst <= 0xffff){
            emitByte(compiler, OP_GET_LGLOBAL);
            emitPair(compiler, (uint8_t)((nameConst >> 8) & 0xff), (uint8_t)(nameConst & 0xff));
        }else{
            errorAt(compiler, &compiler->parser.pre, "Too many constants.");
            return;
        }
    }
    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before class body");
    while(!checkType(compiler, TOKEN_RIGHT_BRACE) && !checkType(compiler, TOKEN_EOF)){
        consume(compiler, TOKEN_IDENTIFIER, "Expect field name.");
        int fieldNameIndex = identifierConst(compiler);
        if(match(compiler, TOKEN_ASSIGN)){
            expression(compiler);
        }else{
            emitByte(compiler, OP_NULL);
        }
        
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after field declaration.");
        if(fieldNameIndex < 0){
            errorAt(compiler, &compiler->parser.pre, "Failed to add field name constant.");
            return;
        }
        if(fieldNameIndex <= 0xff){
            emitPair(compiler, OP_DEFINE_FIELD, (uint8_t)fieldNameIndex);
        }else if(fieldNameIndex <= 0xffff){
            emitByte(compiler, OP_DEFINE_LFIELD);
            emitPair(compiler, (uint8_t)((fieldNameIndex >> 8) & 0xff), (uint8_t)(fieldNameIndex & 0xff));
        }else{
            errorAt(compiler, &compiler->parser.pre, "Too many constants (field name).");
            return;
        }
    }
    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(compiler, OP_POP);
}

static void methodDecl(Compiler* compiler){
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after method.");
    consume(compiler, TOKEN_IDENTIFIER, "Expect receiver name.");

    Token recvName = compiler->parser.pre;

    consume(compiler, TOKEN_IDENTIFIER, "Expect receiver type.");
    Token className = compiler->parser.pre;
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after receiver.");

    uint8_t getOp, getLOp;
    int classIndex;
    if((classIndex = resolveLocal(compiler, &className)) != -1){
        getOp = OP_GET_LOCAL;
        getLOp = OP_GET_LLOCAL;
    }else if((classIndex = resolveUpvalue(compiler, &className)) != -1){
        getOp = OP_GET_UPVALUE;
        getLOp = OP_GET_LUPVALUE;
    }else{
        Value nameValue = OBJECT_VAL(copyString(compiler->vm, className.head, className.len));
        push(compiler->vm, nameValue);
        classIndex = addConstant(compiler->vm, &compiler->func->chunk, nameValue);
        pop(compiler->vm);
        getOp = OP_GET_GLOBAL;
        getLOp = OP_GET_LGLOBAL;
    }

    if(classIndex < 0){
        errorAt(compiler, &compiler->parser.pre, "Undefined class name.");
        return;
    }
    if(classIndex <= 0xff){
        emitPair(compiler, getOp, (uint8_t)classIndex);
    }else if(classIndex <= 0xffff){
        emitByte(compiler, getLOp);
        emitPair(compiler, (uint8_t)((classIndex >> 8) & 0xff), (uint8_t)(classIndex & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many constants.");
        return;
    }

    consume(compiler, TOKEN_IDENTIFIER, "Expect method name.");

    Token methodName = compiler->parser.pre;
    Value methodNameValue = OBJECT_VAL(copyString(compiler->vm, methodName.head, methodName.len));
    push(compiler->vm, methodNameValue);
    uint16_t methodNameConst = addConstant(compiler->vm, &compiler->func->chunk, methodNameValue);
    pop(compiler->vm);

    FuncType type = TYPE_METHOD;
    if(methodName.len == 4 && memcmp(methodName.head, "init", 4) == 0){
        type = TYPE_INITIALIZER;
    }
    compileMethod(compiler, recvName, type);
    if(methodNameConst < 0){
        errorAt(compiler, &compiler->parser.pre, "Failed to add method name constant.");
        return;
    }
    if(methodNameConst <= 0xff){
        emitPair(compiler, OP_METHOD, (uint8_t)methodNameConst);
    }else if(methodNameConst <= 0xffff){
        emitByte(compiler, OP_LMETHOD);
        emitPair(compiler, (uint8_t)((methodNameConst >> 8) & 0xff), (uint8_t)(methodNameConst & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many constants.");
        return;
    }
}

static void importDecl(Compiler* compiler){
    consume(compiler, TOKEN_STRING_START, "Expect module name string.");
    Token pathToken = compiler->parser.cur;
    Value valStr = OBJECT_VAL(copyString(compiler->vm, pathToken.head, pathToken.len));
    push(compiler->vm, valStr);
    int index = addConstant(compiler->vm, &compiler->func->chunk, valStr);
    pop(compiler->vm);

    if(index < 0){
        errorAt(compiler, &compiler->parser.pre, "Failed to add module path constant.");
        return;
    }
    if(index <= 0xff){
        emitPair(compiler, OP_IMPORT, (uint8_t)index);
    }else if(index <= 0xffff){
        emitByte(compiler, OP_LIMPORT);
        emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many constants.");
        return;
    }

    advance(compiler);
    consume(compiler, TOKEN_STRING_END, "Expect '\"' after module name string.");

    Token aliasName = pathToken;
    for(int i = 0; i < aliasName.len; i++){
        char c = aliasName.head[1];
        if(c == '/' || c == '\\'){
            aliasName.head += i + 1;
            aliasName.len -= i + 1;
            i = -1;
        }
    }

    for(int i = aliasName.len - 1; i >= 0; i--){
        char c = aliasName.head[i];
        if(c == '.'){
            aliasName.len = i;
            break;
        }
    }

    int aliasIndex = 0;
    if(compiler->scopeDepth > 0){
        addLocal(compiler, aliasName);
        compiler->locals[compiler->localCnt - 1].depth = compiler->scopeDepth;
    }else{
        Value aliasValue = OBJECT_VAL(copyString(compiler->vm, aliasName.head, aliasName.len));
        push(compiler->vm, aliasValue);
        aliasIndex = addConstant(compiler->vm, &compiler->func->chunk, aliasValue);
        pop(compiler->vm);
        if(aliasIndex < 0){
            errorAt(compiler, &compiler->parser.pre, "Failed to add module alias constant.");
            return;
        }
        defineVar(compiler, aliasIndex);
    }

    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after import statement.");
}

static void beginScope(Compiler* compiler){
    compiler->scopeDepth++;
}

static void block(Compiler* compiler){
    while(!checkType(compiler, TOKEN_RIGHT_BRACE) && !checkType(compiler, TOKEN_EOF)){
        decl(compiler);
    }
    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void endScope(Compiler* compiler){
    compiler->scopeDepth--;
    while(compiler->localCnt > 0 && 
        compiler->locals[compiler->localCnt-1].depth > compiler->scopeDepth){
            emitByte(compiler, OP_CLOSE_UPVALUE);
            compiler->localCnt--;
        }
}

static void stmt(Compiler* compiler){
    if(match(compiler, TOKEN_PRINT))
        printStmt(compiler);
    else if(match(compiler, TOKEN_IF))
        ifStmt(compiler);
    else if(match(compiler, TOKEN_WHILE))
        whileStmt(compiler);
    else if(match(compiler, TOKEN_FOR))
        forStmt(compiler);
    else if(match(compiler, TOKEN_BREAK))
        breakStmt(compiler);
    else if(match(compiler, TOKEN_SWITCH))
        switchStmt(compiler);
    else if(match(compiler, TOKEN_CONTINUE))
        continueStmt(compiler);
    else if(match(compiler, TOKEN_SYSTEM))
        systemStmt(compiler);
    else if(match(compiler, TOKEN_RETURN))
        returnStmt(compiler);
    else if(match(compiler, TOKEN_LEFT_BRACE)){
        beginScope(compiler);
        block(compiler);
        endScope(compiler);
    }else expressionStmt(compiler);
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

static void ifStmt(Compiler* compiler){
    /*
    code: if (condition) { then_branch } else { else_branch }

    Bytecode Layout & Jumps:
    
            +--------------------------------------------+
            |                                            |
            V                                            |
    | ---cond--- | OP_JUMP_IF_FALSE | offset_to_else | OP_POP | ---then--- | OP_JUMP | offset_to_end | OP_POP | ---else--- |
                                    |                                                ^
                                    |                                                |
                                    +------------------------------------------------+
    */
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    stmt(compiler);

    int elseJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, thenJump);

    emitByte(compiler, OP_POP);
    if(match(compiler, TOKEN_ELSE)) stmt(compiler);

    patchJump(compiler, elseJump);
}

static void whileStmt(Compiler* compiler){
    /*
    code: while (condition) { body }

    Bytecode Layout & Jumps:

                            +---------------------------------------+
                            |                                       |
                            |                                       v
    | ---cond--- | OP_JUMP_IF_FALSE | OP_POP | ---body--- | OP_LOOP | OP_POP |
    ^                                                           |
    |                                                           |
    +-----------------------------------------------------------+
    */

    int loopStart = compiler->func->chunk.count;

    if(compiler->loopCnt == LOOP_MAX){
        errorAt(compiler, &compiler->parser.pre, "Too many nested loops.");
        return;
    }
    
    Loop *loop = &compiler->loops[compiler->loopCnt++];
    loop->start = loopStart;
    loop->scopeDepth = compiler->scopeDepth;
    loop->breakCnt = 0;

    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    stmt(compiler);

    emitLoop(compiler, loopStart);

    patchJump(compiler, exitJump);
    emitByte(compiler, OP_POP);

    for(int i = 0; i < loop->breakCnt; i++){
        patchJump(compiler, loop->breakJump[i]);
    }
    compiler->loopCnt--;
}

static void emitGetGlobal(Compiler* compiler, const char* name){
    Value strVal = OBJECT_VAL(copyString(compiler->vm, name, strlen(name)));
    push(compiler->vm, strVal);
    int index = addConstant(compiler->vm, &compiler->func->chunk, strVal);
    pop(compiler->vm);

    if(index <= 0xff){
        emitPair(compiler, OP_GET_GLOBAL, (uint8_t)index);
    }else{
        emitByte(compiler, OP_GET_LGLOBAL);
        emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
    }
}

static void forStmt(Compiler* compiler){
    /*
    code: for (init; condition; increment) { body }

    Bytecode Layout & Jumps:

                 +------------------------------------------------------------------+
                 |                                                  +---------------|-------------------------------+
                 |                                              +---|---------------|--------------+                |
                 |                                              |   |               |              |                |
                 v                                              |   v               |              v                |
    | ---init--- | ---cond--- | OP_JUMP_IF_FALSE | OP_POP | OP_JUMP | ---inc--- | OP_LOOP | OP_POP | ---body--- | OP_LOOP | OP_POP |
                                        |                                                                                 ^
                                        |                                                                                 |
                                        +---------------------------------------------------------------------------------+
    */
    beginScope(compiler);
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    if(match(compiler, TOKEN_VAR)){
        int varConst = parseVar(compiler, "Expect variable name.");
        if(match(compiler, TOKEN_COLON)){
            emitByte(compiler, OP_NULL);
            defineVar(compiler, varConst);

            emitGetGlobal(compiler, "iter"); // [x, iter_func]
            expression(compiler);

            emitPair(compiler, OP_CALL, 1);  // [x, iterator]

            Token iterToken;
            iterToken.head = "$iter";
            iterToken.len = 5;
            iterToken.type = TOKEN_IDENTIFIER;
            iterToken.line = 0;
            addLocal(compiler, iterToken);
            compiler->locals[compiler->localCnt - 1].depth = compiler->scopeDepth;

            int loopStart = compiler->func->chunk.count;
            emitGetGlobal(compiler, "next");

            emitPair(compiler, OP_GET_LOCAL, (uint8_t)(compiler->localCnt - 1)); 
            // [x, iterator, next_func, iterator]
            emitPair(compiler, OP_CALL, 1);  
            // [x, iterator, next_val]
            emitPair(compiler, OP_SET_LOCAL, (uint8_t)(compiler->localCnt - 2)); 

            emitByte(compiler, OP_DUP);  
            // [..., next_val, next_val]
            emitByte(compiler, OP_NULL); 
            // [..., next_val, next_val, null]
            emitByte(compiler, OP_EQUAL); 
            // [..., next_val, is_null]
            
            emitByte(compiler, OP_NOT);   
            // [..., next_val, is_valid]
            int exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
            
            emitByte(compiler, OP_POP);
            emitByte(compiler, OP_POP);

            consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after foreach.");

            Loop* loop = &compiler->loops[compiler->loopCnt++];
            loop->start = loopStart;
            loop->scopeDepth = compiler->scopeDepth;
            loop->breakCnt = 0;

            stmt(compiler);

            emitLoop(compiler, loopStart);

            patchJump(compiler, exitJump);
            emitByte(compiler, OP_POP);
            emitByte(compiler, OP_POP);

            for(int i = 0; i < loop->breakCnt; i++){
                patchJump(compiler, loop->breakJump[i]);
            }
            compiler->loopCnt--;
            endScope(compiler);
            return;
        }

        if(match(compiler, TOKEN_ASSIGN)){
            expression(compiler);
        }else{
            emitByte(compiler, OP_NULL);
        }
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
        defineVar(compiler, varConst);
    }else{
        if(match(compiler, TOKEN_SEMICOLON)){
            // pass
        }else{
            expressionStmt(compiler);
        }
    }

    int loopStart = compiler->func->chunk.count;

    int exitJump = -1;
    if(!match(compiler, TOKEN_SEMICOLON)){
        expression(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
        exitJump = emitJump(compiler, OP_JUMP_IF_FALSE);
        emitByte(compiler, OP_POP);
    }

    if(!match(compiler, TOKEN_SEMICOLON)){
        int bodyJump = emitJump(compiler, OP_JUMP);
        int incrementStart = compiler->func->chunk.count;

        expression(compiler);
        emitByte(compiler, OP_POP);
        consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(compiler, loopStart);
        loopStart = incrementStart;
        patchJump(compiler, bodyJump);
    }

    if(compiler->loopCnt == LOOP_MAX){
        errorAt(compiler, &compiler->parser.pre, "Too many nested loops.");
        return;
    }
    
    Loop *loop = &compiler->loops[compiler->loopCnt++];
    loop->start = loopStart;
    loop->scopeDepth = compiler->scopeDepth;
    loop->breakCnt = 0;

    stmt(compiler);
    emitLoop(compiler, loopStart);

    if(exitJump != -1){
        patchJump(compiler, exitJump);
        emitByte(compiler, OP_POP);
    }

    for(int i = 0; i < loop->breakCnt; i++){
        patchJump(compiler, loop->breakJump[i]);
    }

    compiler->loopCnt--;
    endScope(compiler);
}

static void breakStmt(Compiler* compiler){
    if(compiler->loopCnt == 0){
        errorAt(compiler, &compiler->parser.pre, "Cannot use 'break' outside of a loop.");
        return;
    }
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after 'break'.");

    while(compiler->localCnt > 0 && 
          compiler->locals[compiler->localCnt-1].depth > compiler->loops[compiler->loopCnt - 1].scopeDepth){
            emitByte(compiler, OP_POP);
            compiler->localCnt--;
    }

    Loop *curLoop = &compiler->loops[compiler->loopCnt - 1];
    if(curLoop->breakCnt == LOOP_MAX){
        errorAt(compiler, &compiler->parser.pre, "Too many 'break' statements in one loop.");
        return;
    }
    curLoop->breakJump[curLoop->breakCnt++] = emitJump(compiler, OP_JUMP);
}

static void continueStmt(Compiler* compiler){
    if(compiler->loopCnt == 0){
        errorAt(compiler, &compiler->parser.pre, "Cannot use 'continue' outside of a loop.");
        return;
    }
    consume(compiler, TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    while(compiler->localCnt > 0 && 
          compiler->locals[compiler->localCnt-1].depth > compiler->loops[compiler->loopCnt - 1].scopeDepth){
            emitByte(compiler, OP_POP);
            compiler->localCnt--;
    }

    Loop *curLoop = &compiler->loops[compiler->loopCnt - 1];
    emitLoop(compiler, curLoop->start);
}

static void switchStmt(Compiler* compiler){
    consume(compiler, TOKEN_LEFT_PAREN, "Expect '(' after 'switch'.");
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after switch condition.");
    consume(compiler, TOKEN_LEFT_BRACE, "Expect '{' before switch cases.");

    int endJumps[CASE_MAX];
    int endJumpCnt = 0;
    
    int fallthroughJump = -1;
    
    int caseCnt = 0;
    bool hasDefault = false;

    while(!checkType(compiler, TOKEN_RIGHT_BRACE) && !checkType(compiler, TOKEN_EOF)){
        if(fallthroughJump != -1){
            patchJump(compiler, fallthroughJump);
            fallthroughJump = -1; 
        }

        if(caseCnt++ >= CASE_MAX){
            errorAt(compiler, &compiler->parser.pre, "Too many cases in one switch.");
            while(!checkType(compiler, TOKEN_RIGHT_BRACE) && !checkType(compiler, TOKEN_EOF)) advance(compiler);
            break;
        }

        if(match(compiler, TOKEN_DEFAULT)){
            if(hasDefault){
                errorAt(compiler, &compiler->parser.pre, "Multiple default cases in one switch.");
            }
            hasDefault = true;

            consume(compiler, TOKEN_FAT_ARROW, "Expect '=>' after 'default'.");
            emitByte(compiler, OP_POP);
            stmt(compiler);
        }else{
            int bodyJumps[CASE_MAX];
            int bodyJumpCount = 0;
            do{
                emitByte(compiler, OP_DUP);
                expression(compiler);
                emitByte(compiler, OP_EQUAL);
                
                int failedMatchJump = emitJump(compiler, OP_JUMP_IF_FALSE);

                bodyJumps[bodyJumpCount++] = emitJump(compiler, OP_JUMP);
                
                patchJump(compiler, failedMatchJump);

            }while(match(compiler, TOKEN_COMMA));
            
            fallthroughJump = emitJump(compiler, OP_JUMP);
            
            for(int i = 0; i < bodyJumpCount; i++){
                patchJump(compiler, bodyJumps[i]);
            }

            consume(compiler, TOKEN_FAT_ARROW, "Expect '=>' after case value(s).");
            
            emitByte(compiler, OP_POP);
            stmt(compiler);
            
            endJumps[endJumpCnt++] = emitJump(compiler, OP_JUMP);
        }
    }
    
    if(fallthroughJump != -1){
        patchJump(compiler, fallthroughJump);
    }

    for(int i = 0; i < endJumpCnt; i++){
        patchJump(compiler, endJumps[i]);
    }

    if(!hasDefault){
        emitByte(compiler, OP_POP);
    }else if(endJumpCnt == 0 && !hasDefault){ 
        emitByte(compiler, OP_POP);
    }

    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after switch cases.");
}

static void systemStmt(Compiler* compiler){
    Token cmdToken = compiler->parser.pre;

    Value cmdVal = OBJECT_VAL(copyString(compiler->vm, cmdToken.head, cmdToken.len));
    emitConstant(compiler, cmdVal);

    emitByte(compiler, OP_SYSTEM);
    emitByte(compiler, OP_POP);  // pop status code
}

static void returnStmt(Compiler* compiler){
    if(compiler->type == TYPE_SCRIPT){
        errorAt(compiler, &compiler->parser.pre, "Cannot return from the top-level.");
    }

    if(match(compiler, TOKEN_SEMICOLON)){
        if(compiler->type == TYPE_INITIALIZER){
            emitPair(compiler, OP_GET_LOCAL, 0); // return 'this'
        }else{
            emitByte(compiler, OP_NULL);
        }
        emitPair(compiler, OP_NULL, OP_RETURN);
    }else{
        if(compiler->type == TYPE_INITIALIZER){
            errorAt(compiler, &compiler->parser.pre, "Can't return a value from an initializer.");
        }
        expression(compiler);
        consume(compiler, TOKEN_SEMICOLON, "Expected ';' after return value.");
        emitByte(compiler, OP_RETURN);
    }
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

    push(compiler->vm, strVal);
    int index = addConstant(compiler->vm, &compiler->func->chunk, strVal);
    pop(compiler->vm);

    return index;
}

static void addLocal(Compiler* compiler, Token name){
    if(compiler->localCnt == LOCAL_MAX){
        errorAt(compiler, &name, "Too many local variables");
        return;
    }
    Local* local = &compiler->locals[compiler->localCnt++];
    local->name = name;
    local->depth = -1;  // sentinel, decl-ed but not def-ed
}

static void declLocal(Compiler* compiler){
    if(compiler->scopeDepth == 0){
        return;
    }
    Token* name = &compiler->parser.pre;
    for(int i = compiler->localCnt-1; i >= 0; i--){
        Local* local = &compiler->locals[i];
        if(local->depth != -1 && local->depth < compiler->scopeDepth){
            break;
        }
        if(name->len == local->name.len && memcmp(name->head, local->name.head, name->len) == 0){
            errorAt(compiler, name, "Variable with this name already existed.");
        }
    }
    addLocal(compiler, *name);
}

static int parseVar(Compiler* compiler, const char* err){
    consume(compiler, TOKEN_IDENTIFIER, err);
    if(compiler->scopeDepth > 0){
        declLocal(compiler);
        return 0;
    }
    return identifierConst(compiler);
}

static void defineVar(Compiler* compiler, int global){
    if(compiler->scopeDepth > 0){
        compiler->locals[compiler->localCnt-1].depth = compiler->scopeDepth;
        return;
    }
    if(global <= 0xff){
        emitByte(compiler, OP_DEFINE_GLOBAL);
        emitByte(compiler, (uint8_t)global);
    }else if(global <= 0xffff){
        emitByte(compiler, OP_DEFINE_LGLOBAL);
        emitPair(compiler, (uint8_t)((global >> 8) & 0xff), (uint8_t)(global & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many variables declared");
    }
}

static int resolveLocal(Compiler* compiler, Token* name){
    // search matched local variable
    for(int i = compiler->localCnt-1; i >= 0; i--){
        Local* local = &compiler->locals[i];
        if(name->len == local->name.len && 
            memcmp(name->head, local->name.head, name->len) == 0){
                if(local->depth == -1){
                    errorAt(compiler, name, "Cannot read local variable");
                }
                return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint16_t index, bool isLocal){
    for(int i = 0; i < compiler->upvalueCnt; i++){
        if(compiler->upvalues[i].index == index && compiler->upvalues[i].isLocal == isLocal){
            return i;
        }
    }

    if(compiler->upvalueCnt == LOCAL_MAX){
        errorAt(compiler, &compiler->parser.pre, "Too many upvalues.");
        return 0;
    }

    compiler->upvalues[compiler->upvalueCnt].isLocal = isLocal;
    compiler->upvalues[compiler->upvalueCnt].index = index;
    compiler->func->upvalueCnt = compiler->upvalueCnt + 1;
    return compiler->upvalueCnt++;    // return current upvalue count
}

static int resolveUpvalue(Compiler* compiler, Token* name){
    if(compiler->enclosing == NULL) return -1;  // top compiler
    
    int localIndex = resolveLocal(compiler->enclosing, name);
    if(localIndex != -1){
        return addUpvalue(compiler, (uint16_t)localIndex, true);
    }

    int upvalueIndex = resolveUpvalue(compiler->enclosing, name);
    // recursive
    if(upvalueIndex != -1){
        return addUpvalue(compiler, (uint16_t)upvalueIndex, false);
    }

    return -1;
}

static void emitByte(Compiler* compiler, uint8_t byte){
    writeChunk(compiler->vm, &compiler->func->chunk, byte, compiler->parser.pre.line);
}

static void emitPair(Compiler* compiler, uint8_t byte1, uint8_t byte2){
    emitByte(compiler, byte1);
    emitByte(compiler, byte2);
}

static int emitJump(Compiler* compiler, uint8_t instruction){
    emitByte(compiler, instruction);
    emitPair(compiler, 0xff, 0xff);
    return compiler->func->chunk.count - 2;
}

static void emitLoop(Compiler* compiler, int loopStart){
    emitByte(compiler, OP_LOOP);
    int offset = compiler->func->chunk.count - loopStart + 2;
    if(offset > UINT16_MAX){
        errorAt(compiler, &compiler->parser.pre, "Loop too large.");
    }
    emitPair(compiler, (uint8_t)((offset >> 8) & 0xff), (uint8_t)(offset & 0xff));
}

static void patchJump(Compiler* compiler, int offset){
    int jump = compiler->func->chunk.count - (offset + 2);
    if(jump > UINT16_MAX){
        errorAt(compiler, &compiler->parser.pre, "Jump too long.");
    }
    compiler->func->chunk.code[offset] = (jump >> 8) & 0xff;
    compiler->func->chunk.code[offset+1] = jump & 0xff;
}

static ObjectFunc* stopCompiler(Compiler* compiler){
    if(compiler->type == TYPE_INITIALIZER){
        emitPair(compiler, OP_GET_LOCAL, 0);
    }else{
        emitByte(compiler, OP_NULL);
    }
    emitByte(compiler, OP_RETURN);

    ObjectFunc* func = compiler->func;

    #ifdef DEBUG_PRINT_CODE
    if(!compiler->parser.hadError){
        printf("== Compiled code ==\n");
        dasmChunk(&compiler->func->chunk, "code");
    }
    #endif

    return func;
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
    push(compiler->vm, value);
    int constantIndex = addConstant(compiler->vm, &compiler->func->chunk, value);
    pop(compiler->vm);

    if(constantIndex < 0){
        fprintf(stderr, "Failed to add constant\n");
        return;
    }else if(constantIndex <= 0xff){
        emitPair(compiler, OP_CONSTANT, (uint8_t)constantIndex);
        return;
    }else if(constantIndex <= 0xffff){
        emitByte(compiler, OP_LCONSTANT);
        emitPair(compiler, (uint8_t)((constantIndex >> 8) & 0xff), (uint8_t)(constantIndex & 0xff));
        return;
    }else{
        fprintf(stderr, "Constant index out of range: %d\n", constantIndex);
        return;
    }
}

static void handleVar(Compiler* compiler, bool canAssign){
    Token* name = &compiler->parser.pre;

    uint8_t getOp, setOp, getLOp, setLOp;
    int index;

    if((index = resolveLocal(compiler, name)) != -1){
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        getLOp = OP_GET_LLOCAL;
        setLOp = OP_SET_LLOCAL;
    }else if((index = resolveUpvalue(compiler, name)) != -1){
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
        getLOp = OP_GET_LUPVALUE; 
        setLOp = OP_SET_LUPVALUE;
    }else{
        index = identifierConst(compiler);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
        getLOp = OP_GET_LGLOBAL;
        setLOp = OP_SET_LGLOBAL;
    }

    uint8_t finalOp, finalLOp;
    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        expression(compiler);
        finalOp = setOp;
        finalLOp = setLOp;
    }else if(canAssign && match(compiler, TOKEN_PLUS_EQUAL)){
        if(index <= 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{
            emitByte(compiler, getLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        expression(compiler);
        emitByte(compiler, OP_ADD);
        finalOp = setOp;
        finalLOp = setLOp;
    }else if(canAssign && match(compiler, TOKEN_MINUS_EQUAL)){
        if(index <= 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{
            emitByte(compiler, getLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        expression(compiler);
        emitByte(compiler, OP_SUBTRACT);
        finalOp = setOp;
        finalLOp = setLOp;
    }else if(canAssign && match(compiler, TOKEN_PLUS_PLUS)){
        if(index <= 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{
            emitByte(compiler, getLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        emitByte(compiler, OP_DUP);
        emitConstant(compiler, NUM_VAL(1));
        emitByte(compiler, OP_ADD);

        if(index <= 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{
            emitByte(compiler, setLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        emitByte(compiler, OP_POP);
        return;
    }else if(canAssign && match(compiler, TOKEN_MINUS_MINUS)){
        if(index <= 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{
            emitByte(compiler, getLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        emitByte(compiler, OP_DUP);
        emitConstant(compiler, NUM_VAL(1));
        emitByte(compiler, OP_SUBTRACT);

        if(index <= 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{
            emitByte(compiler, setLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        emitByte(compiler, OP_POP);
        return;
    }else{
        finalOp = getOp;
        finalLOp = getLOp;
    }

    if(index <= 0xff){
        emitPair(compiler, finalOp, (uint8_t)index);
    }else if(index <= 0xffff){
        emitByte(compiler, finalLOp);
        emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
    }else{
        errorAt(compiler, &compiler->parser.pre, "Too many variables!");
    }
}

static void handleGrouping(Compiler* compiler, bool canAssign){
    expression(compiler);
    consume(compiler, TOKEN_RIGHT_PAREN, "Expected ')' after expression");
}

static void handleUnary(Compiler* compiler, bool canAssign){
    TokenType type = compiler->parser.pre.type;

    if(type == TOKEN_PLUS_PLUS || type == TOKEN_MINUS_MINUS){
        consume(compiler, TOKEN_IDENTIFIER, "Expect variable name after prefix operator.");
        
        Token* name = &compiler->parser.pre;
        uint8_t getOp, setOp, getLOp, setLOp;
        
        int index = resolveLocal(compiler, name);
        if(index != -1){
            getOp = OP_GET_LOCAL; setOp = OP_SET_LOCAL;
            getLOp = OP_GET_LLOCAL; setLOp = OP_SET_LLOCAL;
        }else if((index = resolveUpvalue(compiler, name)) != -1){
            getOp = OP_GET_UPVALUE; setOp = OP_SET_UPVALUE;
            getLOp = OP_GET_LUPVALUE; setLOp = OP_SET_LUPVALUE;
        }else{
            index = identifierConst(compiler);
            getOp = OP_GET_GLOBAL; setOp = OP_SET_GLOBAL;
            getLOp = OP_GET_LGLOBAL; setLOp = OP_SET_LGLOBAL;
        }

        if(index <= 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{
            emitByte(compiler, getLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }

        emitConstant(compiler, NUM_VAL(1));
        emitByte(compiler, (type == TOKEN_PLUS_PLUS) ? OP_ADD : OP_SUBTRACT);

        if(index <= 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{
            emitByte(compiler, setLOp);
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }
        return;
    }

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
        case TOKEN_PERCENT:         emitByte(compiler, OP_MODULO); break;
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

static void handleAnd(Compiler* compiler, bool canAssign){
    int endJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    emitByte(compiler, OP_POP);
    parsePrecedence(compiler, PREC_AND);
    patchJump(compiler, endJump);
}

static void handleOr(Compiler* compiler, bool canAssign){
    int elseJump = emitJump(compiler, OP_JUMP_IF_FALSE);
    int endJump = emitJump(compiler, OP_JUMP);
    patchJump(compiler, elseJump);
    emitByte(compiler, OP_POP);
    parsePrecedence(compiler, PREC_OR);
    patchJump(compiler, endJump);
}

static uint8_t argList(Compiler* compiler){
    uint8_t argCnt = 0;
    if(!checkType(compiler, TOKEN_RIGHT_PAREN)){
        do{
            expression(compiler);
            if(argCnt == 255){
                errorAt(compiler, &compiler->parser.pre, "Cannot have more than 255 arguments.");
            }
            argCnt++;
        }while(match(compiler, TOKEN_COMMA));
    }
    consume(compiler, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCnt;
}

static void handleCall(Compiler* compiler, bool canAssign){
    uint8_t argCount = argList(compiler);
    emitByte(compiler, OP_CALL);
    emitByte(compiler, argCount);
}

static void handleImport(Compiler* compiler, bool canAssign){
    consume(compiler, TOKEN_STRING_START, "Expect a string after 'import'.");

    if(compiler->parser.cur.type == TOKEN_STRING_END){
        errorAt(compiler, &compiler->parser.pre, "import path cannot be empty.");
    }else{
        Token *token = &compiler->parser.cur;
        Value valStr = OBJECT_VAL(copyString(compiler->vm, token->head, token->len));
        
        push(compiler->vm, valStr);
        int index = addConstant(compiler->vm, &compiler->func->chunk, valStr);
        pop(compiler->vm);

        if(index < 256){
            emitPair(compiler, OP_IMPORT, (uint8_t)index);
        }else if(index < 0xffff){
            emitByte(compiler, OP_LIMPORT);
            emitByte(compiler, (uint8_t)((index >> 8) & 0xff));
            emitByte(compiler, (uint8_t)(index & 0xff));
        }else{
            errorAt(compiler, &compiler->parser.pre, "Too many constants.");
        }
        advance(compiler);
    }

    consume(compiler, TOKEN_STRING_END, "Expected '\"' after import path.");
}

static void handleDot(Compiler* compiler, bool canAssign){
    consume(compiler, TOKEN_IDENTIFIER, "Expect property name after '.'.");

    Token* name = &compiler->parser.pre;
    Value strVal = OBJECT_VAL(copyString(compiler->vm, name->head, name->len));

    push(compiler->vm, strVal);
    int index = addConstant(compiler->vm, &compiler->func->chunk, strVal);
    pop(compiler->vm);

    uint8_t finalOp, finalLOp;

    uint8_t getOp = (index < 0xff) ? OP_GET_PROPERTY : OP_GET_LPROPERTY;
    uint8_t setOp = (index < 0xff) ? OP_SET_PROPERTY : OP_SET_LPROPERTY;

    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        expression(compiler);
        if(index < 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, setOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }
    }else if(canAssign && match(compiler, TOKEN_PLUS_EQUAL)){
        emitByte(compiler, OP_DUP);
        if(index < 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, getOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff)); 
        }

        expression(compiler);
        emitByte(compiler, OP_ADD);
        
        if(index < 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, setOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }
    }else if(canAssign && match(compiler, TOKEN_MINUS_EQUAL)){
        emitByte(compiler, OP_DUP);
        if(index < 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, getOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff)); 
        }

        expression(compiler);
        emitByte(compiler, OP_SUBTRACT);
        
        if(index < 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, setOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }
    }else if(canAssign && match(compiler, TOKEN_PLUS_PLUS)){
        emitByte(compiler, OP_DUP);
        
        if(index < 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, getOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff)); 
        }

        emitConstant(compiler, NUM_VAL(1));
        emitByte(compiler, OP_ADD);

        if(index < 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, setOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }
        emitConstant(compiler, NUM_VAL(1)); // [new_val, 1]
        emitByte(compiler, OP_SUBTRACT);
    }else if(canAssign && match(compiler, TOKEN_MINUS_MINUS)){
        emitByte(compiler, OP_DUP);
        
        if(index < 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, getOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff)); 
        }

        emitConstant(compiler, NUM_VAL(1));
        emitByte(compiler, OP_SUBTRACT);

        if(index < 0xff){
            emitPair(compiler, setOp, (uint8_t)index);
        }else{ 
            emitByte(compiler, setOp); 
            emitPair(compiler, (uint8_t)((index >> 8) & 0xff), (uint8_t)(index & 0xff));
        }
        emitConstant(compiler, NUM_VAL(1)); // [new_val, 1]
        emitByte(compiler, OP_ADD);
    }else{
        if(index < 0xff){
            emitPair(compiler, getOp, (uint8_t)index);
        }else{
            emitByte(compiler, getOp);
            emitByte(compiler, (uint8_t)((index >> 8) & 0xff));
            emitByte(compiler, (uint8_t)(index & 0xff));
        }
    }
}

static void handleList(Compiler* compiler, bool canAssign){
    uint8_t itemCnt = 0;
    if(!checkType(compiler, TOKEN_RIGHT_BRACKET)){
        expression(compiler);
        if(checkType(compiler, TOKEN_SEMICOLON)){
            consume(compiler, TOKEN_SEMICOLON, "Expect ';' in list bulk initialization.");
            expression(compiler);
            emitByte(compiler, OP_FILL_LIST);
            consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after list bulk initialization.");
            return;
        }

        itemCnt++;
        while(match(compiler, TOKEN_COMMA)){
            expression(compiler);
            if(itemCnt == 255){
                errorAt(compiler, &compiler->parser.pre, "Cannot have more than 255 items in list.");
            }
            itemCnt++;
        }
    }

    consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after list items.");
    emitByte(compiler, OP_BUILD_LIST);
    emitByte(compiler, (uint8_t)itemCnt);

}

static void handleIndex(Compiler* compiler, bool canAssign){
    bool isSlice = false;

    if(match(compiler, TOKEN_COLON)){
        isSlice = true;
        emitByte(compiler, OP_NULL);
    }else{
        expression(compiler);
        if(match(compiler, TOKEN_COLON)){
            isSlice = true;
        }
    }

    if(isSlice){
        if(checkType(compiler, TOKEN_COLON) || checkType(compiler, TOKEN_RIGHT_BRACKET)){
            // if match [::step] or [start:], end is omitted
            emitByte(compiler, OP_NULL);    // end as null
        }else{
            expression(compiler);
        }

        if(match(compiler, TOKEN_COLON)){
            if(checkType(compiler, TOKEN_RIGHT_BRACKET)){ // default step
                emitByte(compiler, OP_NULL);
            }else{
                expression(compiler);
            }
        }else{  // no step
            emitByte(compiler, OP_NULL);
        }

        consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after slice.");
        emitByte(compiler, OP_SLICE);
        return;
    }

    consume(compiler, TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        expression(compiler);
        emitByte(compiler, OP_INDEX_SET);
    }else if(canAssign && match(compiler, TOKEN_PLUS_EQUAL)){
        emitByte(compiler, OP_DUP_2);       // [list, index, list, index]
        emitByte(compiler, OP_INDEX_GET);   // [list, index, val]
        expression(compiler);               // [list, index, val, rhs]
        emitByte(compiler, OP_ADD);         // [list, index, new_val]
        emitByte(compiler, OP_INDEX_SET);   // [new_val]
    }
    else if(canAssign && match(compiler, TOKEN_MINUS_EQUAL)){
        emitByte(compiler, OP_DUP_2);
        emitByte(compiler, OP_INDEX_GET);
        expression(compiler);
        emitByte(compiler, OP_SUBTRACT);
        emitByte(compiler, OP_INDEX_SET);
    }else if(canAssign && match(compiler, TOKEN_PLUS_PLUS)){
        emitByte(compiler, OP_DUP_2);       // [list, index, list, index]
        emitByte(compiler, OP_INDEX_GET);   // [list, index, val]
        emitConstant(compiler, NUM_VAL(1)); 
        emitByte(compiler, OP_ADD);         // [list, index, val, new_val] 
        emitByte(compiler, OP_INDEX_SET);   // [new_val]
        emitConstant(compiler, NUM_VAL(1)); // [new_val, 1]
        emitByte(compiler, OP_SUBTRACT);    // [old_val]
    }else if(canAssign && match(compiler, TOKEN_MINUS_MINUS)){
        emitByte(compiler, OP_DUP_2);       // [list, index, list, index]
        emitByte(compiler, OP_INDEX_GET);   // [list, index, val]
        emitConstant(compiler, NUM_VAL(1)); 
        emitByte(compiler, OP_SUBTRACT);    // [list, index, new_val]
        emitByte(compiler, OP_INDEX_SET);   // [new_val]
        emitConstant(compiler, NUM_VAL(1)); 
        emitByte(compiler, OP_ADD);         // [old_val]
    }else{
        emitByte(compiler, OP_INDEX_GET);
    }
}

static void handleMap(Compiler* compiler, bool canAssign){
    uint8_t itemCnt = 0;
    if(!checkType(compiler, TOKEN_RIGHT_BRACE)){
        do{
            if(itemCnt >= 255){
                errorAt(compiler, &compiler->parser.pre, "Cannot have more than 256 items when init with a map entry.");
            }
            expression(compiler);
            consume(compiler, TOKEN_COLON, "Expect ':' after map key.");
            expression(compiler);
            itemCnt++;
        }while(match(compiler, TOKEN_COMMA));
    }
    consume(compiler, TOKEN_RIGHT_BRACE, "Expect '}' after map entries");
    emitPair(compiler, OP_BUILD_MAP, itemCnt);
}

static void handleThis(Compiler* compiler, bool canAssign){
    if(compiler->enclosing == NULL || compiler->type != TYPE_METHOD){
        errorAt(compiler, &compiler->parser.pre, "Cannot use 'this' outside of a method.");
        return;
    }
    if(canAssign && match(compiler, TOKEN_ASSIGN)){
        errorAt(compiler, &compiler->parser.pre, "Cannot assign to 'this'.");
        return;
    }
    emitPair(compiler, OP_GET_LOCAL, 0);
}

static void consume(Compiler* compiler, TokenType type, const char* errMsg){
    if(compiler->parser.cur.type == type){
        advance(compiler);
        return;
    }
    errorAt(compiler, &compiler->parser.cur, errMsg);
}

static void errorAt(Compiler* compiler, Token* token, const char* message){
    const char* srcName = compiler->func->srcName != NULL 
                            ? compiler->func->srcName->chars 
                            : "<script>";

    fprintf(stderr, "Error [%s, line %d] ",srcName, compiler->parser.cur.line);
    if(token->type != TOKEN_EOF){
        fprintf(stderr, "at '%.*s': ", token->len, token->head);
    }else{
        fprintf(stderr, "at end: ");
    }
    fprintf(stderr, "%s", message);
    fprintf(stderr, "\n");
    compiler->parser.hadError = true;
}