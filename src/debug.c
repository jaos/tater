#include <assert.h>
#include <stdio.h>
#include "debug.h"
#include "object.h"
#include "compiler.h"
#include "scanner.h"

void chunk_t_disassemble(const chunk_t *chunk, const char *name)
{
    printf(gettext("== start %s ==\n"), name);
    for (int offset = 0; offset < chunk->count;) {
        offset = chunk_t_disassemble_instruction(chunk, offset);
    }
    printf(gettext("==   end %s ==\n"), name);
}

static int constant_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    assert(chunk->count > 0);
    const uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    assert(chunk->constants.count >= constant);
    value_t_print(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int invoke_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    assert(chunk->count > 0);
    const uint8_t constant = chunk->code[offset + 1];
    const uint8_t arg_count = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, arg_count, constant);
    assert(chunk->constants.count >= constant);
    value_t_print(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}

static int simple_instruction(const char *name, const int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int byte_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    // TODO figure out how to map our chunk to the compiler locals to get the variable name
    assert(chunk->count > 0);
    const uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char *name, const int sign, const chunk_t *chunk, const int offset)
{
    assert(chunk->count > 0);
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

static int long_constant_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    assert(chunk->count > 0);
    const uint32_t constant = chunk->code[offset + 1] |
        (chunk->code[offset + 2] << 8) |
        (chunk->code[offset + 3] << 16);
    printf("%-16s %4d '", name, constant);
    value_t_print(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

int chunk_t_disassemble_instruction(const chunk_t *chunk, int offset)
{
    printf("%04d ", offset);

    int line = chunk_t_get_line(chunk, offset);
    if (offset > 0 && line == chunk_t_get_line(chunk, offset - 1)) {
        printf("     | ");
    } else {
        printf("%4d ", line);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT: return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_NIL: return simple_instruction("OP_NIL", offset);
        case OP_TRUE: return simple_instruction("OP_TRUE", offset);
        case OP_FALSE: return simple_instruction("OP_FALSE", offset);
        case OP_POP: return simple_instruction("OP_POP", offset);
        case OP_GET_LOCAL: return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL: return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL: return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL: return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL: return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE: return byte_instruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE: return byte_instruction("OP_SET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY: return constant_instruction("OP_GET_PROPERTY", chunk, offset);
        case OP_SET_PROPERTY: return constant_instruction("OP_SET_PROPERTY", chunk, offset);
        case OP_GET_SUPER: return constant_instruction("OP_GET_SUPER", chunk, offset);
        case OP_EQUAL: return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER: return simple_instruction("OP_GREATER", offset);
        case OP_LESS: return simple_instruction("OP_LESS", offset);
        case OP_ADD: return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT: return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY: return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE: return simple_instruction("OP_DIVIDE", offset);
        case OP_NOT: return simple_instruction("OP_NOT", offset);
        case OP_NEGATE: return simple_instruction("OP_NEGATE", offset);
        case OP_PRINT: return simple_instruction("OP_PRINT", offset);
        case OP_JUMP: return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP: return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL: return byte_instruction("OP_CALL", chunk, offset);
        case OP_INVOKE: return invoke_instruction("OP_INVOKE", chunk, offset);
        case OP_SUPER_INVOKE: return invoke_instruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            value_t_print(chunk->constants.values[constant]);
            printf("\n");

            obj_function_t *function = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalue_count; j++) {
                int is_local = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
            }
            return offset;
        }
        case OP_CLOSE_UPVALUE: return simple_instruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN: return simple_instruction("OP_RETURN", offset);
        case OP_EXIT: return simple_instruction("OP_EXIT", offset);
        case OP_ASSERT: return jump_instruction("OP_ASSERT", 1, chunk, offset);
        case OP_CLASS: return constant_instruction("OP_CLASS", chunk, offset);
        case OP_INHERIT: return simple_instruction("OP_INHERIT", offset);
        case OP_METHOD: return constant_instruction("OP_METHOD", chunk, offset);
        case OP_CONSTANT_LONG: return long_constant_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_POPN: return byte_instruction("OP_POPN", chunk, offset);
        case OP_DUP: return simple_instruction("OP_DUP", offset);
        default: {
            printf(gettext("Unknown opcode %d\n"), instruction);
            return offset + 1;
        }
    }
}

const char *op_code_t_to_str(const op_code_t op)
{
    switch (op) {
        case OP_CONSTANT: return "OP_CONSTANT";
        case OP_NIL: return "OP_NIL";
        case OP_TRUE: return "OP_TRUE";
        case OP_FALSE: return "OP_FALSE";
        case OP_POP: return "OP_POP";
        case OP_GET_LOCAL: return "OP_GET_LOCAL";
        case OP_SET_LOCAL: return "OP_SET_LOCAL";
        case OP_GET_GLOBAL: return "OP_GET_GLOBAL";
        case OP_DEFINE_GLOBAL: return "OP_DEFINE_GLOBAL";
        case OP_SET_GLOBAL: return "OP_SET_GLOBAL";
        case OP_GET_UPVALUE: return "OP_GET_UPVALUE";
        case OP_SET_UPVALUE: return "OP_SET_UPVALUE";
        case OP_GET_PROPERTY: return "OP_GET_PROPERTY";
        case OP_SET_PROPERTY: return "OP_SET_PROPERTY";
        case OP_GET_SUPER: return "OP_GET_SUPER";
        case OP_EQUAL: return "OP_EQUAL";
        case OP_GREATER: return "OP_GREATER";
        case OP_LESS: return "OP_LESS";
        case OP_ADD: return "OP_ADD";
        case OP_SUBTRACT: return "OP_SUBTRACT";
        case OP_MULTIPLY: return "OP_MULTIPLY";
        case OP_DIVIDE: return "OP_DIVIDE";
        case OP_NOT: return "OP_NOT";
        case OP_NEGATE: return "OP_NEGATE";
        case OP_PRINT: return "OP_PRINT";
        case OP_JUMP: return "OP_JUMP";
        case OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE";
        case OP_LOOP: return "OP_LOOP";
        case OP_CALL: return "OP_CALL";
        case OP_INVOKE: return "OP_INVOKE";
        case OP_SUPER_INVOKE: return "OP_SUPER_INVOKE";
        case OP_CLOSURE: return "OP_CLOSURE";
        case OP_CLOSE_UPVALUE: return "OP_CLOSE_UPVALUE";
        case OP_RETURN: return "OP_RETURN";
        case OP_EXIT: return "OP_EXIT";
        case OP_ASSERT: return "OP_ASSERT";
        case OP_CLASS: return "OP_CLASS";
        case OP_INHERIT: return "OP_INHERIT";
        case OP_METHOD: return "OP_METHOD";
        case OP_CONSTANT_LONG: return "OP_CONSTANT_LONG";
        case OP_POPN: return "OP_POPN";
        case OP_DUP: return "OP_DUP";
        default: return NULL;
    }
}

const char *value_type_t_to_str(const value_type_t type)
{
    switch (type) {
        case VAL_BOOL: return "VAL_BOOL";
        case VAL_NIL: return "VAL_NIL";
        case VAL_NUMBER: return "VAL_NUMBER";
        case VAL_OBJ: return "VAL_OBJ";
        case VAL_EMPTY: return "VAL_EMPTY";
        default: return NULL;
    }
}

const char *token_type_t_to_str(const token_type_t type)
{
    switch (type) {
        // single character
        case TOKEN_LEFT_PAREN: return "TOKEN_LEFT_PAREN";
        case TOKEN_RIGHT_PAREN: return "TOKEN_RIGHT_PAREN";
        case TOKEN_LEFT_BRACE: return "TOKEN_LEFT_BRACE";
        case TOKEN_RIGHT_BRACE: return "TOKEN_RIGHT_BRACE";
        case TOKEN_COLON: return "TOKEN_COLON";
        case TOKEN_COMMA: return "TOKEN_COMMA";
        case TOKEN_DOT: return "TOKEN_DOT";
        case TOKEN_MINUS: return "TOKEN_MINUS";
        case TOKEN_PLUS: return "TOKEN_PLUS";
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON";
        case TOKEN_SLASH: return "TOKEN_SLASH";
        case TOKEN_STAR: return "TOKEN_STAR";
        // one or two characters
        case TOKEN_BANG: return "TOKEN_BANG";
        case TOKEN_BANG_EQUAL: return "TOKEN_BANG_EQUAL";
        case TOKEN_EQUAL: return "TOKEN_EQUAL";
        case TOKEN_EQUAL_EQUAL: return "TOKEN_EQUAL_EQUAL";
        case TOKEN_GREATER: return "TOKEN_GREATER";
        case TOKEN_GREATER_EQUAL: return "TOKEN_GREATER_EQUAL";
        case TOKEN_LESS: return "TOKEN_LESS";
        case TOKEN_LESS_EQUAL: return "TOKEN_LESS_EQUAL";
        // literals
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER";
        case TOKEN_STRING: return "TOKEN_STRING";
        case TOKEN_NUMBER: return "TOKEN_NUMBER";
        // keywords
        case TOKEN_AND: return "TOKEN_AND";
        case TOKEN_BREAK: return "TOKEN_BREAK";
        case TOKEN_CASE: return "TOKEN_CASE";
        case TOKEN_CLASS: return "TOKEN_CLASS";
        case TOKEN_CONTINUE: return "TOKEN_CONTINUE";
        case TOKEN_DEFAULT: return "TOKEN_DEFAULT";
        case TOKEN_ELSE: return "TOKEN_ELSE";
        case TOKEN_FALSE: return "TOKEN_FALSE";
        case TOKEN_FOR: return "TOKEN_FOR";
        case TOKEN_FUN: return "TOKEN_FUN";
        case TOKEN_IF: return "TOKEN_IF";
        case TOKEN_NIL: return "TOKEN_NIL";
        case TOKEN_OR: return "TOKEN_OR";
        case TOKEN_PRINT: return "TOKEN_PRINT";
        case TOKEN_RETURN: return "TOKEN_RETURN";
        case TOKEN_SUPER: return "TOKEN_SUPER";
        case TOKEN_SWITCH: return "TOKEN_SWITCH";
        case TOKEN_THIS: return "TOKEN_THIS";
        case TOKEN_TRUE: return "TOKEN_TRUE";
        case TOKEN_VAR: return "TOKEN_VAR";
        case TOKEN_WHILE: return "TOKEN_WHILE";
        case TOKEN_ERROR: return "TOKEN_ERROR";
        case TOKEN_EOF: return "TOKEN_EOF";
        default: return NULL;
    }
}

const char *obj_type_t_to_str(const obj_type_t type)
{
    switch (type) {
        case OBJ_BOUND_METHOD: return "OBJ_BOUND_METHOD";
        case OBJ_CLASS: return "OBJ_CLASS";
        case OBJ_CLOSURE: return "OBJ_CLOSURE";
        case OBJ_FUNCTION: return "OBJ_FUNCTION";
        case OBJ_INSTANCE: return "OBJ_INSTANCE";
        case OBJ_NATIVE: return "OBJ_NATIVE";
        case OBJ_STRING: return "OBJ_STRING";
        case OBJ_LIST: return "OBJ_LIST";
        case OBJ_MAP: return "OBJ_MAP";
        case OBJ_BOUND_NATIVE_METHOD: return "OBJ_BOUND_NATIVE_METHOD";
        default: return NULL;
    }
}
