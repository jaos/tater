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

static value_t clock_native(const int, const value_t*)
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

static value_t has_field_native(const int arg_count, const value_t *args)
{
    if (arg_count != 2) {
        return FALSE_VAL;
    }

    if (!IS_INSTANCE(args[0]))
        return FALSE_VAL;

    if (!IS_STRING(args[1]))
        return FALSE_VAL;

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    value_t v;
    return BOOL_VAL(get_table_t(&instance->fields, AS_STRING(args[1]), &v));
}

static value_t is_instance_native(const int arg_count, const value_t *args)
{
    if (arg_count != 2) {
        return FALSE_VAL;
    }

    if (!IS_INSTANCE(args[0]))
        return FALSE_VAL;

    if (!IS_CLASS(args[1]))
        return FALSE_VAL;

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    obj_class_t *cls = AS_CLASS(args[1]);
    if (instance->cls == cls) {
        return TRUE_VAL;
    } else {
        return FALSE_VAL;
    }
}

static value_t get_field_native(const int arg_count, const value_t *args)
{
    if (arg_count != 2)
        return NIL_VAL;
    if (!IS_INSTANCE(args[0]))
        return NIL_VAL;
    if (!IS_STRING(args[1]))
        return NIL_VAL;

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    value_t v = NIL_VAL;
    get_table_t(&instance->fields, AS_STRING(args[1]), &v);
    return v;
}

static value_t set_field_native(const int arg_count, const value_t *args)
{
    if (arg_count != 3)
        return FALSE_VAL;
    if (!IS_INSTANCE(args[0]))
        return FALSE_VAL;

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    set_table_t(&instance->fields, AS_STRING(args[1]), args[2]);
    return args[2];
}

static value_t sys_version_native(const int, const value_t *)
{
    push(OBJ_VAL(copy_string(VERSION, strlen(VERSION))));
    return pop();
}

void init_vm(void)
{
    reset_stack();
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_garbage_collect = 1024 * 1024;
    vm.exit_status = 0;

    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;

    init_table_t(&vm.globals);
    init_table_t(&vm.strings);
    vm.init_string = NULL; // in case of GC race inside copy_string that allocates
    vm.init_string = copy_string(KEYWORD_INIT, KEYWORD_INIT_LEN);

    define_native("clock", clock_native);
    define_native("has_field", has_field_native);
    define_native("is_instance", is_instance_native);
    define_native("sys_version", sys_version_native);
    define_native("get_field", get_field_native);
    define_native("set_field", set_field_native);
}

void free_vm(void)
{
    free_table_t(&vm.globals);
    free_table_t(&vm.strings);
    vm.init_string = NULL; // before free_objects so it cleans it up for us
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

static value_t peek(const int distance)
{
    return vm.stack_top[-1 - distance];
}

static bool call(obj_closure_t *closure, const int arg_count)
{
    if (arg_count != closure->function->arity) {
        runtime_error(gettext("Expected %d arguments but got %d."), closure->function->arity, arg_count);
        return false;
    }
    if (vm.frame_count == FRAMES_MAX) {
        runtime_error(gettext("Stack overflow."));
        return false;
    }
    call_frame_t *frame = &vm.frames[vm.frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm.stack_top - arg_count - 1;
    return true;
}

static bool call_value(const value_t callee, const int arg_count)
{
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_BOUND_METHOD: {
                obj_bound_method_t *bound_method = AS_BOUND_METHOD(callee);
                vm.stack_top[-arg_count - 1] = bound_method->receiver; // swap out our instance for "this" referencing
                return call(bound_method->method, arg_count);
            }
            case OBJ_CLASS: {
                obj_class_t *cls = AS_CLASS(callee);
                vm.stack_top[-arg_count - 1] = OBJ_VAL(new_obj_instance_t(cls));
                value_t initializer;
                if (get_table_t(&cls->methods, vm.init_string, &initializer)) {
                    return call(AS_CLOSURE(initializer), arg_count);
                } else if (arg_count != 0) {
                    runtime_error(gettext("Expected 0 arguments but got %d to initialize %s."), arg_count, cls->name->chars);
                    return false;
                }
                return true;
            }
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
    runtime_error(gettext("Can only call functions and classes."));
    return false;
}

static bool invoke_from_class(obj_class_t *cls, const obj_string_t *name, const int arg_count)
{
    value_t method;
    if (!get_table_t(&cls->methods, name, &method)) {
        runtime_error(gettext("Undefined property '%s'."), name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), arg_count);
}

static bool invoke(const obj_string_t *name, const int arg_count)
{
    const value_t receiver = peek(arg_count); // instance is already on the stack for us
    if (!IS_INSTANCE(receiver)) {
        runtime_error(gettext("Only instances have methods."));
        return false;
    }

    obj_instance_t *instance = AS_INSTANCE(receiver);

    // priority... do not invoke a field that is a function like a method
    value_t function_value;
    if (get_table_t(&instance->fields, name, &function_value)) {
        vm.stack_top[-arg_count - 1] = function_value; // swap receiver for our function
        return call_value(function_value, arg_count);
    }
    return invoke_from_class(instance->cls, name, arg_count);
}

static bool bind_method(obj_class_t *cls, const obj_string_t *name)
{
    value_t method;
    if (!get_table_t(&cls->methods, name, &method)) {
        runtime_error(gettext("Undefined property '%s'."), name->chars);
        return false;
    }
    obj_bound_method_t *bound_method = new_obj_bound_method_t(peek(0), AS_CLOSURE(method));

    // pop the instance and replace with the bound method
    pop();
    push(OBJ_VAL(bound_method));
    return true;
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

static void close_upvalues(const value_t *last)
{
    while(vm.open_upvalues != NULL && vm.open_upvalues->location >= last) {
        obj_upvalue_t *upvalue = vm.open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.open_upvalues = upvalue->next;
    }
}

static void define_method(obj_string_t *name)
{
    value_t method = peek(0);
    obj_class_t *cls = AS_CLASS(peek(1)); // left on the stack for us by class_declaration
    set_table_t(&cls->methods, name, method);
    pop();
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

static interpret_result_t run(void)
{
    call_frame_t *frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(value_type_wrapper, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtime_error(gettext("Operands must be numbers.")); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        const double b = AS_NUMBER(pop()); \
        const double a = AS_NUMBER(pop()); \
        push(value_type_wrapper(a op b)); \
    } while (false)

    // TODO revisit with jump table, computed goto, or direct threaded code techniques
    for (;;) {
        #ifdef DEBUG_TRACE_EXECUTION
        printf("                    ");
        for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");

        // TODO on errors we might end up with nothing here and blow up, including the instruction READ_BYTE below...
        if (!frame->ip) {
            fprintf(stderr, "FIXME I am a work in progress!\n");
            exit(EXIT_FAILURE);
        }
        disassemble_instruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
        #endif
        assert(frame);
        assert(frame->ip);

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                const value_t v = READ_CONSTANT();
                push(v);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(TRUE_VAL); break;
            case OP_FALSE: push(FALSE_VAL); break;
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
                    runtime_error(gettext("Undefined variable '%s'."), name->chars);
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
                    runtime_error(gettext("Undefined variable '%s'."), name->chars);
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
            case OP_GET_PROPERTY: {
                if (!IS_INSTANCE(peek(0))) {
                    runtime_error(gettext("Only instances have properties."));
                    return INTERPRET_RUNTIME_ERROR;
                }
                obj_instance_t *instance = AS_INSTANCE(peek(0));
                const obj_string_t *name = READ_STRING();

                // fields (priority, may shadow methods)
                value_t value;
                if (get_table_t(&instance->fields, name, &value)) {
                    pop(); // instance
                    push(value);
                    break;
                }

                // otherwise methods
                if (!bind_method(instance->cls, name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1))) {
                    runtime_error(gettext("Only instances have fields."));
                    return INTERPRET_RUNTIME_ERROR;
                }
                obj_instance_t *instance = AS_INSTANCE(peek(1));
                set_table_t(&instance->fields, READ_STRING(), peek(0)); // read name, peek the value to set
                const value_t value = pop(); // pop the value
                pop(); // pop the instance
                push(value); // push the value so we leave the value as the return
                break;
            }
            case OP_GET_SUPER: {
                const obj_string_t *method_name = READ_STRING();
                obj_class_t *superclass = AS_CLASS(pop());
                // NOTE this is only for methods, not fields
                if (!bind_method(superclass, method_name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
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
                    runtime_error(gettext("Operands must be two numbers or two strings."));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: {
                if (IS_NUMBER(peek(0)) && AS_NUMBER(peek(0)) == 0) {
                    runtime_error(gettext("Illegal divide by zero."));
                    return INTERPRET_RUNTIME_ERROR;
                }
                BINARY_OP(NUMBER_VAL, /); break;
            }
            case OP_NOT: push(BOOL_VAL(is_falsey(pop()))); break;
            case OP_NEGATE: { // TODO this can optimize by changing the value in place w/o push/pop
                if (!IS_NUMBER(peek(0))) {
                    runtime_error(gettext("Operand must be a number."));
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
                frame = &vm.frames[vm.frame_count - 1]; // move to new call_frame_t
                break;
            }
            case OP_INVOKE: { // combined OP_GET_PROPERTY and OP_CALL
                const obj_string_t *method_name = READ_STRING();
                const int arg_count = READ_BYTE();
                if (!invoke(method_name, arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1]; // move to new call_frame_t
                break;
            }
            case OP_SUPER_INVOKE: { // combined OP_GET_SUPER and OP_CALL
                const obj_string_t *method_name = READ_STRING();
                const int arg_count = READ_BYTE();
                obj_class_t *superclass = AS_CLASS(pop());
                if (!invoke_from_class(superclass, method_name, arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1]; // move to new call_frame_t
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
                frame = &vm.frames[vm.frame_count - 1]; // move to new call_frame_t
                break;
            }
            case OP_EXIT: {
                const value_t exit_code = pop();
                if (!IS_NUMBER(exit_code)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm.exit_status = AS_NUMBER(exit_code);
                if (AS_NUMBER(exit_code)) {
                    return INTERPRET_EXIT;
                }
                return INTERPRET_EXIT_OK;
            }
            case OP_CLASS: {
                push(OBJ_VAL(new_obj_class_t(READ_STRING())));
                break;
            }
            case OP_INHERIT: {
                const value_t superclass = peek(1);
                if (!IS_CLASS(superclass)) {
                    runtime_error(gettext("Superclass must be a class."));
                    return INTERPRET_RUNTIME_ERROR;
                }
                obj_class_t *subclass = AS_CLASS(peek(0));
                // initialize the new subclass with copies of the superclass methods, to be optionally overridden later
                add_all_table_t(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop();
                break;
            }
            case OP_METHOD: {
                define_method(READ_STRING());
                break;
            }
            // not in lox
            case OP_CONSTANT_LONG: {
                assert(false);
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
            default:
                DEBUG_LOGGER("Unhandled default for instruction %d, %s\n",
                    instruction,
                    op_code_t_to_str(instruction)
                );
                exit(EXIT_FAILURE);
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
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
