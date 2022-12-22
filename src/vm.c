#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
}

void init_vm(void)
{
    reset_stack();
    init_table_t(&vm.globals);
    init_table_t(&vm.strings);
    vm.objects = NULL;
}

void free_vm(void)
{
    free_table_t(&vm.globals);
    free_table_t(&vm.strings);
    free_objects();
}

void push(const value_t value)
{
    assert(((uintptr_t)vm.stack_top - (uintptr_t)&vm.stack) <= (sizeof(value_t) * 256));
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t pop(void)
{
    assert(vm.stack_top != vm.stack);
    vm.stack_top--;
    return *vm.stack_top;
}

static void popn(const uint8_t count)
{
    vm.stack_top -= count;
}

static value_t peek(int distance)
{
    return vm.stack_top[-1 - distance];
}

static void runtime_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction].line; // NOTE typo in book? (no .line)
    fprintf(stderr, "[line %d] in script\n", line);
    reset_stack();
}

static bool is_falsey(value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void)
{
    obj_string_t *b = AS_STRING(pop());
    obj_string_t *a = AS_STRING(pop());
    obj_string_t *str = concatenate_string(a, b);
    push(OBJ_VAL(str));
}

static interpret_result_t run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type_wrapper, op) \
    do { \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtime_error("Operands must be numbers."); \
        return INTERPRET_RUNTRIME_ERROR; \
    } \
    double b = AS_NUMBER(pop()); \
    double a = AS_NUMBER(pop()); \
    push(value_type_wrapper(a op b)); \
    } while (false)
#define READ_STRING() AS_STRING(READ_CONSTANT())

    // TODO revisit with jump table, computed goto, or direct threaded code techniques
    for (;;) {
        #ifdef DEBUG
        printf("        ");
        for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
        #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                value_t v = READ_CONSTANT();
                push(v);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_POPN: { uint8_t pop_count = READ_BYTE(); popn(pop_count); break;}
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(vm.stack[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                vm.stack[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                obj_string_t *name = READ_STRING();
                value_t value;
                if (!get_table_t(&vm.globals, OBJ_VAL(name), &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTRIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                obj_string_t *name = READ_STRING();
                set_table_t(&vm.globals, OBJ_VAL(name), peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                obj_string_t *name = READ_STRING();
                if (set_table_t(&vm.globals, OBJ_VAL(name), peek(0))) {
                    delete_table_t(&vm.globals, OBJ_VAL(name));
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTRIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                value_t b = pop();
                value_t a = pop();
                push(BOOL_VAL(values_equal(a,b)));
                break;
            }
            case OP_CONSTANT_LONG: {
                uint8_t p1 = READ_BYTE();
                uint8_t p2 = READ_BYTE();
                uint8_t p3 = READ_BYTE();
                uint32_t idx = p1 | (p2 << 8) | (p3 << 16);
                value_t v = vm.chunk->constants.values[idx];
                push(v);
                break;
            }
            case OP_NEGATE: { // TODO this can optimize by changing the value in place w/o push/pop
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTRIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTRIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT: push(BOOL_VAL(is_falsey(pop()))); break;
            case OP_PRINT: print_value(pop()); printf("\n"); break;
            case OP_RETURN: {
                return INTERPRET_OK;
            }
            default: DEBUG_LOGGER("Unhandled default\n",); exit(EXIT_FAILURE);
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef READ_STRING
}

interpret_result_t interpret(const char *source)
{
    chunk_t chunk;
    init_chunk(&chunk);

    if (!compile(source, &chunk)) {
        free_chunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    interpret_result_t r = run();
    free_chunk(&chunk);
    return r;
}
