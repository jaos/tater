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


#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char *start;
    const char *current;
    int line;
} scanner_t;

static scanner_t scanner;

void scanner_t_init(const char *source)
{
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static token_t make_token(const token_type_t type)
{
    token_t token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static token_t error_token(const char *message)
{
    token_t token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = scanner.line;
    return token;
}

static bool is_at_end(void)
{
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static bool match(const char expected)
{
    if (is_at_end()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

static char peek(void)
{
    return *scanner.current;
}

static char peek_next(void) {
    if (is_at_end()) return '\0';
    return scanner.current[1];
}

static void skip_whitespace(void)
{
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t': {
                advance();
                break;
            }
            case '\n': {
                scanner.line++;
                advance();
                break;
            }
            case '#': {
                while (peek() != '\n' && !is_at_end()) advance();
                break;
            }
            case '/': {
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else {
                    return;
                }
                break;
            }
            default: return;
        }
    }
}

static token_t string(void)
{
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }
    if (is_at_end())
        return error_token(gettext("Unterminated string."));
    advance();

    return make_token(TOKEN_STRING);
}

static bool is_digit(const char c)
{
    return c >= '0' && c <= '9';
}
static bool is_hexdigit(const char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static token_t number(void)
{
    bool is_hex = false;

    while (is_digit(peek())) advance();
    if (peek() == '.' && is_digit(peek_next())) {
        advance();
    }
    if (peek() == '_' && is_digit(peek_next())) {
        advance();
    }
    if (peek() == 'x' && is_hexdigit(peek_next())) {
        is_hex = true;
        advance();
    }
    if (peek() == 'b' && is_digit(peek_next())) {
        advance();
    }
    if (peek() == ' ' && is_hexdigit(peek_next())) {
        advance();
    }
    if (peek() == 'o' && is_digit(peek_next())) {
        advance();
    }
    if (is_hex)
        while (is_hexdigit(peek()) || peek() == '_' || peek() == ' ') {
            advance();
        }
    else {
        while (is_digit(peek()) || peek() == '_' || peek() == ' ') {
            advance();
        }
    }
    return make_token(TOKEN_NUMBER);
}

static bool is_alpha(const char c)
{
    return (c >= 'a' && c <='z') || (c >= 'A' && c<= 'Z') || c == '_';
}

static token_type_t check_keyword(int start, int length, const char *rest, token_type_t type)
{
    if (scanner.current - scanner.start == start + length && memcmp(scanner.start+start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}
static token_type_t identifier_type(void)
{
    switch (scanner.start[0]) {
        case 'a': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'n': return check_keyword(2, 1, "d", TOKEN_AND);
                    case 's': return check_keyword(2, 4, "sert", TOKEN_ASSERT);
                    default: ; // default returned below
                }
            }
            break;
        }
        case 'b': return check_keyword(1, 4, "reak", TOKEN_BREAK);
        case 'c': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 2, "se", TOKEN_CASE);
                    case 'o': return check_keyword(2, 6, "ntinue", TOKEN_CONTINUE);
                    default: ; // default returned below
                }
            }
            break;
        }
        case 'd': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return check_keyword(2, 5, "fault", TOKEN_DEFAULT);
                    default: ; // default returned below
                }
            }
            break;
        }
        case 'e': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'l': return check_keyword(2, 2, "se", TOKEN_ELSE);
                    case 'x': return check_keyword(2, 2, "it", TOKEN_EXIT);
                    case 'r': return check_keyword(2, 3, "ror", TOKEN_PERROR);
                    default: ; // default returned below
                }
            }
            break;
        }
        case 'f': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'n': return TOKEN_FN;
                    default: ; // default returned below
                }
            }
            break;
        }
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'l': return check_keyword(1, 2, "et", TOKEN_LET);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return check_keyword(2, 2, "lf", TOKEN_SELF);
                    case 'u': return check_keyword(2, 3, "per", TOKEN_SUPER);
                    case 'w': return check_keyword(2, 4, "itch", TOKEN_SWITCH);
                    default: ;  // default returned below
                }
            }
            break;
        }
        case 't': {
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                    case 'y': return check_keyword(2, 2, "pe", TOKEN_TYPE);
                    default: ;  // default returned below
                }
            }
            break;
        }
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
        default: ; // default return below
    }
    return TOKEN_IDENTIFIER;
}

static token_t identifier(void)
{
    while (is_alpha(peek()) || is_digit(peek())) advance();
    return make_token(identifier_type());
}

token_t scanner_t_scan_token(void)
{
    skip_whitespace();
    scanner.start = scanner.current;
    if (is_at_end()) return make_token(TOKEN_EOF);

    const char c = advance();
    if (is_alpha(c)) return identifier();
    if (is_digit(c)) return number();
    switch (c) {
        // single character
        case '(': return make_token(TOKEN_LEFT_PAREN);
        case ')': return make_token(TOKEN_RIGHT_PAREN);
        case '{': return make_token(TOKEN_LEFT_BRACE);
        case '}': return make_token(TOKEN_RIGHT_BRACE);
        case '[': return make_token(TOKEN_LEFT_BRACKET);
        case ']': return make_token(TOKEN_RIGHT_BRACKET);
        case ':': return make_token(TOKEN_COLON);
        case ';': return make_token(TOKEN_SEMICOLON);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '-': {
            const char next = peek();
            switch (next) {
                case '=': advance(); return make_token(TOKEN_MINUS_EQUAL);
                case '-': advance(); return make_token(TOKEN_MINUS_MINUS);
                default: return make_token(TOKEN_MINUS);
            }
        }
        case '+':  {
            const char next = peek();
            switch (next) {
                case '=': advance(); return make_token(TOKEN_PLUS_EQUAL);
                case '+': advance(); return make_token(TOKEN_PLUS_PLUS);
                default: return make_token(TOKEN_PLUS);
            }
        }
        case '/':  {
            const char next = peek();
            switch (next) {
                case '=': advance(); return make_token(TOKEN_SLASH_EQUAL);
                default: return make_token(TOKEN_SLASH);
            }
        }
        case '*':  {
            const char next = peek();
            switch (next) {
                case '=': advance(); return make_token(TOKEN_STAR_EQUAL);
                default: return make_token(TOKEN_STAR);
            }
        }
        // one or two characters
        case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<': {
            const char next = peek();
            switch (next) {
                case '<': advance(); return make_token(match('=') ? TOKEN_SHIFT_LEFT_EQUAL : TOKEN_SHIFT_LEFT);
                case '=': advance(); return make_token(TOKEN_LESS_EQUAL);
                default: return make_token(TOKEN_LESS);
            }
        }
        case '>': {
            const char next = peek();
            switch (next) {
                case '>': advance(); return make_token(match('=') ? TOKEN_SHIFT_RIGHT_EQUAL : TOKEN_SHIFT_RIGHT);
                case '=': advance(); return make_token(TOKEN_GREATER_EQUAL);
                default: return make_token(TOKEN_GREATER);
            }
        }
        case '"': return string();
        case '|': {
            const char next = peek();
            switch (next) {
                case '|': advance(); return make_token(TOKEN_OR);
                case '=': advance(); return make_token(TOKEN_OR_EQUAL);
                default: return make_token(TOKEN_BIT_OR);
            }
        }
        case '&': {
            const char next = peek();
            switch (next) {
                case '&': advance(); return make_token(TOKEN_AND);
                case '=': advance(); return make_token(TOKEN_AND_EQUAL);
                default: return make_token(TOKEN_BIT_AND);
            }
        }
        case '%': return make_token(TOKEN_MOD);
        case '~': return make_token(TOKEN_BIT_NOT);
        case '^': return make_token(match('=') ? TOKEN_XOR_EQUAL : TOKEN_BIT_XOR);
        default: return error_token(gettext("Unexpected character."));
    }
}
