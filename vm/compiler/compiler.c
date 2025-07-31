#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/common.h"
#include "compiler.h"
#include "../util/memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "../util/debug.h"
#endif

typedef struct {
    Token name;
    int depth;
    int loopDepth;
    bool fixed;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    FUNC_FUNCTION,
    FUNC_METHOD,
    FUNC_BUILDER,
    FUNC_SCRIPT,
} FunctionType;

typedef struct CodePoint {
    int code;
    int scopeDepth;
    int loopDepth;
} CodePoint;

struct Compiler {
    struct Compiler* enclosing;

    Local locals[UINT8_COUNT];
    int localCount;
    int scopeDepth;
    
    CodePoint breakPoints[UINT8_COUNT];
    int breakCount;
    CodePoint loopPoints[UINT8_COUNT];
    int loopDepth;

    Upvalue upvalues[UINT8_COUNT];

    ObjFunction* function;
    FunctionType type;
};

struct ClassCompiler {
    struct ClassCompiler* enclosing;
    bool hasSuperclass;
};

typedef struct {
    Token current;
    Token previous;
    Scanner* scanner;
    bool hadError;
    bool panicMode;
    VM* vm;
    Compiler* compiler;
    ClassCompiler* classCompiler;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_OR,          // ||
    PREC_AND,         // &&
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * /
    PREC_UNARY,       // ! -
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(Parser* parser, bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence prec;
} ParseRule;

static Chunk* currentChunk(Parser* parser) {
    return &parser->compiler->function->chunk;
}

static void errorAt(Parser* parser, Token* tok, const char* msg) {
    if (parser->panicMode) return;
    parser->panicMode = true;

    fprintf(stderr, "[line %d] Error", tok->line);

    if (tok->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (tok->type != TOKEN_ERROR) {
        fprintf(stderr, " at '%.*s'", tok->length, tok->start);
    }

    fprintf(stderr, ": %s\n", msg);
    parser->hadError = true;
}

static void errorAtCurrent(Parser* parser, const char* msg) {
    errorAt(parser, &parser->previous, msg);
}

static void error(Parser* parser, const char* msg) {
    errorAt(parser, &parser->previous, msg);
}

static void advance(Parser* parser) {
    parser->previous = parser->current;
    for (;;) {
        parser->current = scanToken(parser->scanner);
        if (parser->current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Parser* parser, TokenType type, const char* msg) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }

    errorAtCurrent(parser, msg);
}

static bool check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type) {
    if (!check(parser, type))
        return false;
    advance(parser);
    return true;
}

// Bytecode emitting

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(parser->vm, currentChunk(parser), byte, parser->previous.line);
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static void emitLoop(Parser* parser, int loopStart) {
    emitByte(parser, OP_LOOP);
    int offs = currentChunk(parser)->count - loopStart + 2;
    if (offs > UINT16_MAX)
        error(parser, "Compiler does not support loops of this size.");
    emitBytes(parser, (offs >> 8) & 0xff, offs & 0xff);
}

static int emitJump(Parser* parser, uint8_t op) {
    emitByte(parser, op);
    emitByte(parser, 0xff);
    emitByte(parser, 0xff);
    return currentChunk(parser)->count - 2;
}

static void patchJump(Parser* parser, int idx) {
    int jump = currentChunk(parser)->count - idx - 2;
    
    if (jump > UINT16_MAX) {
        error(parser, "Compiler does not support jumps of this distance.");
    }

    currentChunk(parser)->code[idx] = (jump >> 8) & 0xff;
    currentChunk(parser)->code[idx + 1] = jump & 0xff;
}

static void emitReturn(Parser* parser) {
    if (parser->compiler->type == FUNC_BUILDER) {
        emitBytes(parser, OP_GET_LOCAL, 0);
    } else {
        emitByte(parser, OP_NULL);
    }

    emitByte(parser, OP_RETURN);
}

static void emitConstant(Parser* parser, Value val) {
    writeConstant(parser->vm, currentChunk(parser), val, parser->previous.line);
}

static void initCompiler(Compiler* compiler, Parser* parser, FunctionType type) {
    compiler->enclosing = parser->compiler;
    parser->compiler = compiler;
    parser->vm->compiler = compiler;

    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->loopDepth = 0;
    compiler->breakCount = 0;

    compiler->function = newFunction(parser->vm);
    compiler->type = type;

    Local* local = &compiler->locals[compiler->localCount++];
    local->depth = 0;
    if (type != FUNC_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
    local->isCaptured = false;

    if (type != FUNC_SCRIPT) {
        compiler->function->name = copyString(parser->vm, parser->previous.start, 
            parser->previous.length);
    }
}

static ObjFunction* endCompiler(Parser* parser) {
    emitReturn(parser);
    ObjFunction* func = parser->compiler->function;

    #ifdef DEBUG_PRINT_CODE
        if (!parser->hadError) {
            disassembleChunk(currentChunk(parser), func->name == NULL ? "<script>" : func->name->chars);
        }
    #endif
        
    parser->compiler = parser->compiler->enclosing;
    parser->vm->compiler = parser->compiler;

    return func;
}

static void beginLoop(Parser* parser) {
    CodePoint* loopPoint = &parser->compiler->loopPoints[parser->compiler->loopDepth];
    loopPoint->code = currentChunk(parser)->count;
    loopPoint->scopeDepth = parser->compiler->scopeDepth;
    // Not really necessary but whatever. vvv
    loopPoint->loopDepth = parser->compiler->loopDepth;
    parser->compiler->loopDepth++;
}

static void endLoop(Parser* parser) {
    parser->compiler->loopDepth--;
    for (int i = parser->compiler->breakCount - 1; i >= 0; i--) {
        CodePoint* breakPoint = &parser->compiler->breakPoints[i];
        if (breakPoint->loopDepth <= parser->compiler->loopDepth)
            break;
        
        patchJump(parser, breakPoint->code);
        parser->compiler->breakCount--;
    }
}

// Parsing

static void expression(Parser* parser);
static void declaration(Parser* parser);
static void statement(Parser* parser);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Parser* parser, Precedence precedence);

static void expression(Parser* parser) {
    parsePrecedence(parser, PREC_ASSIGNMENT);
}

static void markInitialized(Parser* parser) {
    if (parser->compiler->scopeDepth == 0)
        return;
    
    parser->compiler->locals[parser->compiler->localCount - 1].depth =
        parser->compiler->scopeDepth;
}

static void defineVariable(Parser* parser, uint8_t idx) {
    if (parser->compiler->scopeDepth > 0) {
        markInitialized(parser);
        return;
    }
    
    emitBytes(parser, OP_DEFINE_GLOBAL, idx);
}

static uint8_t identifierConstant(Parser* parser, Token* tok) {
    return addConstant(
        parser->vm,
        currentChunk(parser), 
        OBJ_VAL(copyString(parser->vm, tok->start, tok->length))
    );
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static bool identifierEquals(Token* a, const char* text) {
    if (a->length != strlen(text))
        return false;
    return memcmp(a->start, text, a->length) == 0;
}

static int resolveLocal(Parser* parser, Compiler* compiler, Token* tok) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(tok, &local->name)) {
            if (local->depth == -1) {
                error(parser, "Definition of local variable is incomplete.");
            }
            return i;
        }
    }

    return -1;
}

static int addUpvalue(Parser* parser, Compiler* compiler, uint8_t idx, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == idx && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error(parser, "Compiler does not support this many closure variables.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = idx;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Parser* parser, Compiler* compiler, Token* tok) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(parser, compiler->enclosing, tok);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(parser, compiler, (uint8_t) local, true);
    }

    int upvalue = resolveUpvalue(parser, compiler->enclosing, tok);
    if (upvalue != -1) {
        return addUpvalue(parser, compiler, (uint8_t) upvalue, false);
    }

    return -1;
}

static void addLocal(Parser* parser, Token tok, bool constant) {
    if (parser->compiler->localCount == UINT8_COUNT) {
        error(parser, "Maximum local variable count exceeded.");
        return;
    }

    Local* local = &parser->compiler->locals[parser->compiler->localCount++];
    local->name = tok;
    local->depth = -1;
    local->loopDepth = parser->compiler->loopDepth;
    local->fixed = constant;
    local->isCaptured = false;
}

static void declareVariable(Parser* parser, bool constant) {
    if (parser->compiler->scopeDepth == 0)
        return;
    
    Token* name = &parser->previous;

    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        Local* local = &parser->compiler->locals[i];
        if (local->depth != -1 && local->depth < parser->compiler->scopeDepth)
            break;
        
        if (identifiersEqual(name, &local->name)) {
            error(parser, "A variable of the given name already exists in the current scope.");
        }
    }

    addLocal(parser, *name, constant);
}

static uint8_t parseVariable(Parser* parser, const char* errMsg, bool constant) {
    consume(parser, TOKEN_IDENTIFIER, errMsg);

    declareVariable(parser, constant);
    if (parser->compiler->scopeDepth > 0) 
        return 0;
    
    return identifierConstant(parser, &parser->previous);
}

static void varDeclaration(Parser* parser, bool constant) {
    uint8_t global = parseVariable(parser, "Expected variable identifier.", constant);

    if (match(parser, TOKEN_EQUAL)) {
        expression(parser);
    } else {
        emitByte(parser, OP_NULL);
    }

    consume(parser, TOKEN_SEMICOLON, "expected ';' after declaration.");

    defineVariable(parser, global);
}

static uint8_t valueList(Parser* parser, TokenType closing, const char* msg) {
    uint8_t argc = 0;
    if (!check(parser, closing)) {
        do {
            expression(parser);
            if (argc >= 255) {
                error(parser, "Compiler does not support over 255 arguments.");
            }
            argc++;
        } while (match(parser, TOKEN_COMMA));
    }
    consume(parser, closing, msg);
    return argc;
}

static uint8_t argumentList(Parser* parser) {
    return valueList(parser, TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
}

static void beginScope(Parser* parser) {
    parser->compiler->scopeDepth++;
}

static void block(Parser* parser) {
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        declaration(parser);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

static void function(Parser* parser, FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, parser, type);

    beginScope(parser);

    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' before function arguments.");
    if (!check(parser, TOKEN_RIGHT_PAREN)) {
        do {
            parser->compiler->function->arity++;
            if (parser->compiler->function->arity > 255) {
                errorAtCurrent(parser, "Compiler does not support over 255 arguments.");
            }

            bool constant = match(parser, TOKEN_CONST);
            uint8_t idx = parseVariable(parser, "Expected argument identifier.", constant);
            defineVariable(parser, idx);
        } while(match(parser, TOKEN_COMMA));
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after function arguments.");
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' before function body.");

    block(parser);

    ObjFunction* func = endCompiler(parser);
    push(parser->vm, OBJ_VAL(func));
    emitByte(parser, OP_CLOSURE);
    emitConstant(parser, OBJ_VAL(func));
    pop(parser->vm);

    for (int i = 0; i < func->upvalueCount; i++) {
        emitByte(parser, compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(parser, compiler.upvalues[i].index);
    }
}

static void funDeclaration(Parser* parser) {
  uint8_t global = parseVariable(parser, "Expect function name.", false);
  markInitialized(parser);
  function(parser, FUNC_FUNCTION);
  defineVariable(parser, global);
}

static void endScope(Parser* parser) {
    parser->compiler->scopeDepth--;

    uint8_t n = 0;
    while (parser->compiler->localCount > 0 && 
            parser->compiler->locals[parser->compiler->localCount - 1].depth > 
            parser->compiler->scopeDepth) {
        if (parser->compiler->locals[parser->compiler->localCount - 1].isCaptured) {
            emitByte(parser, OP_CLOSE_UPVALUE);
        } else {
            n++;
        }
        parser->compiler->localCount--;
    }
    emitBytes(parser, OP_POP_N, n);
}

static void expressionStatement(Parser* parser) {
    expression(parser);
    consume(parser, TOKEN_SEMICOLON, "Expected ';' after statement.");
    emitByte(parser, OP_POP);
}

static void and_(Parser* parser, bool canAssign) {
    int andJump = emitJump(parser, OP_JUMP_IF_FALSE);

    emitByte(parser, OP_POP);
    parsePrecedence(parser, PREC_AND);

    patchJump(parser, andJump);
}

static void or_(Parser* parser, bool canAssign) {
    int orJump = emitJump(parser, OP_JUMP_IF_TRUE);

    emitByte(parser, OP_POP);
    parsePrecedence(parser, PREC_OR);

    patchJump(parser, orJump);
}

static void synchronize(Parser* parser) {
    parser->panicMode = false;

    while (parser->current.type != TOKEN_EOF) {
        if (parser->previous.type == TOKEN_SEMICOLON)
            return;
        
        switch (parser->current.type) {
            case TOKEN_CLASS:
            case TOKEN_FN:
            case TOKEN_VAR:
            case TOKEN_LET:
            case TOKEN_CONST:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_RETURN:
                return;
            default:
                break;
        }

        advance(parser);
    }
}

static void method(Parser* parser) {
    bool isDefaultMethod = false;
    uint8_t constant = 0;
    if (match(parser, TOKEN_DEF))
        isDefaultMethod = true;
    
    consume(parser, TOKEN_IDENTIFIER, "Expected method name.");

    if (isDefaultMethod) {
        if (identifierEquals(&parser->previous, "string")) {
            constant = DEFMTH_STRING;
        } else {
            error(parser, "Unknown default method.");
        }
    } else {
        constant = identifierConstant(parser, &parser->previous);
    }

    FunctionType type = FUNC_METHOD;
    function(parser, type);
    emitBytes(parser, OP_METHOD, isDefaultMethod ? 2 : 0);
    emitByte(parser, constant);
}

static void builder(Parser* parser) {
    FunctionType type = FUNC_BUILDER;
    function(parser, type);
    emitBytes(parser, OP_METHOD, 1);
}

static void namedVariable(Parser* parser, Token tok, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(parser, parser->compiler, &tok);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(parser, parser->compiler, &tok)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(parser, &tok);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    TokenType assignmentToken = TOKEN_NULL;
    if (canAssign) {
        if (match(parser, TOKEN_EQUAL)) {
            assignmentToken = TOKEN_EQUAL;
        } else if (match(parser, TOKEN_PLUS_EQUAL)) {
            assignmentToken = TOKEN_PLUS_EQUAL;
        } else if (match(parser, TOKEN_MINUS_EQUAL)) {
            assignmentToken = TOKEN_MINUS_EQUAL;
        } else if (match(parser, TOKEN_STAR_EQUAL)) {
            assignmentToken = TOKEN_STAR_EQUAL;
        } else if (match(parser, TOKEN_SLASH_EQUAL)) {
            assignmentToken = TOKEN_SLASH_EQUAL;
        }
    }

    if (assignmentToken == TOKEN_NULL) {
        emitBytes(parser, getOp, (uint8_t) arg);
        return;
    }


    if (setOp == OP_SET_LOCAL && parser->compiler->locals[arg].fixed) {
        error(parser, "Variable is constant and cannot be modified.");
        return;
    }

    if (assignmentToken != TOKEN_EQUAL)
        emitBytes(parser, getOp, (uint8_t) arg);

    expression(parser);

    switch (assignmentToken) {
        case TOKEN_PLUS_EQUAL:
            emitByte(parser, OP_ADD);
            break;
        
        case TOKEN_MINUS_EQUAL:
            emitByte(parser, OP_SUBTRACT);
            break;
        
        case TOKEN_STAR_EQUAL:
            emitByte(parser, OP_MULTIPLY);
            break;
        
        case TOKEN_SLASH_EQUAL:
            emitByte(parser, OP_DIVIDE);
            break;
        
        case TOKEN_EQUAL:
            break;
        
        default:
            error(parser, "Unhandled assignment token.\n");
            break;
    }

    emitBytes(parser, setOp, (uint8_t) arg);
}

static void variable(Parser* parser, bool canAssign) {
    namedVariable(parser, parser->previous, canAssign);
}

static Token syntheticToken(const char* text) {
    Token tok;
    tok.start = text;
    tok.length = strlen(text);
    return tok;
}

static void classDeclaration(Parser* parser) {
    consume(parser, TOKEN_IDENTIFIER, "Expected identifier after 'class'.");
    Token className = parser->previous;
    uint8_t nameConstant = identifierConstant(parser, &parser->previous);
    declareVariable(parser, true);

    emitBytes(parser, OP_CLASS, nameConstant);
    defineVariable(parser, nameConstant);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = parser->classCompiler;
    parser->classCompiler = &classCompiler;

    if (match(parser, TOKEN_LEFT_ARROW)) {
        consume(parser, TOKEN_IDENTIFIER, "Expecteded superclass name.");
        variable(parser, false);

        if (identifiersEqual(&className, &parser->previous)) {
            error(parser, "Class cannot inherit from itself.");
        }

        beginScope(parser);
        addLocal(parser, syntheticToken("super"), true);
        defineVariable(parser, 0);

        namedVariable(parser, className, false);
        emitByte(parser, OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(parser, className, false);
    consume(parser, TOKEN_LEFT_BRACE, "Expected '{' after class name.");
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
        if (match(parser, TOKEN_FN)) {
            method(parser);
        } else if (match(parser, TOKEN_BUILD)) {
            builder(parser);
        } else {
            advance(parser);
            error(parser, "Expected field, method, or constructor.");
            return;
        }
    }
    consume(parser, TOKEN_RIGHT_BRACE, "Expected '}' after class body.");

    if (classCompiler.hasSuperclass) {
        endScope(parser);
    }

    emitByte(parser, OP_POP);

    parser->classCompiler = classCompiler.enclosing;
}

static void declaration(Parser* parser) {
    if (match(parser, TOKEN_VAR) || match(parser, TOKEN_LET) || match(parser, TOKEN_CONST)) {
        bool constant = parser->previous.type == TOKEN_CONST;
        varDeclaration(parser, constant);
    } else if (match(parser, TOKEN_FN)) {
        funDeclaration(parser);
    } else if (match(parser, TOKEN_CLASS)) {
        classDeclaration(parser);
    } else {
        statement(parser);
    }

    if (parser->panicMode)
        synchronize(parser);
}

static void ifStatement(Parser* parser) {
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' before condition.");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

    int thenJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);
    statement(parser);

    int elseJump = emitJump(parser, OP_JUMP);

    patchJump(parser, thenJump);
    emitByte(parser, OP_POP);

    if (match(parser, TOKEN_ELSE))
        statement(parser);
    
    patchJump(parser, elseJump);
}

static void whileStatement(Parser* parser) {
    beginLoop(parser);

    int loopStart = currentChunk(parser)->count;
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' before condition.");
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

    int exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
    emitByte(parser, OP_POP);
    statement(parser);

    emitLoop(parser, loopStart);

    patchJump(parser, exitJump);
    emitByte(parser, OP_POP);

    endLoop(parser);
}

static void forStatement(Parser* parser) {
    beginScope(parser);
    consume(parser, TOKEN_LEFT_PAREN, "Expected '(' before loop clauses.");

    if (match(parser, TOKEN_VAR) || match(parser, TOKEN_LET)) {
        varDeclaration(parser, false);
    } else if (!match(parser, TOKEN_SEMICOLON)) {
        expressionStatement(parser);
    }

    int loopStart = currentChunk(parser)->count;
    int exitJump = -1;
    if (!match(parser, TOKEN_SEMICOLON)) {
        expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';' after condition clause.");

        exitJump = emitJump(parser, OP_JUMP_IF_FALSE);
        emitByte(parser, OP_POP);
    }

    if (!match(parser, TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(parser, OP_JUMP);

        beginLoop(parser);
        int incrementStart = currentChunk(parser)->count;

        expression(parser);
        emitByte(parser, OP_POP);

        consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after loop clauses.");

        emitLoop(parser, loopStart);
        loopStart = incrementStart;
        patchJump(parser, bodyJump);
    } else {
        beginLoop(parser);
    }

    statement(parser);
    emitLoop(parser, loopStart);

    if (exitJump != -1) {
        patchJump(parser, exitJump);
        emitByte(parser, OP_POP);
    }

    endLoop(parser);
    endScope(parser);
}

static void breakStatement(Parser* parser) {
    if (parser->compiler->breakCount == UINT8_COUNT) {
        error(parser, "Compiler does not support this many break statements in one context.");
    }
    
    if (parser->compiler->loopDepth == 0) {
        error(parser, "Cannot break out of non-loop context.");
        return;
    }

    CodePoint* breakPoint = &parser->compiler->breakPoints[parser->compiler->breakCount++];

    uint8_t n = 0;
    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        if (parser->compiler->locals[i].loopDepth < parser->compiler->loopDepth)
            break;
        n++;
    }
    emitBytes(parser, OP_POP_N, n);

    int idx = emitJump(parser, OP_JUMP);
    breakPoint->code = idx;
    breakPoint->loopDepth = parser->compiler->loopDepth;
    breakPoint->scopeDepth = parser->compiler->scopeDepth;

    consume(parser, TOKEN_SEMICOLON, "Expected ';' after break.");
}

static void continueStatement(Parser* parser) {
    if (parser->compiler->loopDepth == 0) {
        error(parser, "Cannot continue out of non-loop context.");
        return;
    }

    int loopPoint = parser->compiler->loopPoints[parser->compiler->loopDepth - 1].code;

    int n = 0;
    for (int i = parser->compiler->localCount - 1; i >= 0; i--) {
        if (parser->compiler->locals[i].loopDepth < parser->compiler->loopDepth)
            break;
        n++;
    }
    emitBytes(parser, OP_POP_N, n);

    emitLoop(parser, loopPoint);

    consume(parser, TOKEN_SEMICOLON, "Expected ';' after continue.");
}

static void returnStatement(Parser* parser) {
    if (parser->compiler->type == FUNC_SCRIPT) {
        error(parser, "Cannot return from outside of a function.");
    }
    
    if (match(parser, TOKEN_SEMICOLON)) {
        emitReturn(parser);
    } else {
        if (parser->compiler->type == FUNC_BUILDER) {
            error(parser, "Cannot return from an initializer.");
            return;
        }

        expression(parser);
        consume(parser, TOKEN_SEMICOLON, "Expected ';' after expression.");
        emitByte(parser, OP_RETURN);
    }
}

static void statement(Parser* parser) {
    if (match(parser, TOKEN_RETURN)) {
        returnStatement(parser);
    } else if (match(parser, TOKEN_IF)) {
        ifStatement(parser);
    } else if (match(parser, TOKEN_WHILE)) {
        whileStatement(parser);
    } else if (match(parser, TOKEN_FOR)) {
        forStatement(parser);
    } else if (match(parser, TOKEN_BREAK)) {
        breakStatement(parser);
    } else if (match(parser, TOKEN_CONTINUE)) {
        continueStatement(parser);
    } else if (match(parser, TOKEN_LEFT_BRACE)) {
        beginScope(parser);
        block(parser);
        endScope(parser);
    } else {
        expressionStatement(parser);
    }
}

static void grouping(Parser* parser, bool canAssign) {
    expression(parser);
    consume(parser, TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void import(Parser* parser, bool canAssign) {
    consume(parser, TOKEN_IDENTIFIER, "Expected library name after 'import'.");

    uint8_t constant = identifierConstant(parser, &parser->previous);
    emitBytes(parser, OP_IMPORT, constant);
}

static void unary(Parser* parser, bool canAssign) {
    TokenType op = parser->previous.type;

    parsePrecedence(parser, PREC_UNARY);

    switch (op) {
        case TOKEN_MINUS: 
            emitByte(parser, OP_NEGATE);
            break;
        case TOKEN_BANG:
            emitByte(parser, OP_NOT);
            break;
        case TOKEN_UNPACK:
            emitByte(parser, OP_UNPACK);
            break;
        default:
            errorAtCurrent(parser, "UNREACHABLE UNARY OPERATOR ERROR");
    }
}

static void binary(Parser* parser, bool canAssign) {
    TokenType op = parser->previous.type;
    ParseRule* rule = getRule(op);
    parsePrecedence(parser, (Precedence)(rule->prec + 1));

    switch (op) {
        case TOKEN_PLUS:
            emitByte(parser, OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(parser, OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(parser, OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(parser, OP_DIVIDE);
            break;
        case TOKEN_BANG_EQUAL:    
            emitByte(parser, OP_NOT_EQUAL); 
            break;
        case TOKEN_EQUAL_EQUAL:   
            emitByte(parser, OP_EQUAL); 
            break;
        case TOKEN_GREATER:       
            emitByte(parser, OP_GREATER); 
            break;
        case TOKEN_GREATER_EQUAL: 
            emitByte(parser, OP_GREATER_EQUAL); 
            break;
        case TOKEN_LESS:          
            emitByte(parser, OP_LESS); 
            break;
        case TOKEN_LESS_EQUAL:    
            emitByte(parser, OP_LESS_EQUAL); 
            break;
        default:
            errorAtCurrent(parser, "UNREACHABLE BINARY ERROR");
    }
}

static void call(Parser* parser, bool canAssign) {
    uint8_t argCount = argumentList(parser);
    emitBytes(parser, OP_CALL, argCount);
}

static void dot(Parser* parser, bool canAssign) {
    consume(parser, TOKEN_IDENTIFIER, "Expected property name after '.'.");
    uint8_t name = identifierConstant(parser, &parser->previous);

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        expression(parser);
        emitBytes(parser, OP_SET_PROPERTY, name);
    } else if (match(parser, TOKEN_LEFT_PAREN)) {
        uint8_t argc = argumentList(parser);
        emitBytes(parser, OP_INVOKE, name);
        emitByte(parser, argc);
    } else {
        emitBytes(parser, OP_GET_PROPERTY, name);
    }
}

static void indx(Parser* parser, bool canAssign) {
    parsePrecedence(parser, PREC_CALL);
    consume(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after index.");

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        expression(parser);
        emitByte(parser, OP_SET_INDEX);
    } else {
        emitByte(parser, OP_GET_INDEX);
    }
}

static void number(Parser* parser, bool canAssign) {
    double val = strtod(parser->previous.start, NULL);
    emitConstant(parser, NUMBER_VAL(val));
}

static void string(Parser* parser, bool canAssign) {
    // Translate escape sequences
    int stringLength = parser->previous.length - 2;
    char* prevString = parser->previous.start + 1;
    char* newString = (char*) malloc(stringLength + 1);
    if (newString == NULL) {
        errorAtCurrent(parser, "Out of memory.");
    }

    int j = 0;
    for (int i = 0; i < stringLength; i++) {
        if (prevString[i] != '\\') {
            newString[j++] = prevString[i];
            continue;
        }

        i++;
        char newChar = prevString[i];
        switch (newChar) {
            case 'n': newChar = '\n'; break;
            case 't': newChar = '\t'; break;
            case 'b': newChar = '\b'; break;
            case 'r': newChar = '\r'; break;
            case 'a': newChar = '\a'; break;
            case '?': newChar = '\?'; break;
            case 'f': newChar = '\f'; break;
            case 'v': newChar = '\v'; break;
            case '0': newChar = '\0'; break;
        }
        newString[j++] = newChar;
    }
    newString[j] = '\0';

    emitConstant(parser, OBJ_VAL(copyString(parser->vm, newString, strlen(newString))));
    
    free(newString);
    newString = NULL;
}

static void super_(Parser* parser, bool canAssign) {
    if (parser->classCompiler == NULL) {
        error(parser, "Cannot use 'super' outside of a class context.");
    } else if (!parser->classCompiler->hasSuperclass) {
        error(parser, "Cannot use 'super' outside of a subclass context.");
    }

    consume(parser, TOKEN_DOT, "Expected '.' after super.");
    consume(parser, TOKEN_IDENTIFIER, "Expected superclass method name.");
    uint8_t name = identifierConstant(parser, &parser->previous);

    namedVariable(parser, syntheticToken("this"), false);
    if (match(parser, TOKEN_LEFT_PAREN)) {
        uint8_t argc = argumentList(parser);
        namedVariable(parser, syntheticToken("super"), false);
        emitBytes(parser, OP_SUPER_INVOKE, name);
        emitByte(parser, argc);
    } else {
        namedVariable(parser, syntheticToken("super"), false);
        emitBytes(parser, OP_GET_SUPER, name);
    }
}

static void this_(Parser* parser, bool canAssign) {
    if (parser->classCompiler == NULL) {
        error(parser, "'this' cannot be used outside of a class.");
        return;
    }
    
    variable(parser, false);
}

static void literal(Parser* parser, bool canAssign) {
    switch (parser->previous.type) {
        case TOKEN_FALSE:
            emitByte(parser, OP_FALSE);
            break;
        case TOKEN_NULL:
            emitByte(parser, OP_NULL);
            break;
        case TOKEN_TRUE:
            emitByte(parser, OP_TRUE);
            break;
        default:
            errorAtCurrent(parser, "UNREACHABLE LITERAL ERROR");
    }
}

static void list(Parser* parser, bool canAssign) {
    uint8_t argc = valueList(parser, TOKEN_RIGHT_BRACKET, "Expected ']' after list.");
    emitBytes(parser, OP_MAKE_LIST, argc);
}

// Rules && Grammar

ParseRule RULES[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,   PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE}, 
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]           = {NULL,     dot,    PREC_CALL},
    [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
    [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
    [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
    [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
    [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,   PREC_AND},
    [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FN]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IMPORT]        = {import,   NULL,   PREC_NONE},
    [TOKEN_NULL]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]            = {NULL,     or_,    PREC_OR},
    [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]         = {super_,   NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {list,     indx,   PREC_CALL},
    [TOKEN_THIS]          = {this_,    NULL,   PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
    [TOKEN_LET]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_UNPACK]        = {unary,    NULL,   PREC_NONE},
    [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CONST]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
    return &RULES[type];
}

static void parsePrecedence(Parser* parser, Precedence prec) {
    advance(parser);

    ParseFn prefixRule = getRule(parser->previous.type)->prefix;
    if (prefixRule == NULL) {
        error(parser, "Expected expression.");
        return;
    }

    bool canAssign = prec <= PREC_ASSIGNMENT;
    prefixRule(parser, canAssign);

    while (prec <= getRule(parser->current.type)->prec) {
        advance(parser);
        ParseFn infixRule = getRule(parser->previous.type)->infix;
        infixRule(parser, canAssign);
    }

    if (canAssign && match(parser, TOKEN_EQUAL)) {
        error(parser, "Cannot perform assignment here.");
    }
}

ObjFunction* compile(VM* vm, const char* src) {
    Scanner scanner;
    initScanner(&scanner, src);

    Parser parser;
    parser.scanner = &scanner;
    parser.hadError = false;
    parser.panicMode = false;
    parser.vm = vm;
    parser.classCompiler = NULL;
    parser.compiler = NULL;

    Compiler compiler;
    initCompiler(&compiler, &parser, FUNC_SCRIPT);

    advance(&parser);

    while (!match(&parser, TOKEN_EOF)) {
        declaration(&parser);
    }

    consume(&parser, TOKEN_EOF, "Expect end of expression.");

    ObjFunction* func = endCompiler(&parser);

    return parser.hadError ? NULL : func;
}

void markCompilerRoots(VM* vm, Compiler* compiler) {
    while (compiler != NULL) {
        markObject(vm, (Obj*) compiler->function);
        compiler = compiler->enclosing;
    }
}
