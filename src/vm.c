#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "vm.h"

vm_t vm;

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
}

void init_vm(void)
{
    reset_stack();
}

void free_vm(void)
{
}

static interpret_result_t run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
    double b = pop(); \
    double a = pop(); \
    push(a op b); \
    } while (false)

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
            case OP_CONSTANT_LONG: {
                uint8_t p1 = READ_BYTE();
                uint8_t p2 = READ_BYTE();
                uint8_t p3 = READ_BYTE();
                uint32_t idx = p1 | (p2 << 8) | (p3 << 16);
                value_t v = vm.chunk->constants.values[idx];
                push(v);
                break;
            }
            case OP_NEGATE: push(-pop()); break; // TODO this can optimize by changing the value in place w/o push/pop
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_RETURN: {
                DEBUG_LOGGER("%shandling return op\n", "");
                print_value(pop());
                printf("\n");
                return INTERPRET_OK;
            }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

interpret_result_t interpret(const char *source)
{
    compile(source);
    return INTERPRET_OK;
}

void push(value_t value)
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
