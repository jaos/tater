#ifndef clox_compiler_h
#define clox_compiler_h

#include <stdbool.h>
#include "chunk.h"
#include "object.h"
#include "vm.h"

bool compile(const char *source, chunk_t *chunk);
#endif
