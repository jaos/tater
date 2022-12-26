#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"
#include "value.h"
#include "scanner.h"

void disassemble_chunk(const chunk_t* chunk, const char* name);
int disassemble_instruction(const chunk_t* chunk, int offset);
const char *op_code_t_to_str(const op_code_t);
const char *value_type_t_to_str(const value_type_t);
const char *token_type_t_to_str(const token_type_t);

#endif
