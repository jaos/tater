#ifndef clox_compiler_h
#define clox_compiler_h

#include <stdbool.h>
#include "chunk.h"
#include "object.h"
#include "vm.h"

obj_function_t *compiler_t_compile(const char *source);
void compiler_t_mark_roots(void);
#endif
