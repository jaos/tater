#ifndef tater_vmopcodes_h
#define tater_vmopcodes_h
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

#include "common.h"

typedef enum {
    OP_CONSTANT,
    OP_CONSTANT_LONG,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_POPN,
    OP_DUP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_GET_SUPER,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_BITWISE_AND,
    OP_BITWISE_OR,
    OP_BITWISE_NOT,
    OP_BITWISE_XOR,
    OP_SHIFT_LEFT,
    OP_SHIFT_RIGHT,
    OP_NOT,
    OP_MOD,
    OP_NEGATE,
    OP_PRINT,
    OP_ERROR,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_EXIT,
    OP_TYPE,
    OP_INHERIT,
    OP_METHOD,
    OP_FIELD,
    OP_ASSERT,
    INVALID_OPCODE,
} op_code_t;

static const char *const op_code_name[] = {
    [OP_CONSTANT] = "OP_CONSTANT",
    [OP_CONSTANT_LONG] = "OP_CONSTANT_LONG",
    [OP_NIL] = "OP_NIL",
    [OP_TRUE] = "OP_TRUE",
    [OP_FALSE] = "OP_FALSE",
    [OP_POP] = "OP_POP",
    [OP_POPN] = "OP_POPN",
    [OP_DUP] = "OP_DUP",
    [OP_GET_LOCAL] = "OP_GET_LOCAL",
    [OP_SET_LOCAL] = "OP_SET_LOCAL",
    [OP_GET_GLOBAL] = "OP_GET_GLOBAL",
    [OP_DEFINE_GLOBAL] = "OP_DEFINE_GLOBAL",
    [OP_SET_GLOBAL] = "OP_SET_GLOBAL",
    [OP_GET_UPVALUE] = "OP_GET_UPVALUE",
    [OP_SET_UPVALUE] = "OP_SET_UPVALUE",
    [OP_GET_PROPERTY] = "OP_GET_PROPERTY",
    [OP_SET_PROPERTY] = "OP_SET_PROPERTY",
    [OP_GET_SUPER] = "OP_GET_SUPER",
    [OP_EQUAL] = "OP_EQUAL",
    [OP_GREATER] = "OP_GREATER",
    [OP_LESS] = "OP_LESS",
    [OP_ADD] = "OP_ADD",
    [OP_SUBTRACT] = "OP_SUBTRACT",
    [OP_MULTIPLY] = "OP_MULTIPLY",
    [OP_DIVIDE] = "OP_DIVIDE",
    [OP_BITWISE_AND] = "OP_BITWISE_AND",
    [OP_BITWISE_OR] = "OP_BITWISE_OR",
    [OP_BITWISE_NOT] = "OP_BITWISE_NOT",
    [OP_BITWISE_XOR] = "OP_BITWISE_XOR",
    [OP_SHIFT_LEFT] = "OP_SHIFT_LEFT",
    [OP_SHIFT_RIGHT] = "OP_SHIFT_RIGHT",
    [OP_NOT] = "OP_NOT",
    [OP_MOD] = "OP_MOD",
    [OP_NEGATE] = "OP_NEGATE",
    [OP_PRINT] = "OP_PRINT",
    [OP_ERROR] = "OP_ERROR",
    [OP_JUMP] = "OP_JUMP",
    [OP_JUMP_IF_FALSE] = "OP_JUMP_IF_FALSE",
    [OP_LOOP] = "OP_LOOP",
    [OP_CALL] = "OP_CALL",
    [OP_INVOKE] = "OP_INVOKE",
    [OP_SUPER_INVOKE] = "OP_SUPER_INVOKE",
    [OP_CLOSURE] = "OP_CLOSURE",
    [OP_CLOSE_UPVALUE] = "OP_CLOSE_UPVALUE",
    [OP_RETURN] = "OP_RETURN",
    [OP_ASSERT] = "OP_ASSERT",
    [OP_EXIT] = "OP_EXIT",
    [OP_TYPE] = "OP_TYPE",
    [OP_INHERIT] = "OP_INHERIT",
    [OP_METHOD] = "OP_METHOD",
    [OP_FIELD] = "OP_FIELD",
    [INVALID_OPCODE] = "INVALID_OPCODE",
};

#endif
