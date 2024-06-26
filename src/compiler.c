/*
 * Copyright (C) 2022-2024 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "type.h"
#include "scanner.h"
#include "vmopcodes.h"

typedef struct {
    token_t current;
    token_t previous;
    bool had_error;
    bool panic_mode;
} parser_t;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_ASSIGNMENT_BY, // +=, -=, *=, /=
    PREC_TERNARY,       // ? :
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_BITWISE,       // | & ^
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * / %
    PREC_UNARY,         // ! - ~
    PREC_CALL,          // . () ++ --
    PREC_PRIMARY,
} precedence_t;

typedef void (*parse_fn_t)(const bool);

typedef struct {
    parse_fn_t prefix;
    parse_fn_t infix;
    precedence_t precedence;
} parse_rule_t;

typedef struct {
    token_t name;
    int depth;
    bool is_captured;
} local_t;

typedef struct {
    uint8_t index;
    bool is_local;
} upvalue_t;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,
} function_type_t;

typedef struct compiler {
    local_t locals[UINT8_COUNT];
    upvalue_t upvalues[UINT8_COUNT];
    table_t string_constants;
    struct compiler *enclosing;
    obj_function_t *function;
    function_type_t type;
    int local_count;
    int scope_depth;
} compiler_t;

typedef struct type_compiler {
    struct type_compiler *enclosing;
    bool has_supertype;
} type_compiler_t;

static parser_t parser;
static compiler_t *current = NULL;
static type_compiler_t *current_type = NULL;
static int compiler_count = 0;
static bool compiler_debug = false;

#define MAX_COMPILERS 1024
#define MAX_PARAMETERS 255

static chunk_t *current_chunk(void)
{
    return &current->function->chunk;
}

static void error_at(const token_t *token, const char *message)
{
    if (parser.panic_mode) return;

    parser.panic_mode = true;
    fprintf(stderr, gettext("[line %d] Error"), token->line);
    if (token->type == TOKEN_EOF) {
        fprintf(stderr, gettext(" at end"));
    } else if (token->type == TOKEN_ERROR) {
        // nothing
    } else {
        fprintf(stderr, gettext(" at '%.*s'"), token->length, token->start);
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
        parser.current = scanner_t_scan_token();
        if (parser.current.type != TOKEN_ERROR) break;
        error_at_current(parser.current.start);
    }
}

static void consume(const token_type_t type, const char *message)
{
    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

static bool check(const token_type_t type)
{
    return parser.current.type == type;
}

static bool match(const token_type_t type)
{
    if (!check(type))
        return false;
    advance();
    return true;
}

static void emit_byte(const uint8_t byte)
{
    chunk_t_write(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(const uint8_t byte1, const uint8_t byte2)
{
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_loop(const int loop_start)
{
    emit_byte(OP_LOOP);

    const int offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX)
        error(gettext("Loop body too large."));

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static int emit_jump(const uint8_t instruction)
{
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void emit_return(void)
{
    if (current->type == TYPE_INITIALIZER) {
        emit_bytes(OP_GET_LOCAL, 0); // make sure the instance from slot zero is left on the stack
    } else {
        emit_byte(OP_NIL);
    }
    emit_byte(OP_RETURN);
}

static uint8_t make_constant(const value_t value)
{
    const int constant = chunk_t_add_constant(current_chunk(), value);
    if (constant > UINT8_MAX) {
        error(gettext("Too many constants in one chunk.")); // See OP_CONSTANT_LONG to fix
        return 0;
    }
    return (uint8_t)constant;
}

static void emit_constant(const value_t value)
{
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void patch_jump(const int offset)
{
    const int jump = current_chunk()->count - offset - 2;
    if (jump > UINT16_MAX) {
        error(gettext("Too much code to jump over."));
    }
    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void compiler_t_init(compiler_t *compiler, const function_type_t type)
{
    if (compiler_count >= MAX_COMPILERS) {
        fprintf(stderr, gettext("Too many compilers."));
        exit(EXIT_FAILURE);
    }
    compiler_count++;
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = obj_function_t_allocate();
    table_t_init(&compiler->string_constants);

    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = obj_string_t_copy_from(parser.previous.start, parser.previous.length, true);
    }

    local_t *local = &current->locals[current->local_count++];
    local->depth = 0;
    local->is_captured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = token_keyword_names[TOKEN_SELF];
        local->name.length = TOKEN_SELF_LEN;
      } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static obj_function_t *compiler_t_end(const bool debug)
{
    emit_return();
    table_t_free(&current->string_constants);
    obj_function_t *function_obj = current->function;
    if (debug || parser.had_error) {
        chunk_t_disassemble(current_chunk(), function_obj->name != NULL ? function_obj->name->chars : "<main>");
    }
    current = current->enclosing;
    compiler_count--;
    return function_obj;
}

static void begin_scope(void)
{
    current->scope_depth++;
}

static void end_scope(void)
{
    current->scope_depth--;

    uint8_t to_pop = 0;
    while (current->local_count > 0 && current->locals[current->local_count - 1].depth > current->scope_depth) {
        if (current->locals[current->local_count - 1].is_captured) {
            // flush any to pop before the OP_CLOSE_UPVALUEA
            if (to_pop) {
                emit_bytes(OP_POPN, to_pop);
                to_pop = 0;
            }
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            to_pop++;
        }
        current->local_count--;
    }
    if (to_pop > 0) {
        // flush remaining pops
        emit_bytes(OP_POPN, to_pop);
    }
}

static void expression(void);
static void statement(void);
static void declaration(void);
static const parse_rule_t *get_rule(const token_type_t type);
static void parse_precedence(const precedence_t precedence);

static uint8_t identifier_constant(const token_t *name)
{
    obj_string_t *constant_str = obj_string_t_copy_from(name->start, name->length, true);
    value_t existing_index;
    if (table_t_get(&current->string_constants, OBJ_VAL(constant_str), &existing_index)) {
        return (uint8_t)AS_NUMBER(existing_index);
    }
    uint8_t index = make_constant(OBJ_VAL(constant_str));
    table_t_set(&current->string_constants, OBJ_VAL(constant_str), NUMBER_VAL(index));
    return index;
}

static bool identifiers_equal(const token_t *a, const token_t *b)
{
    if (a->length != b->length)
        return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(compiler_t *compiler, const token_t *name)
{
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        const local_t *local = &compiler->locals[i];
        if (identifiers_equal(name, &local->name)) {
            if (local->depth == -1) {
                error(gettext("Can't read local variable in its own initializer."));
            }
            return i;
        }
    }
    return -1;
}

static int add_upvalue(compiler_t *compiler, const uint8_t index, const bool is_local)
{
    int upvalue_count = compiler->function->upvalue_count;
    for (int i = 0; i < upvalue_count; i++) {
        upvalue_t *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }
    if (upvalue_count == UINT8_COUNT) {
        error(gettext("Too many closure variables in function."));
        return 0;
    }
    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}

static int resolve_upvalue(compiler_t *compiler, const token_t *name)
{
    if (compiler->enclosing == NULL)
        return -1; // must be global

    // try local
    const int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    // try enclosing... in which case every enclosing scope will have
    // it's own upvalue that the enclosed scopes then reference.
    const int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void add_local(const token_t name)
{
    if (current->local_count == UINT8_COUNT) {
        error(gettext("Too many local variables in function."));
        return;
    }
    local_t *local = &current->locals[current->local_count++];
    local->name = name;
    // NOTE here use use a depth of -1 to indicate uninitialized, see mark_initialized and resolve_local
    // local->depth = current->scope_depth;
    local->depth = -1;
    local->is_captured = false;
}

static void declare_variable(void)
{
    if (current->scope_depth == 0)
        return; // handled by defining globals elsewhere
    const token_t *name = &parser.previous;

    for (int i = current->local_count - 1; i >= 0; i--) {
        local_t *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;
        }
        if (identifiers_equal(name, &local->name)) {
            error(gettext("Already a variable with this name in this scope."));
        }
    }
    add_local(*name);
}

static uint8_t parse_variable(const char *message)
{
    consume(TOKEN_IDENTIFIER, message);
    declare_variable();
    if (current->scope_depth > 0) {
        return 0; // dummy index since this isn't a constant
    }
    return identifier_constant(&parser.previous);
}

static void mark_initialized(void)
{
    if (current->scope_depth == 0) {
        return;
    }
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(const uint8_t variable)
{
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_bytes(OP_DEFINE_GLOBAL, variable);
}

static uint8_t argument_list(void)
{
    uint8_t arg_count = 0;
    bool trailing = false; // trailing comma is ok
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            if (match(TOKEN_RIGHT_PAREN)) {
                trailing = true;
                break;
            }
            expression();
            if (arg_count >= MAX_PARAMETERS) {
                error(gettext("Exceeded maximum number of arguments."));
            }
            arg_count++;
        } while (match(TOKEN_COMMA));
    }
    if (!trailing)
        consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after arguments."));
    return arg_count;
}

static void and_expr(const bool) // can_assign
{
    const int end_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);

    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static token_t synthetic_token(const char *text)
{
    token_t token = {0};
    token.start = text;
    token.length = strlen(text);
    return token;
}

static void ternary(const bool)
{
    const int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    expression();

    const int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);

    emit_byte(OP_POP);

    consume(TOKEN_COLON, gettext("Expected colon with expression."));
    expression();

    patch_jump(else_jump);
}

static void binary(const bool)
{
    token_type_t operator_type = parser.previous.type;
    const parse_rule_t *rule = get_rule(operator_type);
    parse_precedence((precedence_t)(rule->precedence + 1));
    switch (operator_type) {
        case TOKEN_BANG_EQUAL: emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emit_byte(OP_EQUAL); break;
        case TOKEN_GREATER: emit_byte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emit_byte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS: emit_byte(OP_ADD); break;
        case TOKEN_MOD: emit_byte(OP_MOD); break;
        case TOKEN_BIT_XOR: emit_byte(OP_BITWISE_XOR); break;
        case TOKEN_BIT_AND: emit_byte(OP_BITWISE_AND); break;
        case TOKEN_BIT_OR: emit_byte(OP_BITWISE_OR); break;
        case TOKEN_SHIFT_LEFT: emit_byte(OP_SHIFT_LEFT); break;
        case TOKEN_SHIFT_RIGHT: emit_byte(OP_SHIFT_RIGHT); break;
        case TOKEN_MINUS: emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR: emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emit_byte(OP_DIVIDE); break;
        default: return; // unreachable
    }
}

static void call(const bool)
{
    const uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

static void map(const bool)
{
    token_t map_token = synthetic_token(KEYWORD_MAP);
    const uint8_t map = identifier_constant(&map_token);
    emit_bytes(OP_GET_GLOBAL, map);
    int arg_count = 0;

    if (!match(TOKEN_RIGHT_BRACE)) {
        do {
            expression();
            match(TOKEN_COLON);
            expression();
            arg_count += 2;
        } while (match(TOKEN_COMMA));
        consume(TOKEN_RIGHT_BRACE, "Expect '}' after expression.");
    }
    emit_bytes(OP_CALL, arg_count);
}

static void list(const bool)
{
    token_t list_token = synthetic_token(KEYWORD_LIST);
    const uint8_t list = identifier_constant(&list_token);
    emit_bytes(OP_GET_GLOBAL, list);

    int arg_count = 0;
    if (!match(TOKEN_RIGHT_BRACKET)) {
        bool trailing = false; // trailing comma is ok
        do {
            if (match(TOKEN_RIGHT_BRACKET)) {
                trailing = true;
                break;
            }
            expression();
            arg_count++;
        } while (match(TOKEN_COMMA));
        if (!trailing)
            consume(TOKEN_RIGHT_BRACKET, "Expect ']' after expression.");
    }
    emit_bytes(OP_CALL, arg_count);
}

static void subscript(const bool can_assign)
{
    uint8_t arg_count = 0;
    expression();
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after expression.");
    arg_count++;

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        arg_count++;
    }
    token_t subscript_token = synthetic_token(KEYWORD_SUBSCRIPT);
    const uint8_t subscript = identifier_constant(&subscript_token);
    emit_bytes(OP_INVOKE, subscript);
    emit_byte(arg_count);
}

static void increment(const bool)
{
    emit_constant(NUMBER_VAL(1));
    emit_byte(OP_ADD);
}

static void decrement(const bool)
{
    emit_constant(NUMBER_VAL(-1));
    emit_byte(OP_ADD);
}

static void number(const bool)
{
    bool cleaned = false;
    char *str = NULL;

    if (memchr(parser.previous.start, '_', parser.previous.length) != NULL || memchr(parser.previous.start, ' ', parser.previous.length) != NULL) {
        const char *n = parser.previous.start;
        str = malloc(sizeof *str * parser.previous.length);
        int offset = 0;
        for (int i = 0; i < parser.previous.length; i++) {
            if (n[i] != '_' && n[i] != ' ') {
                str[offset] = n[i];
                offset++;
            }
        }
        cleaned = true;
        str[offset] = '\0';
    }

    if (str == NULL)
        str = (char *)parser.previous.start;

    if (str[0] == '0' && str[1] == 'x') {
        double value = (double)strtoll(str, NULL, 16);
        emit_constant(NUMBER_VAL(value));
    } else if (str[0] == '0' && str[1] == 'b') {
        double value = (double)strtoll(str + 2, NULL, 2);
        emit_constant(NUMBER_VAL(value));
    } else if (str[0] == '0' && str[1] == 'o') {
        double value = (double)strtoll(str + 2, NULL, 8);
        emit_constant(NUMBER_VAL(value));
    } else {
        const double value = strtod(str, NULL);
        emit_constant(NUMBER_VAL(value));
    }

    if (cleaned)
        free(str);
}

static void string(const bool)
{
    emit_constant(OBJ_VAL(obj_string_t_copy_from(parser.previous.start + 1, parser.previous.length - 2, true)));
}

static bool match_for_load_and_modify(void)
{
    if (match(TOKEN_PLUS_EQUAL) ||
        match(TOKEN_MINUS_EQUAL) ||
        match(TOKEN_STAR_EQUAL) ||
        match(TOKEN_SLASH_EQUAL) ||
        match(TOKEN_AND_EQUAL) ||
        match(TOKEN_OR_EQUAL) ||
        match(TOKEN_XOR_EQUAL) ||
        match(TOKEN_SHIFT_LEFT_EQUAL) ||
        match(TOKEN_SHIFT_RIGHT_EQUAL) ||
        match(TOKEN_PLUS_PLUS) ||
        match(TOKEN_MINUS_MINUS)
    )
        return true;
    return false;
}

static void load_and_modify(const uint8_t name, const token_type_t match, const uint8_t get_op, const uint8_t set_op)
{
    emit_bytes(get_op, name);
    switch (match) {
        case TOKEN_PLUS_EQUAL: expression(); emit_byte(OP_ADD); break;
        case TOKEN_MINUS_EQUAL: expression(); emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR_EQUAL: expression(); emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH_EQUAL: expression(); emit_byte(OP_DIVIDE); break;
        case TOKEN_XOR_EQUAL: expression(); emit_byte(OP_BITWISE_XOR); break;
        case TOKEN_OR_EQUAL: expression(); emit_byte(OP_BITWISE_OR); break;
        case TOKEN_AND_EQUAL: expression(); emit_byte(OP_BITWISE_AND); break;
        case TOKEN_SHIFT_LEFT_EQUAL: expression(); emit_byte(OP_SHIFT_LEFT); break;
        case TOKEN_SHIFT_RIGHT_EQUAL: expression(); emit_byte(OP_SHIFT_RIGHT); break;
        case TOKEN_PLUS_PLUS:
        case TOKEN_MINUS_MINUS:
            emit_constant(NUMBER_VAL(match == TOKEN_PLUS_PLUS ? 1 : -1));
            emit_byte(OP_ADD);
            break;
        default: ;
    }
    emit_bytes(set_op, name);
}

static void subscript_modify_in_place(const int slot, const uint8_t get_op)
{
    uint8_t arg_count = 0;
    expression();
    token_t saved_expression = parser.previous;
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after expression.");
    arg_count++;

    // if we have a load and modify coming
    if (match_for_load_and_modify()) {
        // save it off
        const token_t match_token = parser.previous;

        // emit another invoke to load the current value to modify
        emit_bytes(get_op, (uint8_t)slot);
        if (saved_expression.type == TOKEN_STRING) {
            parser.previous = saved_expression;
            string(true);
        } else if (saved_expression.type == TOKEN_NUMBER) {
            parser.previous = saved_expression;
            number(true);
        } else {
            error(gettext("Invalid subscript."));
        }
        token_t subscript_token = synthetic_token(KEYWORD_SUBSCRIPT);
        const uint8_t subscript_v = identifier_constant(&subscript_token);
        emit_bytes(OP_INVOKE, subscript_v);
        emit_byte(arg_count);

        switch (match_token.type) {
            case TOKEN_PLUS_EQUAL: expression(); emit_byte(OP_ADD); break;
            case TOKEN_MINUS_EQUAL: expression(); emit_byte(OP_SUBTRACT); break;
            case TOKEN_STAR_EQUAL: expression(); emit_byte(OP_MULTIPLY); break;
            case TOKEN_SLASH_EQUAL: expression(); emit_byte(OP_DIVIDE); break;
            case TOKEN_XOR_EQUAL: expression(); emit_byte(OP_BITWISE_XOR); break;
            case TOKEN_OR_EQUAL: expression(); emit_byte(OP_BITWISE_OR); break;
            case TOKEN_AND_EQUAL: expression(); emit_byte(OP_BITWISE_AND); break;
            case TOKEN_SHIFT_LEFT_EQUAL: expression(); emit_byte(OP_SHIFT_LEFT); break;
            case TOKEN_SHIFT_RIGHT_EQUAL: expression(); emit_byte(OP_SHIFT_RIGHT); break;
            case TOKEN_PLUS_PLUS:
            case TOKEN_MINUS_MINUS:
                emit_constant(NUMBER_VAL(match_token.type == TOKEN_PLUS_PLUS ? 1 : -1));
                emit_byte(OP_ADD);
                break;
            default: ;

        }
        arg_count++;
    }
    else if (match(TOKEN_EQUAL)) {
        expression();
        arg_count++;
    }

    token_t subscript_token = synthetic_token(KEYWORD_SUBSCRIPT);
    const uint8_t subscript_v = identifier_constant(&subscript_token);
    emit_bytes(OP_INVOKE, subscript_v);
    emit_byte(arg_count);
}

static void named_variable(const token_t name, const bool can_assign)
{
    uint8_t get_op, set_op;
    int arg = resolve_local(current, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &name)) != -1){
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(set_op, (uint8_t)arg);
    } else if (can_assign && match_for_load_and_modify()) {
        load_and_modify(arg, parser.previous.type, get_op, set_op);
    } else if (can_assign && match(TOKEN_LEFT_BRACKET)) {
        emit_bytes(get_op, (uint8_t)arg);
        subscript_modify_in_place(arg, get_op);
    } else {
        emit_bytes(get_op, (uint8_t)arg);
    }
}

static void dot(const bool can_assign)
{
    consume(TOKEN_IDENTIFIER, gettext("Expect property name after '.'."));
    const uint8_t name = identifier_constant(&parser.previous);

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) { // optimization here since we are immediately calling the method
        const uint8_t arg_count = argument_list();
        emit_bytes(OP_INVOKE, name);
        emit_byte(arg_count);
    } else if (can_assign && match_for_load_and_modify()) {
        named_variable(synthetic_token(token_keyword_names[TOKEN_SELF]), false);
        load_and_modify(name, parser.previous.type, OP_GET_PROPERTY, OP_SET_PROPERTY);
    } else if (can_assign && match(TOKEN_LEFT_BRACKET)) {
        emit_bytes(OP_GET_PROPERTY, (uint8_t)name);
        subscript_modify_in_place(name, OP_GET_PROPERTY);
    } else {
        emit_bytes(OP_GET_PROPERTY, name);
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

static void grouping(const bool)
{
    expression();
    consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after expression."));
}

static void or_expr(const bool) // can_assign
{
    const int else_jump = emit_jump(OP_JUMP_IF_FALSE);
    const int end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);

    parse_precedence(PREC_OR);
    patch_jump(end_jump);
}

static void variable(const bool can_assign)
{
    named_variable(parser.previous, can_assign);
}

static void super_expr(const bool)
{
    if (current_type == NULL) {
        error(gettext("Can't use 'super' outside of a type."));
    } else if (!current_type->has_supertype) {
        error(gettext("Can't use 'super' in a type with no supertype."));
    }

    consume(TOKEN_DOT, gettext("Expect '.' after 'super'."));
    consume(TOKEN_IDENTIFIER, gettext("Expect supertype method name."));
    const uint8_t method_name = identifier_constant(&parser.previous);

    // capture self and super in case we are in a closure
    named_variable(synthetic_token(token_keyword_names[TOKEN_SELF]), false);

    // try a fast path
    if (match(TOKEN_LEFT_PAREN)) {
        const uint8_t arg_count = argument_list();
        named_variable(synthetic_token(token_keyword_names[TOKEN_SUPER]), false);
        emit_bytes(OP_SUPER_INVOKE, method_name);
        emit_byte(arg_count);
    } else { // slow path
        named_variable(synthetic_token(token_keyword_names[TOKEN_SUPER]), false);
        emit_bytes(OP_GET_SUPER, method_name);
    }
}

static void self_expr(const bool)
{
    // check we have a setup from type_declaration
    if (current_type == NULL) {
        error(gettext("Can't use 'self' outside of a type."));
        return;
    }
    variable(false);
}

static void unary(const bool)
{
    const token_type_t operator_type = parser.previous.type;

    // compile the operand
    parse_precedence(PREC_UNARY);

    // emit the operator instruction
    switch (operator_type) {
        case TOKEN_BANG: emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        case TOKEN_BIT_NOT : emit_byte(OP_BITWISE_NOT); break;
        default: return; // unreachable
    }
}

const parse_rule_t rules[] = {
    [TOKEN_LEFT_PAREN]          = {grouping,    call,       PREC_CALL},
    [TOKEN_RIGHT_PAREN]         = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LEFT_BRACE]          = {map,         NULL,       PREC_NONE},
    [TOKEN_RIGHT_BRACE]         = {NULL,        NULL,       PREC_NONE},
    [TOKEN_COMMA]               = {NULL,        NULL,       PREC_NONE},
    [TOKEN_DOT]                 = {NULL,        dot,        PREC_CALL},
    [TOKEN_BIT_OR]              = {NULL,        binary,     PREC_BITWISE},
    [TOKEN_OR_EQUAL]            = {NULL,        NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_BIT_AND]             = {NULL,        binary,     PREC_BITWISE},
    [TOKEN_AND_EQUAL]           = {NULL,        NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_BIT_XOR]             = {NULL,        binary,     PREC_BITWISE},
    [TOKEN_XOR_EQUAL]           = {NULL,        NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_QUESTION_MARK]       = {NULL,        ternary,    PREC_TERNARY},
    [TOKEN_SHIFT_LEFT]          = {NULL,        binary,     PREC_BITWISE},
    [TOKEN_SHIFT_LEFT_EQUAL]    = {NULL,        NULL,       PREC_BITWISE},
    [TOKEN_SHIFT_RIGHT]         = {NULL,        binary,     PREC_BITWISE},
    [TOKEN_SHIFT_RIGHT_EQUAL]   = {NULL,        NULL,       PREC_BITWISE},
    [TOKEN_LEFT_BRACKET]        = {list,        subscript,  PREC_CALL},
    [TOKEN_RIGHT_BRACKET]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_MINUS]               = {unary,       binary,     PREC_TERM},
    [TOKEN_MINUS_EQUAL]         = {unary,       NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_MINUS_MINUS]         = {NULL,        decrement,  PREC_CALL},
    [TOKEN_PLUS]                = {NULL,        binary,     PREC_TERM},
    [TOKEN_PLUS_EQUAL]          = {NULL,        NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_PLUS_PLUS]           = {NULL,        increment,  PREC_CALL},
    [TOKEN_MOD]                 = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_SEMICOLON]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SLASH]               = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_SLASH_EQUAL]         = {NULL,        NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_STAR]                = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_STAR_EQUAL]          = {NULL,        NULL,       PREC_ASSIGNMENT_BY},
    [TOKEN_BANG]                = {unary,       NULL,       PREC_UNARY},
    [TOKEN_BIT_NOT]             = {unary,       NULL,       PREC_UNARY},
    [TOKEN_BANG_EQUAL]          = {NULL,        binary,     PREC_EQUALITY},
    [TOKEN_EQUAL]               = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EQUAL_EQUAL]         = {NULL,        binary,     PREC_EQUALITY},
    [TOKEN_GREATER]             = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]       = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_LESS]                = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]          = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_IDENTIFIER]          = {variable,    NULL,       PREC_NONE},
    [TOKEN_STRING]              = {string,      NULL,       PREC_NONE},
    [TOKEN_NUMBER]              = {number,      NULL,       PREC_NONE},
    [TOKEN_AND]                 = {NULL,        and_expr,   PREC_AND},
    [TOKEN_TYPE]                = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ELSE]                = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FALSE]               = {literal,     NULL,       PREC_NONE},
    [TOKEN_FOR]                 = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FN]                  = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IF]                  = {NULL,        NULL,       PREC_NONE},
    [TOKEN_NIL]                 = {literal,     NULL,       PREC_NONE},
    [TOKEN_OR]                  = {NULL,        or_expr,    PREC_OR},
    [TOKEN_PRINT]               = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RETURN]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SUPER]               = {super_expr,  NULL,       PREC_NONE},
    [TOKEN_SELF]                = {self_expr,   NULL,       PREC_NONE},
    [TOKEN_TRUE]                = {literal,     NULL,       PREC_NONE},
    [TOKEN_LET]                 = {NULL,        NULL,       PREC_NONE},
    [TOKEN_WHILE]               = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ERROR]               = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EXIT]                = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ASSERT]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EOF]                 = {NULL,        NULL,       PREC_NONE},
};

static void parse_precedence(const precedence_t precedence)
{
    advance();
    const parse_fn_t prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error(gettext("Expect expression."));
        return;
    }

    const bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        const parse_fn_t infix_rule = get_rule(parser.previous.type)->infix;
        if (infix_rule == NULL) {
            error(gettext("Expect expression."));
            return;
        }
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error(gettext("Invalid assignment target."));
    }
}

static const parse_rule_t *get_rule(const token_type_t type)
{
    return &rules[type];
}

static void expression(void)
{
    parse_precedence(PREC_ASSIGNMENT);
}

static void block(void)
{
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, gettext("Expect '}' after block."));
    match(TOKEN_SEMICOLON);
}

static void function(function_type_t type)
{
    compiler_t compiler;
    compiler_t_init(&compiler, type);
    begin_scope();

    consume(TOKEN_LEFT_PAREN, gettext("Expect '(' after function name."));
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > MAX_PARAMETERS) {
                error_at_current(gettext("Exceeded maximum number of parameters."));
            }
            const uint8_t constant = parse_variable(gettext("Expect parameter name."));
            define_variable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after parameters."));
    consume(TOKEN_LEFT_BRACE, gettext("Expect '{' before function body."));
    block();

    obj_function_t *function = compiler_t_end(compiler_debug); // no end_scope required here
    emit_bytes(OP_CLOSURE, make_constant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalue_count; i++) {
        emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(compiler.upvalues[i].index);
    }
}

static void field(void)
{
    consume(TOKEN_IDENTIFIER, gettext("Expect field name."));
    const uint8_t field_name = identifier_constant(&parser.previous);
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after field declaration."));
    emit_bytes(OP_FIELD, field_name);
}

static void method(void)
{
    consume(TOKEN_IDENTIFIER, gettext("Expect method name."));
    const uint8_t constant = identifier_constant(&parser.previous);

    function_type_t type = TYPE_METHOD;
    if (parser.previous.length == KEYWORD_INIT_LEN && memcmp(parser.previous.start, KEYWORD_INIT, KEYWORD_INIT_LEN) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);

    emit_bytes(OP_METHOD, constant);
}

static void fun_declaration(void)
{
    const uint8_t global = parse_variable(gettext("Expect function name."));
    mark_initialized(); // so we can support recursion before we compile the body
    function(TYPE_FUNCTION);
    define_variable(global);
}

static void var_declaration(void)
{
    const uint8_t global = parse_variable(gettext("Expect variable name."));
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }

    consume(TOKEN_SEMICOLON, gettext("Expect ';' after variable declaration."));
    define_variable(global);
}

static void type_declaration(void)
{
    consume(TOKEN_IDENTIFIER, gettext("Expect type name."));
    const token_t type_name = parser.previous;
    const uint8_t name_constant = identifier_constant(&parser.previous);
    declare_variable();

    emit_bytes(OP_TYPE, name_constant);
    define_variable(name_constant);

    // setup a type compiler instance while we do the work
    type_compiler_t type_compiler;
    type_compiler.enclosing = current_type;
    type_compiler.has_supertype = false;
    current_type = &type_compiler;

    if (match(TOKEN_LEFT_PAREN)) {
        consume(TOKEN_IDENTIFIER, gettext("Expect supertype name."));
        variable(false);

        if (identifiers_equal(&type_name, &parser.previous)) {
            error(gettext("A type can't inherit from itself."));
        }
        consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after supertype."));

        // setup super... make a new scope so each type gets local slot for super, preventing collisions
        begin_scope();
        add_local(synthetic_token(token_keyword_names[TOKEN_SUPER]));
        define_variable(0);

        named_variable(type_name, false);
        emit_byte(OP_INHERIT);
        type_compiler.has_supertype = true;
    }

    named_variable(type_name, false); // get self back on the stack as a named variable before we compile methods

    consume(TOKEN_LEFT_BRACE, gettext("Expect '{' before type body."));
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        // could add fields and other things here besides methods
        if (match(TOKEN_LET)) {
            field();
        } else if (match(TOKEN_FN)) {
            method();
        } else {
            error(gettext("Expect field or method in the type body."));
            return;
        }
    }
    consume(TOKEN_RIGHT_BRACE, gettext("Expect '}' after type body."));
    match(TOKEN_SEMICOLON);

    emit_byte(OP_POP); // done with our type, pop it off the stack

    if (type_compiler.has_supertype) {
        end_scope();
    }

    // pop off the type compiler
    current_type = current_type->enclosing;
}

static void expression_statement(void)
{
    // ignore empty expressions
    if (match(TOKEN_SEMICOLON)) {
        return;
    }
    expression();
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after expression."));
    emit_byte(OP_POP);
}

int inner_most_loop_start = -1;
int inner_most_loop_end = -1;
int inner_most_loop_scope_depth = 0;

static void for_statement(void)
{
    int surrounding_loop_start = inner_most_loop_start;
    int surrounding_loop_end = inner_most_loop_end;
    int surrounding_loop_scope_depth = inner_most_loop_scope_depth;

    begin_scope();

    consume(TOKEN_LEFT_PAREN, gettext("Expect '(' after 'for'."));
    if (match(TOKEN_SEMICOLON)) {
        // no loop initializer
    } else if (match(TOKEN_LET)) {
        var_declaration();
    } else {
        expression_statement();
    }

    inner_most_loop_start = current_chunk()->count;
    inner_most_loop_scope_depth = current->scope_depth;

    int exit_jump = -1;
    if (!match(TOKEN_SEMICOLON)) { // loop condition
        expression();
        consume(TOKEN_SEMICOLON, gettext("Expect ';' after loop condition."));

        // jump out of the loop if the condition is false
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP); // condition
    }

    if (!match(TOKEN_RIGHT_PAREN)) { // increment clause
        int body_jump = emit_jump(OP_JUMP);
        int increment_start = current_chunk()->count;

        expression();

        emit_byte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after for clauses."));

        emit_loop(inner_most_loop_start);
        inner_most_loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(inner_most_loop_start);

    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    if (inner_most_loop_end != -1) {
        patch_jump(inner_most_loop_end);
    }

    inner_most_loop_start = surrounding_loop_start;
    inner_most_loop_end = surrounding_loop_end;
    inner_most_loop_scope_depth = surrounding_loop_scope_depth;
    end_scope();
}

static void if_statement(void)
{
    consume(TOKEN_LEFT_PAREN, gettext("Expect '(' after 'if'."));
    expression();
    consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after condition."));

    const int then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    const int else_jump = emit_jump(OP_JUMP);

    patch_jump(then_jump);

    emit_byte(OP_POP);

    if (match(TOKEN_ELSE))
        statement();

    patch_jump(else_jump);
}

static void print_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after value."));
    emit_byte(OP_PRINT);
}

static void perror_statement(void)
{
    expression();
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after value."));
    emit_byte(OP_ERROR);
}

static void return_statement(void)
{
    if (current->type == TYPE_SCRIPT) {
        error(gettext("Can't return from top-level code."));
    }

    if (match(TOKEN_SEMICOLON)) {
        emit_return();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error(gettext("Can't return a value from an initializer."));
            // let it continue from here so that the compiler can synchronize
        }
        expression();
        consume(TOKEN_SEMICOLON, gettext("Expect ';' after return value."));
        emit_byte(OP_RETURN);
    }
}

static void while_statement(void)
{
    int surrounding_loop_start = inner_most_loop_start;
    int surrounding_loop_end = inner_most_loop_end;
    int surrounding_loop_scope_depth = inner_most_loop_scope_depth;

    inner_most_loop_start = current_chunk()->count;
    inner_most_loop_scope_depth = current->scope_depth;

    consume(TOKEN_LEFT_PAREN, gettext("Expect '(' after 'while'."));
    expression();
    consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after condition."));

    const int exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();

    emit_loop(inner_most_loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);

    if (inner_most_loop_end != -1) {
        patch_jump(inner_most_loop_end);
    }

    inner_most_loop_start = surrounding_loop_start;
    inner_most_loop_end = surrounding_loop_end;
    inner_most_loop_scope_depth = surrounding_loop_scope_depth;
}

static void synchronize(void)
{
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        switch (parser.current.type) {
            case TOKEN_TYPE:
            case TOKEN_FN:
            case TOKEN_LET:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_SWITCH:
            case TOKEN_PRINT:
            case TOKEN_PERROR:
            case TOKEN_RETURN:
                return;
            default:
                ; // do nothing
        }
        advance();
    }
}

static void declaration(void)
{
    if (match(TOKEN_TYPE)) {
        type_declaration();
    } else if (match(TOKEN_FN)) {
        fun_declaration();
    } else if (match(TOKEN_LET)) {
        var_declaration();
    } else {
        statement();
    }
    if (parser.panic_mode) synchronize();
}

#define MAX_CASES 256

static void switch_statement(void)
{
    consume(TOKEN_LEFT_PAREN, gettext("Expect '(' after 'switch'."));
    expression();
    consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after value."));
    consume(TOKEN_LEFT_BRACE, gettext("Expect '{' before switch cases."));

    int case_ends[MAX_CASES];
    int case_count = 0;
    bool seen_default = false;

    while (!match(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        if (seen_default) {
            error_at_current(gettext("Can't have another case or default after the default case."));
        }

        int jump = -1;
        if (match(TOKEN_CASE)) {
            if (case_count == MAX_CASES) {
                error(gettext("Too many case statements."));
                return;
            }
            emit_byte(OP_DUP); // dup the switch value to compare against
            expression();
            consume(TOKEN_COLON, gettext("Expect ':' after case value."));
            emit_byte(OP_EQUAL);
            jump = emit_jump(OP_JUMP_IF_FALSE);
            emit_byte(OP_POP); // Pop the comparison result.
        } else {
            consume(TOKEN_DEFAULT, gettext("Expect 'case' or 'default'."));
            consume(TOKEN_COLON, gettext("Expect ':' after default."));
            seen_default = true;
        }

        while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_CASE) && !check(TOKEN_DEFAULT)) {
            statement();
        }

        case_ends[case_count++] = emit_jump(OP_JUMP);

        if (jump != -1) {
            patch_jump(jump);
            emit_byte(OP_POP);
        }
    }

    // Patch all the case jumps to the end.
    for (int i = 0; i < case_count; i++) {
        patch_jump(case_ends[i]);
    }

    emit_byte(OP_POP); // The switch value.
}
#undef MAX_CASES

static void break_statement(void)
{
    if (inner_most_loop_start == -1) {
        error(gettext("Can't use 'break' outside of a loop."));
    }
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after 'break'."));

    uint8_t pop_count = 0;
    for (int i = current->local_count - 1; i >=0 && current->locals[i].depth > inner_most_loop_scope_depth; i--) {
        pop_count++;
    }
    emit_bytes(OP_POPN, pop_count);
    inner_most_loop_end = emit_jump(OP_JUMP);
}

static void continue_statement(void)
{
    if (inner_most_loop_start == -1) {
        error(gettext("Can't use 'continue' outside of a loop."));
    }
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after 'continue'."));

    uint8_t pop_count = 0;
    for (int i = current->local_count - 1; i >=0 && current->locals[i].depth > inner_most_loop_scope_depth; i--) {
        pop_count++;
    }
    emit_bytes(OP_POPN, pop_count);
    emit_loop(inner_most_loop_start);
}

static void exit_statement(void)
{
    const double exit_value = 0;
    if (match(TOKEN_LEFT_PAREN)) {
        expression();
        consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after exit expression."));
        consume(TOKEN_SEMICOLON, gettext("Expect ';' after 'exit'."));
    } else {
        consume(TOKEN_SEMICOLON, gettext("Expect ';' after 'exit'."));
        emit_constant(NUMBER_VAL(exit_value));
    }
    emit_byte(OP_EXIT);
}

static void assert_statement(void)
{
    consume(TOKEN_LEFT_PAREN, gettext("Expect '(' after 'assert'."));
    expression();
    consume(TOKEN_RIGHT_PAREN, gettext("Expect ')' after condition."));
    consume(TOKEN_SEMICOLON, gettext("Expect ';' after 'assert'."));

    const int fail_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    const int succeed_jump = emit_jump(OP_JUMP);
    patch_jump(fail_jump);

    char errbuf[255];
    snprintf(errbuf, 255, "[line %d] Assertion failed", parser.current.line);
    obj_string_t *constant_str = obj_string_t_copy_from(errbuf, strlen(errbuf), true);
    emit_bytes(OP_CONSTANT, make_constant(OBJ_VAL(constant_str)));
    emit_byte(OP_PRINT);

    emit_constant(NUMBER_VAL(-1));
    emit_byte(OP_EXIT);
    patch_jump(succeed_jump);
}

static void statement(void)
{
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else if (match(TOKEN_PERROR)) {
        perror_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_RETURN)) {
        return_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        begin_scope();
        block();
        end_scope();
    } else if (match(TOKEN_ASSERT)) {
        assert_statement();
    } else if (match(TOKEN_EXIT)) {
        exit_statement();
    } else if (match(TOKEN_SWITCH)) {
        switch_statement();
    } else if (match(TOKEN_BREAK)) {
        break_statement();
    } else if (match(TOKEN_CONTINUE)) {
        continue_statement();
    } else {
        expression_statement();
    }
}

obj_function_t *compiler_t_compile(const char *source, const bool debug)
{
    scanner_t_init(source);

    compiler_t compiler;
    compiler_t_init(&compiler, TYPE_SCRIPT);

    parser.had_error = false;
    parser.panic_mode = false;
    compiler_debug = debug;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    obj_function_t *function_obj = compiler_t_end(debug);
    return parser.had_error ? NULL : function_obj;
}

void compiler_t_mark_roots(void)
{
    compiler_t *compiler = current;
    while (compiler != NULL) {
        obj_t_mark((obj_t*)compiler->function);
        compiler = compiler->enclosing;
    }
}
#undef MAX_COMPILERS
#undef MAX_PARAMETERS
