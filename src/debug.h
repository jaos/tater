#ifndef clox_debug_h
#define clox_debug_h

#include "type.h"
#include "scanner.h"
#include "vmopcodes.h"

void chunk_t_disassemble(const chunk_t *chunk, const char *name);
int chunk_t_disassemble_instruction(const chunk_t *chunk, int offset);
const char *op_code_t_to_str(const op_code_t);
const char *value_type_t_to_str(const value_type_t);
const char *token_type_t_to_str(const token_type_t);
const char *obj_type_t_to_str(const obj_type_t type);

#endif
