/*
 * Copyright (C) 2022-2023 Jason Woodward <woodwardj at jaos dot org>
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
#include <stdio.h>
#include "debug.h"
#include "type.h"
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
    value_t_print(stdout, chunk->constants.values[constant]);
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
    value_t_print(stdout, chunk->constants.values[constant]);
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
    printf("%-16s %4ud '", name, constant);
    value_t_print(stdout, chunk->constants.values[constant]);
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
        case OP_CONSTANT: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_NIL: return simple_instruction(op_code_name[instruction], offset);
        case OP_TRUE: return simple_instruction(op_code_name[instruction], offset);
        case OP_FALSE: return simple_instruction(op_code_name[instruction], offset);
        case OP_POP: return simple_instruction(op_code_name[instruction], offset);
        case OP_GET_LOCAL: return byte_instruction(op_code_name[instruction], chunk, offset);
        case OP_SET_LOCAL: return byte_instruction(op_code_name[instruction], chunk, offset);
        case OP_GET_GLOBAL: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_DEFINE_GLOBAL: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_SET_GLOBAL: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_GET_UPVALUE: return byte_instruction(op_code_name[instruction], chunk, offset);
        case OP_SET_UPVALUE: return byte_instruction(op_code_name[instruction], chunk, offset);
        case OP_GET_PROPERTY: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_SET_PROPERTY: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_GET_SUPER: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_EQUAL: return simple_instruction(op_code_name[instruction], offset);
        case OP_GREATER: return simple_instruction(op_code_name[instruction], offset);
        case OP_LESS: return simple_instruction(op_code_name[instruction], offset);
        case OP_ADD: return simple_instruction(op_code_name[instruction], offset);
        case OP_SUBTRACT: return simple_instruction(op_code_name[instruction], offset);
        case OP_MULTIPLY: return simple_instruction(op_code_name[instruction], offset);
        case OP_DIVIDE: return simple_instruction(op_code_name[instruction], offset);
        case OP_BITWISE_OR: return simple_instruction(op_code_name[instruction], offset);
        case OP_BITWISE_AND: return simple_instruction(op_code_name[instruction], offset);
        case OP_BITWISE_XOR: return simple_instruction(op_code_name[instruction], offset);
        case OP_BITWISE_NOT: return simple_instruction(op_code_name[instruction], offset);
        case OP_SHIFT_LEFT: return simple_instruction(op_code_name[instruction], offset);
        case OP_SHIFT_RIGHT: return simple_instruction(op_code_name[instruction], offset);
        case OP_NOT: return simple_instruction(op_code_name[instruction], offset);
        case OP_MOD: return simple_instruction(op_code_name[instruction], offset);
        case OP_NEGATE: return simple_instruction(op_code_name[instruction], offset);
        case OP_PRINT: return simple_instruction(op_code_name[instruction], offset);
        case OP_ERROR: return simple_instruction(op_code_name[instruction], offset);
        case OP_JUMP: return jump_instruction(op_code_name[instruction], 1, chunk, offset);
        case OP_JUMP_IF_FALSE: return jump_instruction(op_code_name[instruction], 1, chunk, offset);
        case OP_LOOP: return jump_instruction(op_code_name[instruction], -1, chunk, offset);
        case OP_CALL: return byte_instruction(op_code_name[instruction], chunk, offset);
        case OP_INVOKE: return invoke_instruction(op_code_name[instruction], chunk, offset);
        case OP_SUPER_INVOKE: return invoke_instruction(op_code_name[instruction], chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", op_code_name[instruction], constant);
            value_t_print(stdout, chunk->constants.values[constant]);
            printf("\n");

            obj_function_t *function = AS_FUNCTION(chunk->constants.values[constant]);
            for (int j = 0; j < function->upvalue_count; j++) {
                int is_local = chunk->code[offset++];
                int index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", offset - 2, is_local ? "local" : "upvalue", index);
            }
            return offset;
        }
        case OP_CLOSE_UPVALUE: return simple_instruction(op_code_name[instruction], offset);
        case OP_RETURN: return simple_instruction(op_code_name[instruction], offset);
        case OP_EXIT: return simple_instruction(op_code_name[instruction], offset);
        case OP_ASSERT: return jump_instruction(op_code_name[instruction], 1, chunk, offset);
        case OP_TYPE: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_INHERIT: return simple_instruction(op_code_name[instruction], offset);
        case OP_METHOD: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_FIELD: return constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_CONSTANT_LONG: return long_constant_instruction(op_code_name[instruction], chunk, offset);
        case OP_POPN: return byte_instruction(op_code_name[instruction], chunk, offset);
        case OP_DUP: return simple_instruction(op_code_name[instruction], offset);
        default: {
            printf(gettext("Unknown opcode %d\n"), instruction);
            return offset + 1;
        }
    }
}

