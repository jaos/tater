#ifndef tater_scanner_h
#define tater_scanner_h
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

#include "common.h"

typedef enum {
    // single character
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
    TOKEN_COLON, TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    TOKEN_BIT_AND, TOKEN_BIT_OR, TOKEN_BIT_NOT, TOKEN_BIT_XOR,
    // several characters
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL,
    TOKEN_XOR_EQUAL, TOKEN_AND_EQUAL, TOKEN_OR_EQUAL,
    TOKEN_MINUS_MINUS, TOKEN_PLUS_PLUS,
    TOKEN_SHIFT_LEFT, TOKEN_SHIFT_RIGHT, TOKEN_SHIFT_LEFT_EQUAL, TOKEN_SHIFT_RIGHT_EQUAL,
    TOKEN_MOD,
    // literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // keywords
    TOKEN_AND, TOKEN_ASSERT, TOKEN_BREAK, TOKEN_CASE, TOKEN_TYPE, TOKEN_CONTINUE,
    TOKEN_DEFAULT, TOKEN_ELSE, TOKEN_EXIT, TOKEN_FALSE, TOKEN_FOR, TOKEN_FN,
    TOKEN_IF, TOKEN_NIL, TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER,
    TOKEN_SWITCH, TOKEN_SELF, TOKEN_TRUE, TOKEN_LET, TOKEN_WHILE, TOKEN_PERROR,

    TOKEN_ERROR, TOKEN_EOF,
} token_type_t;

static const char *const token_keyword_names[] = {
    // single character
    [TOKEN_LEFT_PAREN] = "(",
    [TOKEN_RIGHT_PAREN] = ")",
    [TOKEN_LEFT_BRACE] = "{",
    [TOKEN_RIGHT_BRACE] = "}",
    [TOKEN_LEFT_BRACKET] = "[",
    [TOKEN_RIGHT_BRACKET] = "]",
    [TOKEN_COLON] = ":",
    [TOKEN_COMMA] = ",",
    [TOKEN_DOT] = ".",
    [TOKEN_MINUS] = "-",
    [TOKEN_PLUS] = "+",
    [TOKEN_SEMICOLON] = ";",
    [TOKEN_SLASH] = "/",
    [TOKEN_STAR] = "*",
    [TOKEN_BIT_AND] = "&",
    [TOKEN_BIT_OR] = "|",
    [TOKEN_BIT_NOT] = "~",
    [TOKEN_BIT_XOR] = "^",
    // one or two characters
    [TOKEN_BANG] = "!",
    [TOKEN_BANG_EQUAL] = "!=",
    [TOKEN_EQUAL] = "=",
    [TOKEN_EQUAL_EQUAL] = "==",
    [TOKEN_GREATER] = ">",
    [TOKEN_GREATER_EQUAL] = ">=",
    [TOKEN_LESS] = "<",
    [TOKEN_LESS_EQUAL] = "<=",
    [TOKEN_PLUS_EQUAL] = "+=",
    [TOKEN_MINUS_EQUAL] = "-=",
    [TOKEN_STAR_EQUAL] = "*=",
    [TOKEN_SLASH_EQUAL] = "/=",
    [TOKEN_XOR_EQUAL] = "^=",
    [TOKEN_AND_EQUAL] = "&=",
    [TOKEN_OR_EQUAL] = "|=",
    [TOKEN_MINUS_MINUS] = "--",
    [TOKEN_PLUS_PLUS] = "++",
    [TOKEN_SHIFT_LEFT] = "<<",
    [TOKEN_SHIFT_RIGHT] = ">>",
    [TOKEN_SHIFT_LEFT_EQUAL] = "<<=",
    [TOKEN_SHIFT_RIGHT_EQUAL] = ">>=",
    [TOKEN_MOD] = "&",
    // literals
    [TOKEN_IDENTIFIER] = "<identifier>",
    [TOKEN_STRING] = "string",
    [TOKEN_NUMBER] = "number",
    // keywords
    [TOKEN_AND] = "and",
    [TOKEN_ASSERT] = "assert",
    [TOKEN_BREAK] = "break",
    [TOKEN_CASE] = "case",
    [TOKEN_TYPE] = "type",
    [TOKEN_CONTINUE] = "continue",
    [TOKEN_DEFAULT] = "default",
    [TOKEN_ELSE] = "else",
    [TOKEN_EXIT] = "exit",
    [TOKEN_FALSE] = "false",
    [TOKEN_FOR] = "for",
    [TOKEN_FN] = "fn",
    [TOKEN_IF] = "if",
    [TOKEN_NIL] = "nil",
    [TOKEN_OR] = "or",
    [TOKEN_PRINT] = "print",
    [TOKEN_RETURN] = "return",
    [TOKEN_SUPER] = "super",
    [TOKEN_SWITCH] = "switch",
    [TOKEN_SELF] = "self",
    [TOKEN_TRUE] = "true",
    [TOKEN_LET] = "let",
    [TOKEN_WHILE] = "while",
    [TOKEN_PERROR] = "error",
    [TOKEN_ERROR] = "error",
    [TOKEN_EOF] = "<EOF>",
    "&&", // equiv to TOKEN_AND
    "||", // equiv to TOKEN_OR
    "list",
    "map",
    NULL,
};


#define KEYWORD_INIT "init"
#define KEYWORD_INIT_LEN 4
#define TOKEN_SELF_LEN 4
#define KEYWORD_SUBSCRIPT "subscript"
#define KEYWORD_SUBSCRIPT_LEN 9
#define KEYWORD_LEN "len"
#define KEYWORD_LEN_LEN 3
#define KEYWORD_GET "get"
#define KEYWORD_GET_LEN 3
#define KEYWORD_SET "set"
#define KEYWORD_SET_LEN 3
#define KEYWORD_APPEND "append"
#define KEYWORD_APPEND_LEN 6
#define KEYWORD_REMOVE "remove"
#define KEYWORD_REMOVE_LEN 6
#define KEYWORD_CLEAR "clear"
#define KEYWORD_CLEAR_LEN 5
#define KEYWORD_KEYS "keys"
#define KEYWORD_KEYS_LEN 4
#define KEYWORD_VALUES "values"
#define KEYWORD_VALUES_LEN 6
#define KEYWORD_LIST "list"
#define KEYWORD_LIST_LEN 4
#define KEYWORD_MAP "map"
#define KEYWORD_MAP_LEN 3

typedef struct {
    token_type_t type;
    const char *start;
    int length;
    int line;
} token_t;

void scanner_t_init(const char *source);
token_t scanner_t_scan_token(void);

#endif
