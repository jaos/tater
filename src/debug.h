#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"

void disassemble_chunk(chunk_t* chunk, const char* name);
int disassemble_instruction(chunk_t* chunk, const int offset);

#endif
