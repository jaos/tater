#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

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
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_INVOKE,
    OP_SUPER_INVOKE,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_ASSERT,
    OP_EXIT,
    OP_CLASS,
    OP_INHERIT,
    OP_METHOD,
} op_code_t;

typedef struct {
    int offset;
    int line;
} line_info_t;

typedef struct {
    int count;
    int capacity;
    uint8_t *code;
    value_array_t constants;
    int *lines;
    /*
    int line_count;
    int line_capacity;
    line_info_t *lines;
    */
} chunk_t;

void init_chunk(chunk_t *chunk);
void free_chunk(chunk_t *chunk);
void write_chunk(chunk_t *chunk, const uint8_t byte, const int line);
int add_constant(chunk_t *chunk, const value_t value);
// int get_line(const chunk_t *chunk, const int instruction);

#endif
