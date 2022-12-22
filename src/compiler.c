#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#ifdef DEBUG
#include "debug.h"
#endif
#include "scanner.h"

typedef struct {
    token_t current;
    token_t previous;
    bool had_error;
    bool panic_mode;
} parser_t;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       //.  ()
    PREC_PRIMARY,
} precedence_t;

typedef void (*parse_fn_t)(const bool);

typedef struct {
    parse_fn_t prefix;
    parse_fn_t infix;
    precedence_t precedence;
} parse_rule_t;

parser_t parser;
chunk_t *compiling_chunk;

static uint8_t identifier_constant(token_t *name);
static void grouping(const bool _);
static void unary(const bool _);
static void binary(const bool _);
static void number(const bool _);
static void literal(const bool _);
static void string(const bool _);
static void variable(const bool can_assign);
parse_rule_t rules[] = {
    [TOKEN_LEFT_PAREN]      = {grouping,    NULL,   PREC_NONE},
    [TOKEN_RIGHT_PAREN]     = {NULL,        NULL,   PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,        NULL,   PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,        NULL,   PREC_NONE},
    [TOKEN_COMMA]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_DOT]             = {NULL,        NULL,   PREC_NONE},
    [TOKEN_MINUS]           = {unary,       binary, PREC_TERM},
    [TOKEN_PLUS]            = {NULL,        binary, PREC_TERM},
    [TOKEN_SEMICOLON]       = {NULL,        NULL,   PREC_NONE},
    [TOKEN_SLASH]           = {NULL,        binary, PREC_FACTOR},
    [TOKEN_STAR]            = {NULL,        binary, PREC_FACTOR},
    [TOKEN_BANG]            = {unary,       NULL,   PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,        binary, PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,        binary, PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,        binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,        binary, PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL,        binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,        binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER]      = {variable,    NULL,   PREC_NONE},
    [TOKEN_STRING]          = {string,      NULL,   PREC_NONE},
    [TOKEN_NUMBER]          = {number,      NULL,   PREC_NONE},
    [TOKEN_AND]             = {NULL,        NULL,   PREC_NONE},
    [TOKEN_CLASS]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_ELSE]            = {NULL,        NULL,   PREC_NONE},
    [TOKEN_FALSE]           = {literal,     NULL,   PREC_NONE},
    [TOKEN_FOR]             = {NULL,        NULL,   PREC_NONE},
    [TOKEN_FUN]             = {NULL,        NULL,   PREC_NONE},
    [TOKEN_IF]              = {NULL,        NULL,   PREC_NONE},
    [TOKEN_NIL]             = {literal,     NULL,   PREC_NONE},
    [TOKEN_OR]              = {NULL,        NULL,   PREC_NONE},
    [TOKEN_PRINT]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_RETURN]          = {NULL,        NULL,   PREC_NONE},
    [TOKEN_SUPER]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_THIS]            = {NULL,        NULL,   PREC_NONE},
    [TOKEN_TRUE]            = {literal,     NULL,   PREC_NONE},
    [TOKEN_VAR]             = {NULL,        NULL,   PREC_NONE},
    [TOKEN_WHILE]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_ERROR]           = {NULL,        NULL,   PREC_NONE},
    [TOKEN_EOF]             = {NULL,        NULL,   PREC_NONE},
};

static void error_at(token_t *token, const char *message)
{
    if (parser.panic_mode) return;

    parser.panic_mode = true;
    fprintf(stderr, "[line %d] Error", token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char *message)
{
    error_at(&parser.previous, message);
}

static void error_at_current(const char *message)
{
    error_at(&parser.current, message);
}

static void advance(void)
{
    parser.previous = parser.current;
    for (;;) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }
}

static void consume(token_type_t type, const char *message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

static chunk_t *current_chunk(void)
{
    return compiling_chunk;
}

static void emit_byte(const uint8_t byte)
{
    write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_return(void)
{
    emit_byte(OP_RETURN);
}

static void end_compiler(void)
{
    emit_return();
    #ifdef DEBUG
    if (!parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
    #endif
}

static void emit_bytes(const uint8_t byte1, const uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static uint8_t make_constant(const value_t value)
{
    int constant = add_constant(current_chunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk."); // See OP_CONSTANT_LONG to fix
        return 0;
    }
    return (uint8_t)constant;
}

static void emit_constant(const value_t value)
{
    emit_bytes(OP_CONSTANT, make_constant(value));
}

void number(const bool)
{
    const double value = strtod(parser.previous.start, NULL);
    emit_constant(NUMBER_VAL(value));
}

static parse_rule_t *get_rule(const token_type_t type)
{
    return &rules[type];
}

static bool match(token_type_t type);
static void parse_precedence(const precedence_t precedence)
{
    advance();
    parse_fn_t prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        parse_fn_t infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static void binary(const bool)
{
    token_type_t operator_type = parser.previous.type;
    parse_rule_t *rule = get_rule(operator_type);
    parse_precedence((precedence_t)(rule->precedence + 1));
    switch (operator_type) {
        case TOKEN_BANG_EQUAL: emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER: emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        default: return; // unreachable
    }
}

static void expression(void)
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void grouping(const bool)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(const bool)
{
    token_type_t operator_type = parser.previous.type;

    // compile the operand
    parse_precedence(PREC_UNARY);

    // emit the operator instruction
    switch (operator_type) {
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default: return; // unreachable
    }
}

static void literal(const bool)
{
    switch (parser.previous.type) {
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        case TOKEN_NIL: emit_byte(OP_NIL); break;
        case TOKEN_TRUE: emit_byte(OP_TRUE); break;
        default: return; // unreachable
    }
}

static void string(const bool)
{
    emit_constant(OBJ_VAL(copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static bool check(token_type_t type) {
    return parser.current.type == type;
}

static bool match(token_type_t type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

static void named_variable(token_t name, const bool can_assign)
{
    uint8_t arg = identifier_constant(&name);
    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(OP_SET_GLOBAL, arg);
    } else {
        emit_bytes(OP_GET_GLOBAL, arg);
    }
}

static void variable(const bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static void print_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void expression_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void statement(void)
{
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else {
        expression_statement();
    }
}

static void synchronize(void)
{
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ; // do nothing
        }
    }
    advance();
}

static uint8_t identifier_constant(token_t *name)
{
    return make_constant(OBJ_VAL(copy_string(name->start, name->length)));
}

static uint8_t parse_variable(const char *message)
{
    consume(TOKEN_IDENTIFIER, message);
    return identifier_constant(&parser.previous);
}

static void define_variable(const uint8_t variable)
{
    emit_bytes(OP_DEFINE_GLOBAL, variable);
}

static void var_declaration(void)
{
    uint8_t global = parse_variable("Expect variable name.");
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    define_variable(global);
}

static void declaration(void)
{
    if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }
    if (parser.panic_mode) synchronize();
}

bool compile(const char *source, chunk_t *chunk)
{
    init_scanner(source);
    parser.had_error = false;
    parser.panic_mode = false;
    compiling_chunk = chunk;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    end_compiler();
    return !parser.had_error;
}
