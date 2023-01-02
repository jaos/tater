#ifndef tater_compiler_h
#define tater_compiler_h

#include <stdbool.h>
#include "type.h"
#include "vm.h"
#include "vmopcodes.h"

obj_function_t *compiler_t_compile(const char *source, const bool debug);
void compiler_t_mark_roots(void);
#endif
