#include <assert.h>
#include <stdio.h>
#include "debug.h"
#include "object.h"
#include "value.h"
#include "compiler.h"
#include "scanner.h"

void disassemble_chunk(const chunk_t *chunk, const char *name)
{
    printf("== start %s ==\n", name);
    for (int offset = 0; offset < chunk->count;) {
        offset = disassemble_instruction(chunk, offset);
    }
    printf("==   end %s ==\n", name);
}

static int simple_instruction(const char *name, const int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

static int constant_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    const uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    assert(chunk->constants.count >= constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 2;
}

static int long_constant_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    const uint32_t constant = chunk->code[offset + 1] |
        (chunk->code[offset + 2] << 8) |
        (chunk->code[offset + 3] << 16);
    printf("%-16s %4d '", name, constant);
    print_value(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 4;
}

static int byte_instruction(const char *name, const chunk_t *chunk, const int offset)
{
    // TODO figure out how to map our chunk to the compiler locals to get the variable name
    const uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jump_instruction(const char *name, const int sign, const chunk_t *chunk, const int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassemble_instruction(const chunk_t *chunk, int offset)
{
    printf("%04d ", offset);

    const int line = get_line(chunk, offset);
    if (offset > 0 && line == get_line(chunk, offset - 1)) {
        printf("   | ");
    } else {
        printf("%4d ", line);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT: return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_CONSTANT_LONG: return long_constant_instruction("OP_CONSTANT_LONG", chunk, offset);
        case OP_PRINT: return simple_instruction("OP_PRINT", offset);
        case OP_JUMP: return jump_instruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jump_instruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP: return jump_instruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL: return byte_instruction("OP_CALL", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            print_value(chunk->constants.values[constant]);
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
        case OP_NEGATE: return simple_instruction("OP_NEGATE", offset);
        case OP_NIL: return simple_instruction("OP_NIL", offset);
        case OP_DUP: return simple_instruction("OP_DUP", offset);
        case OP_TRUE: return simple_instruction("OP_TRUE", offset);
        case OP_FALSE: return simple_instruction("OP_FALSE", offset);
        case OP_POP: return simple_instruction("OP_POP", offset);
        case OP_POPN: return byte_instruction("OP_POPN", chunk, offset);
        case OP_GET_LOCAL: return byte_instruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL: return byte_instruction("OP_SET_LOCAL", chunk, offset);
        case OP_GET_GLOBAL: return constant_instruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL: return constant_instruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL: return constant_instruction("OP_SET_GLOBAL", chunk, offset);
        case OP_GET_UPVALUE: return byte_instruction("OP_GET_UPVALUE", chunk, offset);
        case OP_SET_UPVALUE: return byte_instruction("OP_SET_UPVALUE", chunk, offset);
        case OP_EQUAL: return simple_instruction("OP_EQUAL", offset);
        case OP_GREATER: return simple_instruction("OP_GREATER", offset);
        case OP_LESS: return simple_instruction("OP_LESS", offset);
        case OP_ADD: return simple_instruction("OP_ADD", offset);
        case OP_SUBTRACT: return simple_instruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY: return simple_instruction("OP_MULTIPLY", offset);
        case OP_DIVIDE: return simple_instruction("OP_DIVIDE", offset);
        case OP_NOT: return simple_instruction("OP_NOT", offset);
        default: {
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
        }
    }
}

const char *op_code_t_to_str(const op_code_t op)
{
    switch (op) {
        case OP_CONSTANT: return "OP_CONSTANT"; break;
        case OP_CONSTANT_LONG: return "OP_CONSTANT_LONG"; break;
        case OP_NIL: return "OP_NIL"; break;
        case OP_TRUE: return "OP_TRUE"; break;
        case OP_FALSE: return "OP_FALSE"; break;
        case OP_POP: return "OP_POP"; break;
        case OP_POPN: return "OP_POPN"; break;
        case OP_GET_LOCAL: return "OP_GET_LOCAL"; break;
        case OP_SET_LOCAL: return "OP_SET_LOCAL"; break;
        case OP_GET_GLOBAL: return "OP_GET_GLOBAL"; break;
        case OP_DEFINE_GLOBAL: return "OP_DEFINE_GLOBAL"; break;
        case OP_SET_GLOBAL: return "OP_SET_GLOBAL"; break;
        case OP_GET_UPVALUE: return "OP_GET_UPVALUE"; break;
        case OP_SET_UPVALUE: return "OP_SET_UPVALUE"; break;
        case OP_EQUAL: return "OP_EQUAL"; break;
        case OP_GREATER: return "OP_GREATER"; break;
        case OP_LESS: return "OP_LESS"; break;
        case OP_ADD: return "OP_ADD"; break;
        case OP_SUBTRACT: return "OP_SUBTRACT"; break;
        case OP_MULTIPLY: return "OP_MULTIPLY"; break;
        case OP_DIVIDE: return "OP_DIVIDE"; break;
        case OP_NOT: return "OP_NOT"; break;
        case OP_NEGATE: return "OP_NEGATE"; break;
        case OP_PRINT: return "OP_PRINT"; break;
        case OP_JUMP: return "OP_JUMP"; break;
        case OP_JUMP_IF_FALSE: return "OP_JUMP_IF_FALSE"; break;
        case OP_LOOP: return "OP_LOOP"; break;
        case OP_DUP: return "OP_DUP"; break;
        case OP_CALL: return "OP_CALL"; break;
        case OP_CLOSURE: return "OP_CALL"; break;
        case OP_CLOSE_UPVALUE: return "OP_CLOSE_UPVALUE"; break;
        case OP_RETURN: return "OP_RETURN"; break;
        default: return NULL;
    }
}

const char *value_type_t_to_str(const value_type_t type)
{
    switch (type) {
        case VAL_BOOL: return "VAL_BOOL"; break;
        case VAL_NIL: return "VAL_NIL"; break;
        case VAL_NUMBER: return "VAL_NUMBER"; break;
        case VAL_OBJ: return "VAL_OBJ"; break;
        case VAL_EMPTY: return "VAL_EMPTY"; break;
        default: return NULL;
    }
}

const char *token_type_t_to_str(const token_type_t type)
{
    switch (type) {
        // single character
        case TOKEN_LEFT_PAREN: return "TOKEN_LEFT_PAREN"; break;
        case TOKEN_RIGHT_PAREN: return "TOKEN_RIGHT_PAREN"; break;
        case TOKEN_LEFT_BRACE: return "TOKEN_LEFT_BRACE"; break;
        case TOKEN_RIGHT_BRACE: return "TOKEN_RIGHT_BRACE"; break;
        case TOKEN_COLON: return "TOKEN_COLON"; break;
        case TOKEN_COMMA: return "TOKEN_COMMA"; break;
        case TOKEN_DOT: return "TOKEN_DOT"; break;
        case TOKEN_MINUS: return "TOKEN_MINUS"; break;
        case TOKEN_PLUS: return "TOKEN_PLUS"; break;
        case TOKEN_SEMICOLON: return "TOKEN_SEMICOLON"; break;
        case TOKEN_SLASH: return "TOKEN_SLASH"; break;
        case TOKEN_STAR: return "TOKEN_STAR"; break;
        // one or two characters
        case TOKEN_BANG: return "TOKEN_BANG"; break;
        case TOKEN_BANG_EQUAL: return "TOKEN_BANG_EQUAL"; break;
        case TOKEN_EQUAL: return "TOKEN_EQUAL"; break;
        case TOKEN_EQUAL_EQUAL: return "TOKEN_EQUAL_EQUAL"; break;
        case TOKEN_GREATER: return "TOKEN_GREATER"; break;
        case TOKEN_GREATER_EQUAL: return "TOKEN_GREATER_EQUAL"; break;
        case TOKEN_LESS: return "TOKEN_LESS"; break;
        case TOKEN_LESS_EQUAL: return "TOKEN_LESS_EQUAL"; break;
        // literals
        case TOKEN_IDENTIFIER: return "TOKEN_IDENTIFIER"; break;
        case TOKEN_STRING: return "TOKEN_STRING"; break;
        case TOKEN_NUMBER: return "TOKEN_NUMBER"; break;
        // keywords
        case TOKEN_AND: return "TOKEN_AND"; break;
        case TOKEN_BREAK: return "TOKEN_BREAK"; break;
        case TOKEN_CASE: return "TOKEN_CASE"; break;
        case TOKEN_CLASS: return "TOKEN_CLASS"; break;
        case TOKEN_CONTINUE: return "TOKEN_CONTINUE"; break;
        case TOKEN_DEFAULT: return "TOKEN_DEFAULT"; break;
        case TOKEN_ELSE: return "TOKEN_ELSE"; break;
        case TOKEN_FALSE: return "TOKEN_FALSE"; break;
        case TOKEN_FOR: return "TOKEN_FOR"; break;
        case TOKEN_FUN: return "TOKEN_FUN"; break;
        case TOKEN_IF: return "TOKEN_IF"; break;
        case TOKEN_NIL: return "TOKEN_NIL"; break;
        case TOKEN_OR: return "TOKEN_OR"; break;
        case TOKEN_PRINT: return "TOKEN_PRINT"; break;
        case TOKEN_RETURN: return "TOKEN_RETURN"; break;
        case TOKEN_SUPER: return "TOKEN_SUPER"; break;
        case TOKEN_SWITCH: return "TOKEN_SWITCH"; break;
        case TOKEN_THIS: return "TOKEN_THIS"; break;
        case TOKEN_TRUE: return "TOKEN_TRUE"; break;
        case TOKEN_VAR: return "TOKEN_VAR"; break;
        case TOKEN_WHILE: return "TOKEN_WHILE"; break;
        case TOKEN_ERROR: return "TOKEN_ERROR"; break;
        case TOKEN_EOF: return "TOKEN_EOF"; break;
        default: return NULL;
    }
}
