#ifndef clox_scanner_h
#define clox_scanner_h

#include "common.h"

typedef enum {
    // single character
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COLON, TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS, TOKEN_PLUS,
    TOKEN_SEMICOLON, TOKEN_SLASH, TOKEN_STAR,
    // one or two characters
    TOKEN_BANG, TOKEN_BANG_EQUAL,
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL,
    TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL,
    TOKEN_MINUS_MINUS, TOKEN_PLUS_PLUS,
    // literals
    TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,
    // keywords
    TOKEN_AND, TOKEN_ASSERT, TOKEN_BREAK, TOKEN_CASE, TOKEN_CLASS, TOKEN_CONTINUE,
    TOKEN_DEFAULT, TOKEN_ELSE, TOKEN_EXIT, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN,
    TOKEN_IF, TOKEN_NIL, TOKEN_OR, TOKEN_PRINT, TOKEN_RETURN, TOKEN_SUPER,
    TOKEN_SWITCH, TOKEN_THIS, TOKEN_TRUE, TOKEN_VAR, TOKEN_WHILE,

    TOKEN_ERROR, TOKEN_EOF,
} token_type_t;

static const char *const token_keyword_names[] = {
    // single character
    [TOKEN_LEFT_PAREN] = "(",
    [TOKEN_RIGHT_PAREN] = ")",
    [TOKEN_LEFT_BRACE] = "{",
    [TOKEN_RIGHT_BRACE] = "}",
    [TOKEN_COLON] = ":",
    [TOKEN_COMMA] = ",",
    [TOKEN_DOT] = ".",
    [TOKEN_MINUS] = "-",
    [TOKEN_PLUS] = "+",
    [TOKEN_SEMICOLON] = ";",
    [TOKEN_SLASH] = "/",
    [TOKEN_STAR] = "*",
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
    [TOKEN_MINUS_MINUS] = "--",
    [TOKEN_PLUS_PLUS] = "++",
    // literals
    [TOKEN_IDENTIFIER] = "<identifier>",
    [TOKEN_STRING] = "<string>",
    [TOKEN_NUMBER] = "<number>",
    // keywords
    [TOKEN_AND] = "and",
    [TOKEN_ASSERT] = "assert",
    [TOKEN_BREAK] = "break",
    [TOKEN_CASE] = "case",
    [TOKEN_CLASS] = "class",
    [TOKEN_CONTINUE] = "continue",
    [TOKEN_DEFAULT] = "default",
    [TOKEN_ELSE] = "else",
    [TOKEN_EXIT] = "exit",
    [TOKEN_FALSE] = "false",
    [TOKEN_FOR] = "for",
    [TOKEN_FUN] = "fun",
    [TOKEN_IF] = "if",
    [TOKEN_NIL] = "nil",
    [TOKEN_OR] = "or",
    [TOKEN_PRINT] = "print",
    [TOKEN_RETURN] = "return",
    [TOKEN_SUPER] = "super",
    [TOKEN_SWITCH] = "switch",
    [TOKEN_THIS] = "this",
    [TOKEN_TRUE] = "true",
    [TOKEN_VAR] = "var",
    [TOKEN_WHILE] = "while",
    [TOKEN_ERROR] = "error",
    [TOKEN_EOF] = "<EOF>",
    NULL,
};


#define KEYWORD_INIT "init"
#define KEYWORD_INIT_LEN 4
#define TOKEN_THIS_LEN 4

typedef struct {
    token_type_t type;
    const char *start;
    int length;
    int line;
} token_t;

void scanner_t_init(const char *source);
token_t scanner_t_scan_token(void);

#endif
