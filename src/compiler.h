#ifndef clox_compiler_h
#define clox_compiler_h

#include <stdbool.h>
#include "chunk.h"

bool compile(const char *source, chunk_t *chunk);
#endif
