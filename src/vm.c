#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

vm_t vm;

static value_t clock_native(int, value_t*)
{
    return NUMBER_VAL((double)clock());
}

static void reset_stack(void)
{
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
    vm.open_upvalues = NULL;
}

static void runtime_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat"
# pragma GCC diagnostic ignored "-Wformat-security"
    vfprintf(stderr, format, args); // Flawfinder: disable
# pragma GCC diagnostic pop
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frame_count - 1; i >= 0; i--) {
        const call_frame_t *frame = &vm.frames[i];
        const obj_function_t *function = frame->closure->function;
        const size_t instruction = frame->ip - function->chunk.code - 1; // previous failed instruction
        fprintf(stderr, "[line %d] in %s\n",
            function->chunk.lines[instruction],
            function->name == NULL ? "script" : function->name->chars
        );
    }
    reset_stack();
}

void define_native(const char *name, const native_fn_t function)
{
    push(OBJ_VAL(copy_string(name, (int)strlen(name))));
    push(OBJ_VAL(new_obj_native_t(function)));
    set_table_t(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

void init_vm(void)
{
    reset_stack();
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_garbage_collect = 1024 * 1024;

    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;

    init_table_t(&vm.globals);
    init_table_t(&vm.strings);
    define_native("clock", clock_native);
}

void free_vm(void)
{
    free_table_t(&vm.globals);
    free_table_t(&vm.strings);
    free_objects();
    free(vm.gray_stack);
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

static bool call(obj_closure_t *closure, const int arg_count)
{
    if (arg_count != closure->function->arity) {
        runtime_error("Expected %d arguments but got %d.", closure->function->arity, arg_count);
        return false;
    }
    if (vm.frame_count == FRAMES_MAX) {
        runtime_error("Stack overflow.");
        return false;
    }
    call_frame_t *frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

static bool call_value(value_t callee, const int arg_count)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE: return call(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE: {
                const native_fn_t native = AS_NATIVE(callee);
                const value_t result = native(arg_count, vm.stack_top - arg_count);
                vm.stack_top -= arg_count + 1;
                push(result);
                return true;
            }
            default: break; // non-callable
        }
    }
    runtime_error("Can only call functions and classes.");
    return false;
}

static obj_upvalue_t *capture_upvalue(value_t *local)
{
    obj_upvalue_t *previous_upvalue = NULL;
    obj_upvalue_t *upvalue = vm.open_upvalues;
    while (upvalue != NULL && upvalue->location > local) {
        previous_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    obj_upvalue_t *created_upvalue = new_obj_upvalue_t(local);
    created_upvalue->next = upvalue;
    if (previous_upvalue == NULL) {
        vm.open_upvalues = created_upvalue;
    } else {
        previous_upvalue->next = created_upvalue;
    }
    return created_upvalue;
}

static void close_upvalues(value_t *last)
{
    while(vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
        obj_upvalue_t *upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static bool is_falsey(const value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void)
{
    obj_string_t *b = AS_STRING(peek(0));
    obj_string_t *a = AS_STRING(peek(1));

    // FAM const obj_string_t *str = concatenate_string(a, b);
    const int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    obj_string_t *result = take_string(chars, length);
    pop(); // make GC happy
    pop(); // make GC happy
    push(OBJ_VAL(result));
}

static interpret_result_t run() {
    call_frame_t *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_type_wrapper, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        const double b = AS_NUMBER(pop()); \
        const double a = AS_NUMBER(pop()); \
        push(value_type_wrapper(a op b)); \
    } while (false)

    // TODO revisit with jump table, computed goto, or direct threaded code techniques
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        printf("        STACK: ");
        for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
        /*
        printf("        GLOBALS: ");
        for (int i = vm.globals.count; i > 0; i--) {
            printf("[ ");
            print_value(vm.globals.entries[i-1].key);
            printf(" => ");
            print_value(vm.globals.entries[i-1].value);
            printf(" ]");
        }
        printf("\n");
        */
        #endif
        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                const value_t v = READ_CONSTANT();
                push(v);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_POP: pop(); break;
            case OP_GET_LOCAL: {
                const uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL: {
                const uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_GET_GLOBAL: {
                const obj_string_t *name = READ_STRING();
                value_t value;
                if (!get_table_t(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                obj_string_t *name = READ_STRING();
                set_table_t(&vm.globals, name, peek(0));
                pop();
                break;
            }
            case OP_SET_GLOBAL: {
                obj_string_t *name = READ_STRING();
                if (set_table_t(&vm.globals, name, peek(0))) {
                    delete_table_t(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                const uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                const uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_EQUAL: {
                const value_t b = pop();
                const value_t a = pop();
                push(BOOL_VAL(values_equal(a,b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    const double b = AS_NUMBER(pop());
                    const double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                } else {
                    runtime_error("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT: push(BOOL_VAL(is_falsey(pop()))); break;
            case OP_NEGATE: { // TODO this can optimize by changing the value in place w/o push/pop
                if (!IS_NUMBER(peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;
            }
            case OP_PRINT: print_value(pop()); printf("\n"); break;
            case OP_JUMP: {
                const uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                const uint16_t offset = READ_SHORT();
                if (is_falsey(peek(0)))
                    frame->ip += offset;
                break;
            }
            case OP_LOOP: {
                const uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL: {
                const int arg_count = READ_BYTE();
                if (!call_value(peek(arg_count), arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            case OP_CLOSURE: {
                obj_closure_t *closure = new_obj_closure_t(AS_FUNCTION(READ_CONSTANT()));
                push(OBJ_VAL(closure));
                for (int i = 0; i < closure->upvalue_count; i++) {
                    const uint8_t is_local = READ_BYTE();
                    const uint8_t index = READ_BYTE();
                    if (is_local) {
                        closure->upvalues[i] = capture_upvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] = frame->closure->upvalues[index];
                    }
                }
                break;
            }
            case OP_CLOSE_UPVALUE: {
                close_upvalues(vm.stack_top - 1);
                pop();
                break;
            }
            case OP_RETURN: {
                const value_t result = pop();
                close_upvalues(frame->slots); // close the remaining open upvalues owned by the returning function
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    pop();
                    return INTERPRET_OK;
                }
                vm.stack_top = frame->slots;
                push(result);
                frame = &vm.frames[vm.frame_count - 1];
                break;
            }
            // not in lox
            case OP_CONSTANT_LONG: {
                const uint8_t p1 = READ_BYTE();
                const uint8_t p2 = READ_BYTE();
                const uint8_t p3 = READ_BYTE();
                const uint32_t idx = p1 | (p2 << 8) | (p3 << 16);
                const value_t v = frame->closure->function->chunk.constants.values[idx];
                push(v);
                break;
            }
            case OP_POPN: { uint8_t pop_count = READ_BYTE(); popn(pop_count); break;}
            case OP_DUP: push(peek(0)); break;
            default: DEBUG_LOGGER("Unhandled default\n",); exit(EXIT_FAILURE);
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_SHORT
#undef BINARY_OP
#undef READ_STRING
}

interpret_result_t interpret(const char *source)
{
    obj_function_t *function = compile(source);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    obj_closure_t *closure = new_obj_closure_t(function);
    pop();
    push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}
