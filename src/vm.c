#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#define GC_HEAP_GROW_FACTOR 2

vm_t vm;

void vm_toggle_gc_stress(void)
{
    vm.flags ^= VM_FLAG_GC_STRESS;
}

void vm_toggle_gc_trace(void)
{
    vm.flags ^= VM_FLAG_GC_TRACE;
}

void vm_toggle_stack_trace(void)
{
    vm.flags ^= VM_FLAG_STACK_TRACE;
}

static bool clock_native(const int, const value_t*)
{
    vm_push(NUMBER_VAL((double)clock()));
    return true;
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
    fprintf(stderr, "\n");

    for (int i = vm.frame_count - 1; i >= 0; i--) {
        const call_frame_t *frame = &vm.frames[i];
        const obj_function_t *function = frame->closure->function;
        const size_t instruction = frame->ip - function->chunk.code - 1; // previous failed instruction
        fprintf(stderr, "[line %d] in %s\n",
            chunk_t_get_line(&function->chunk, instruction),
            function->name == NULL ? "script" : function->name->chars
        );
    }
    reset_stack();
}

void vm_define_native(const char *name, const native_fn_t function, const int arity)
{
    vm_push(OBJ_VAL(obj_string_t_copy_from(name, (int)strlen(name))));
    vm_push(OBJ_VAL(obj_native_t_allocate(function, AS_STRING(vm.stack[0]), arity)));
    table_t_set(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    vm_pop();
    vm_pop();
}

static bool has_field_native(const int arg_count, const value_t *args)
{
    if (arg_count != 2) {
        return false;
    }

    if (!IS_INSTANCE(args[0]))
        return false;

    if (!IS_STRING(args[1]))
        return false;

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    value_t v;
    vm_push(BOOL_VAL(table_t_get(&instance->fields, AS_STRING(args[1]), &v)));
    return true;
}

static bool is_instance_native(const int arg_count, const value_t *args)
{
    if (arg_count != 2) {
        return false;
    }

    /* built in types*/
    if (IS_STRING(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "str", 3) == 0) {
        vm_push(TRUE_VAL); return true;
    }
    if (!IS_STRING(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "str", 3) == 0) {
        vm_push(FALSE_VAL); return true;
    }
    if (IS_LIST(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "list", 3) == 0) {
        vm_push(TRUE_VAL); return true;
    }
    if (!IS_LIST(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "list", 3) == 0) {
        vm_push(FALSE_VAL); return true;
    }
    if (IS_NIL(args[0]) && IS_NIL(args[1])) {
        vm_push(TRUE_VAL); return true;
    }
    if (!IS_NIL(args[0]) && IS_NIL(args[1])) {
        vm_push(FALSE_VAL); return true;
    }
    if (IS_BOOL(args[0]) && IS_BOOL(args[1])) {
        vm_push(TRUE_VAL); return true;
    }
    if (!IS_BOOL(args[0]) && IS_BOOL(args[1])) {
        vm_push(FALSE_VAL); return true;
    }
    if (IS_NUMBER(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "number", 6) == 0) {
        vm_push(TRUE_VAL); return true;
    }
    if (!IS_NUMBER(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "number", 6) == 0) {
        vm_push(FALSE_VAL); return true;
    }
    if (IS_MAP(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "map", 3) == 0) {
        vm_push(TRUE_VAL); return true;
    }
    if (!IS_MAP(args[0]) && IS_NATIVE(args[1]) && memcmp(AS_NATIVE(args[1])->name->chars, "map", 3) == 0) {
        vm_push(FALSE_VAL); return true;
    }

    // class instances
    if (!IS_INSTANCE(args[0])) {
        vm_push(FALSE_VAL); return true;
    }
    if (!IS_CLASS(args[1])) {
        vm_push(FALSE_VAL); return true;
    }
    obj_instance_t *instance = AS_INSTANCE(args[0]);
    obj_class_t *cls = AS_CLASS(args[1]);
    if (instance->cls == cls) {
        vm_push(TRUE_VAL);
    } else {
        vm_push(FALSE_VAL);
    }
    return true;
}

static bool get_field_native(const int arg_count, const value_t *args)
{
    if (arg_count != 2 || !IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
        runtime_error(gettext("get_field requires an object and a string field name."));
        return false;
    }

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    value_t v = NIL_VAL;
    table_t_get(&instance->fields, AS_STRING(args[1]), &v);
    vm_push(v);
    return true;
}

static bool set_field_native(const int arg_count, const value_t *args)
{
    if (arg_count != 3 || !IS_INSTANCE(args[0])) {
        runtime_error(gettext("set_field requires instance, field name, and value."));
        return false;
    }

    obj_instance_t *instance = AS_INSTANCE(args[0]);
    table_t_set(&instance->fields, AS_STRING(args[1]), args[2]);
    vm_push(args[2]);
    return true;
}

static bool sys_version_native(const int, const value_t *)
{
    vm_push(OBJ_VAL(obj_string_t_copy_from(VERSION, strlen(VERSION))));
    return true;
}

static bool str_native(const int arg_count, const value_t *args)
{
    if (arg_count != 1) {
        vm_push(OBJ_VAL(obj_string_t_copy_from("", 0)));
        return true;
    }

    value_t v = args[0];
    vm_push(OBJ_VAL(value_t_to_obj_string_t(v)));
    return true;
}

static bool list_native(const int argc, const value_t *args)
{
    obj_list_t *list = obj_list_t_allocate();
    vm_push(OBJ_VAL(list));
    for (int i = 0 ; i < argc; i++) {
        value_array_t_add(&list->elements, args[i]);
    }
    return true;
}

static bool map_native(const int argc, const value_t *args)
{
    obj_map_t *map = obj_map_t_allocate();
    vm_push(OBJ_VAL(map));

    if (argc % 2) {
        runtime_error(gettext("Cannot initialize a map with an odd number of arguments."));
        return false;
    }
    for (int i = 0; i < argc; i += 2) {
        if (!IS_STRING(args[i])) {
            runtime_error(gettext("Map keys can only be strings."));
            return false;
        }
        table_t_set(&map->table, AS_STRING(args[i]), args[i+1]);
    }
    return true;
}

static bool number_native(const int arg_count, const value_t *args)
{
    if (arg_count != 1) {
        runtime_error(gettext("number requires on argument."));
        return false;
    }

    if (IS_NUMBER(args[0])) {
        vm_push(args[0]);
        return true;
    }
    if (IS_STRING(args[0])) {
        double n = strtod(AS_CSTRING(args[0]), NULL);
        vm_push(NUMBER_VAL(n));
        return true;
    }
    if (IS_BOOL(args[0])) {
        bool v = AS_BOOL(args[0]);
        vm_push(v ? NUMBER_VAL(1) : NUMBER_VAL(0));
        return true;
    }
    if (IS_NIL(args[0])) {
        vm_push(NUMBER_VAL(0));
        return true;
    }

    runtime_error(gettext("number argument invalid."));
    return false;
}

static bool string_method_invoke(const obj_string_t *method, const int arg_count, const value_t *args)
{
    obj_string_t *str = AS_STRING(args[0]);

    if (method->length == 3 && memcmp(method->chars, KEYWORD_LEN, KEYWORD_LEN_LEN) == 0) {
        if (arg_count != 1) {
            runtime_error(gettext("str.len takes no arguments."));
            return false;
        }
        vm_push(NUMBER_VAL(str->length));
        return true;
    }

    else if (method->length == 6 && memcmp(method->chars, "substr", 6) == 0) {
        if (arg_count != 3 || !IS_OBJ(args[0]) || !IS_STRING(args[0]) || !IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) {
            runtime_error(gettext("str.substr requires a string argument and a start position and a length."));
            return false;
        }
        int start = (int)AS_NUMBER(args[1]);
        if (start < 0) {
            start += str->length;
        }
        int end = start + (int)AS_NUMBER(args[2]);
        if (start < 0) {
            runtime_error(gettext("invalid str.substr start position."));
            return false;
        }
        if (end > str->length) {
            runtime_error(gettext("invalid str.substr end position."));
            return false;
        }
        vm_push(OBJ_VAL(obj_string_t_copy_from(str->chars + start, end-start)));
        return true;
    }

    else if (method->length == 9 && memcmp(method->chars, KEYWORD_SUBSCRIPT, KEYWORD_SUBSCRIPT_LEN) == 0) {
        if (arg_count != 2 || !IS_OBJ(args[0]) || !IS_STRING(args[0]) || !IS_NUMBER(args[1])) {
            runtime_error(gettext("str.subscript requires a string argument and a position."));
            return false;
        }
        int start = (int)AS_NUMBER(args[1]);
        if (start < 0) {
            start += str->length;
        }
        int end = start + 1;
        if (start < 0 || end > str->length) {
            runtime_error(gettext("invalid str.substr end position."));
            return false;
        }
        vm_push(OBJ_VAL(obj_string_t_copy_from(str->chars + start, end-start)));
        return true;
    }

    runtime_error(gettext("No such str method %.*s"), method->length, method->chars);
    return false;
}

static bool list_method_invoke(const obj_string_t *method, const int arg_count, const value_t *args)
{
    obj_list_t *list = AS_LIST(args[0]);

    if (method->length == 3) {
        if (memcmp(method->chars, KEYWORD_LEN, KEYWORD_LEN_LEN) == 0) {
            if (arg_count > 1) {
                runtime_error(gettext("list.len takes no arguments."));
                return false;
            }
            vm_push(NUMBER_VAL(list->elements.count));
            return true;
        }
        else if (memcmp(method->chars, KEYWORD_GET, KEYWORD_GET_LEN) == 0) {
            if (arg_count != 2) {
                runtime_error(gettext("list.get requires a single numerical argument."));
                return false;
            }
            if (!IS_NUMBER(args[1])) {
                runtime_error(gettext("list.get requires a single numerical argument."));
                return false;
            }
            int index = (int)AS_NUMBER(args[1]);
            if (index < 0) {
                index += list->elements.count;
            }
            if (index < 0 || index > list->elements.count - 1) {
                runtime_error(gettext("invalid list.get index."));
                return false;
            }
            value_t v = list->elements.values[index];
            vm_push(v);
            return true;
        }
    }

    else if (method->length == 5) {
        if (memcmp(method->chars, KEYWORD_CLEAR, KEYWORD_CLEAR_LEN) == 0) {
            if (arg_count != 1) {
                runtime_error(gettext("list.clear requires no arguments."));
                return false;
            }
            list->elements.count = 0;
            vm_push(NIL_VAL);
            return true;
        }
    }

    else if (method->length == 6) {
        if (memcmp(method->chars, KEYWORD_APPEND, KEYWORD_APPEND_LEN) == 0) {
            if (arg_count != 2) {
                runtime_error(gettext("list.append requires a single argument."));
                return false;
            }
            value_t to_add = args[1];
            value_array_t_add(&list->elements, to_add);
            vm_push(to_add);
            return true;
        }
        if (memcmp(method->chars, KEYWORD_REMOVE, KEYWORD_REMOVE_LEN) == 0) {
            if (list->elements.count == 0) {
                vm_push(NUMBER_VAL(list->elements.count));
                return true;
            }
            if (arg_count != 2) {
                runtime_error(gettext("list.remove requires a single argument."));
                return false;
            }
            if (!IS_NUMBER(args[1])) {
                runtime_error(gettext("list.remove requires a single numerical argument."));
                return false;
            }
            int index = (int)AS_NUMBER(args[1]);
            if (index < 0) {
                index += list->elements.count;
            }
            if (index < 0 || index > list->elements.count - 1) {
                runtime_error(gettext("invalid list.remove index."));
                return false;
            }
            if (index == list->elements.count - 1) {
                list->elements.count--;
                vm_push(NUMBER_VAL(list->elements.count));
                return true;
            } else {
                for (int i = index; i < list->elements.count; i++) {
                    value_t v = list->elements.values[i];
                    list->elements.values[i] = list->elements.values[i + 1];
                    list->elements.values[i + 1] = v;
                }
                list->elements.count--;
                vm_push(NUMBER_VAL(list->elements.count));
                return true;
            }
        }
    }

    else if (method->length == 9 && memcmp(method->chars, KEYWORD_SUBSCRIPT, KEYWORD_SUBSCRIPT_LEN) == 0) {
        if (arg_count != 2) {
            runtime_error(gettext("list.subscript requires a single numerical argument."));
            return false;
        }
        if (!IS_NUMBER(args[1])) {
            runtime_error(gettext("list.subscript requires a single numerical argument."));
            return false;
        }
        int index = (int)AS_NUMBER(args[1]);
        if (index < 0) {
            index += list->elements.count;
        }
        if (index < 0 || index > list->elements.count - 1) {
            runtime_error(gettext("invalid list.subscript index."));
            return false;
        }
        value_t v = list->elements.values[index];
        vm_push(v);
        return true;
    }

    runtime_error(gettext("No such list method %.*s"), method->length, method->chars);
    return false;
}

static bool map_method_invoke(const obj_string_t *method, const int arg_count, const value_t *args)
{
    obj_map_t *map = AS_MAP(args[0]);

    if (method->length == 3) {
        if (memcmp(method->chars, KEYWORD_LEN, KEYWORD_LEN_LEN) == 0) {
            if (arg_count > 1) {
                runtime_error(gettext("map.len takes no arguments."));
                return false;
            }
            vm_push(NUMBER_VAL(map->table.count));
            return true;
        }
        else if (memcmp(method->chars, KEYWORD_GET, KEYWORD_GET_LEN) == 0) {
            if (arg_count != 2 || !IS_STRING(args[1])) {
                runtime_error(gettext("map.get requires a single string argument."));
                return false;
            }
            value_t v;
            if (table_t_get(&map->table, AS_STRING(args[1]), &v)) {
                vm_push(v);
            } else {
                vm_push(NIL_VAL);
            }
            return true;
        }
        else if (memcmp(method->chars, KEYWORD_SET, KEYWORD_SET_LEN) == 0) {
            if (arg_count != 3 || !IS_STRING(args[1])) {
                runtime_error(gettext("map.set requires a string and a value argument."));
                return false;
            }
            value_t v = args[2];
            if (table_t_set(&map->table, AS_STRING(args[1]), v)) {
                vm_push(NIL_VAL);
                return true;
            }
            runtime_error(gettext("map.set failed."));
            return false;
        }
    }

    else if (method->length == 4 && memcmp(method->chars, KEYWORD_KEYS, KEYWORD_KEYS_LEN) == 0) {
        if (arg_count > 1) {
            runtime_error(gettext("map.keys takes no arguments."));
            return false;
        }
        obj_list_t *keys = obj_list_t_allocate();
        vm_push(OBJ_VAL(keys));
        for (int i = 0; i < map->table.capacity; i++) {
            entry_t entry = map->table.entries[i];
            if (entry.key != NULL) {
                value_array_t_add(&keys->elements, OBJ_VAL(entry.key));
            }
        }
        return true;
    }

    else if (method->length == 6) {
        if (memcmp(method->chars, KEYWORD_REMOVE, KEYWORD_REMOVE_LEN) == 0) {
            if (arg_count != 2 || !IS_STRING(args[1])) {
                runtime_error(gettext("map.remove requires a single string argument."));
                return false;
            }
            table_t_delete(&map->table, AS_STRING(args[1]));
            vm_push(NIL_VAL);
            return true;
        }
        else if (memcmp(method->chars, KEYWORD_VALUES, KEYWORD_VALUES_LEN) == 0) {
            if (arg_count != 1) {
                runtime_error(gettext("map.values requires no arguments."));
                return false;
            }
            obj_list_t *values = obj_list_t_allocate();
            vm_push(OBJ_VAL(values));
            for (int i = 0; i < map->table.capacity; i++) {
                entry_t entry = map->table.entries[i];
                if (entry.key != NULL) {
                    value_array_t_add(&values->elements, entry.value);
                }
            }
            return true;
        }
    }

    else if (method->length == 9 && memcmp(method->chars, KEYWORD_SUBSCRIPT, KEYWORD_SUBSCRIPT_LEN) == 0) {
        if (arg_count != 2 || !IS_STRING(args[1])) {
            runtime_error(gettext("map.subscript requires a single string argument."));
            return false;
        }
        value_t v;
        if (table_t_get(&map->table, AS_STRING(args[1]), &v)) {
            vm_push(v);
        } else {
            vm_push(NIL_VAL);
        }
        return true;
    }

    runtime_error(gettext("No such map method %.*s"), method->length, method->chars);
    return false;
}

void vm_t_init(void)
{
    reset_stack();
    vm.objects = NULL;
    vm.bytes_allocated = 0;
    vm.next_garbage_collect = 1024 * 1024;
    vm.flags = 0;
    vm.exit_status = 0;

    vm.gray_count = 0;
    vm.gray_capacity = 0;
    vm.gray_stack = NULL;

    table_t_init(&vm.globals);
    table_t_init(&vm.strings);
    vm.init_string = NULL; // in case of GC race inside obj_string_t_copy_from that allocates
    vm.init_string = obj_string_t_copy_from(KEYWORD_INIT, KEYWORD_INIT_LEN);

    vm_define_native("clock", clock_native, 0);
    vm_define_native("has_field", has_field_native, 2);
    vm_define_native("is", is_instance_native, 2);
    vm_define_native("sys_version", sys_version_native, 0);
    vm_define_native("get_field", get_field_native, 2);
    vm_define_native("set_field", set_field_native, 3);
    vm_define_native("str", str_native, -1);
    vm_define_native("list", list_native, -1);
    vm_define_native("number", number_native, -1);
    vm_define_native("map", map_native, -1);
}

void vm_set_argc_argv(const int argc, const char *argv[])
{
    obj_string_t *argc_str = obj_string_t_copy_from("argc", 4);
    vm_push(OBJ_VAL(argc_str));
    value_t v = NUMBER_VAL(argc);
    table_t_set(&vm.globals, argc_str, v);
    vm_pop();

    obj_string_t *argv_str = obj_string_t_copy_from("argv", 4);
    vm_push(OBJ_VAL(argv_str));
    obj_list_t *argv_list = obj_list_t_allocate();
    vm_push(OBJ_VAL(argv_list));
    for (int i = 0; i < argc; i++) {
        obj_string_t *arg = obj_string_t_copy_from(argv[i], strlen(argv[i]));
        vm_push(OBJ_VAL(arg));
        value_array_t_add(&argv_list->elements, OBJ_VAL(arg));
        vm_pop();
    }
    table_t_set(&vm.globals, argv_str, OBJ_VAL(argv_list));
    vm_pop();
    vm_pop();
}

extern char **environ;
void vm_inherit_env(void)
{
    obj_string_t *env_global_name = obj_string_t_copy_from("env", 3);
    vm_push(OBJ_VAL(env_global_name));

    obj_map_t *env_map = obj_map_t_allocate();
    vm_push(OBJ_VAL(env_map));

    char **env = environ;
    while (*env != NULL) {

        const int env_len = strlen(*env);
        const char *delim_offset = index(*env, '=') + 1;
        const int from_delim_len = strlen(delim_offset);
        const int to_delim_len = env_len - from_delim_len - 1;

        obj_string_t *env_name = obj_string_t_copy_from(*env, to_delim_len);
        vm_push(OBJ_VAL(env_name));
        obj_string_t *env_value = obj_string_t_copy_from(delim_offset, from_delim_len);
        vm_push(OBJ_VAL(env_value));
        table_t_set(&env_map->table, env_name, OBJ_VAL(env_value));
        vm_pop();
        vm_pop();

        env++;
    }
    table_t_set(&vm.globals, env_global_name, OBJ_VAL(env_map));
    vm_pop();
    vm_pop();
}

static void mark_array(value_array_t *array);
static void blacken_object(obj_t *object);
static void vm_t_free_object(obj_t *o);
static void mark_roots(void);
static void trace_references(void);
static void sweep(void);

void vm_collect_garbage(void)
{
    vm_gc_toggle_active();
    size_t before = vm.bytes_allocated;
    if (vm.flags & VM_FLAG_GC_TRACE) {
        printf("== start gc\n");
    }

    mark_roots();
    trace_references();
    table_t_remove_white(&vm.strings);
    sweep();

    vm.next_garbage_collect = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

    if (vm.flags & VM_FLAG_GC_TRACE) {
        printf("==   end gc\n");
        printf("           collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm.bytes_allocated,
            before,
            vm.bytes_allocated,
            vm.next_garbage_collect);
    }
    vm_gc_toggle_active();
}

static void vm_t_free_objects(void)
{
    obj_t *o = vm.objects;
    while (o != NULL) {
        obj_t *next = o->next;
        vm_t_free_object(o);
        o = next;
    }
}

void vm_t_free(void)
{
    table_t_free(&vm.globals);
    table_t_free(&vm.strings);
    vm.init_string = NULL; // before free_objects so it cleans it up for us
    vm_t_free_objects();
    free(vm.gray_stack);
}

void vm_push(const value_t value)
{
    assert(((uintptr_t)vm.stack_top - (uintptr_t)&vm.stack) <= (sizeof(value_t) * 256));
    *vm.stack_top = value;
    vm.stack_top++;
}

value_t vm_pop(void)
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
    if (closure->function->arity >= 0 && arg_count != closure->function->arity) {
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
                vm.stack_top[-arg_count - 1] = bound_method->receiving_instance; // swap out our instance for "this" referencing
                return call(bound_method->method, arg_count);
            }
            case OBJ_BOUND_NATIVE_METHOD: {
                obj_bound_native_method_t *bound_native_method = AS_BOUND_NATIVE_METHOD(callee);
                vm.stack_top[-arg_count - 1] = bound_native_method->receiving_instance; // swap out our instance
                value_t *args = vm.stack_top - arg_count - 1;
                vm.stack_top -= arg_count + 1;
                return bound_native_method->function(bound_native_method->name, arg_count + 1, args);
            }
            case OBJ_CLASS: {
                obj_class_t *cls = AS_CLASS(callee);
                vm.stack_top[-arg_count - 1] = OBJ_VAL(obj_instance_t_allocate(cls));
                value_t initializer;
                if (table_t_get(&cls->methods, vm.init_string, &initializer)) {
                    return call(AS_CLOSURE(initializer), arg_count);
                } else if (arg_count != 0) {
                    runtime_error(gettext("Expected 0 arguments but got %d to initialize %s."), arg_count, cls->name->chars);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE: return call(AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE: {
                const obj_native_t *native = AS_NATIVE(callee);
                if (native->arity >= 0 && arg_count != native->arity) {
                    runtime_error(gettext("%s expected %d arguments but got %d."), native->name->chars, native->arity, arg_count);
                    return false;
                }
                if(!native->function(arg_count, vm.stack_top - arg_count)) {
                    return false;
                }
                value_t r = vm_pop();
                vm.stack_top -= arg_count + 1;
                vm_push(r);
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
    if (!table_t_get(&cls->methods, name, &method)) {
        runtime_error(gettext("Undefined property '%s'."), name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), arg_count);
}

static bool invoke(const obj_string_t *name, const int arg_count)
{
    const value_t receiving_instance = peek(arg_count); // instance is already on the stack for us

    /* dispatch to native helpers*/
    if (IS_STRING(receiving_instance)) {
        value_t *args = vm.stack_top - arg_count - 1;
        vm.stack_top -= arg_count + 1;
        bool rv = string_method_invoke(name, arg_count + 1, args); // leaving arg on the stack
        return rv;
    }
    else if (IS_LIST(receiving_instance)) {
        value_t *args = vm.stack_top - arg_count - 1;
        vm.stack_top -= arg_count + 1;
        bool rv = list_method_invoke(name, arg_count + 1, args); // leaving arg on the stack
        return rv;
    }
    else if (IS_MAP(receiving_instance)) {
        value_t *args = vm.stack_top - arg_count - 1;
        vm.stack_top -= arg_count + 1;
        bool rv = map_method_invoke(name, arg_count + 1, args); // leaving arg on the stack
        return rv;
    }
    // TODO number, bool?

    // otherwise lox class
    else if (IS_INSTANCE(receiving_instance)) {
        obj_instance_t *instance = AS_INSTANCE(receiving_instance);

        // priority... do not invoke a field that is a function like a method
        value_t function_value;
        if (table_t_get(&instance->fields, name, &function_value)) {
            vm.stack_top[-arg_count - 1] = function_value; // swap receiving_instance for our function
            return call_value(function_value, arg_count);
        }
        return invoke_from_class(instance->cls, name, arg_count);
    }

    else {
        runtime_error(gettext("Only instances have methods."));
        return false;
    }
    return false;
}

static bool bind_method(obj_class_t *cls, const obj_string_t *name)
{
    value_t method;
    if (!table_t_get(&cls->methods, name, &method)) {
        runtime_error(gettext("Undefined property '%s'."), name->chars);
        return false;
    }
    obj_bound_method_t *bound_method = obj_bound_method_t_allocate(peek(0), AS_CLOSURE(method));

    // pop the instance and replace with the bound method
    vm_pop();
    vm_push(OBJ_VAL(bound_method));
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

    obj_upvalue_t *created_upvalue = obj_upvalue_t_allocate(local);
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
    table_t_set(&cls->methods, name, method);
    vm_pop();
}

static bool is_falsey(const value_t value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void)
{
    obj_string_t *b = AS_STRING(peek(0));
    obj_string_t *a = AS_STRING(peek(1));

    const int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    obj_string_t *result = obj_string_t_copy_own(chars, length);
    vm_pop(); // make GC happy
    vm_pop(); // make GC happy
    vm_push(OBJ_VAL(result));
}

static vm_t_interpret_result_t run(void)
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
        const double b = AS_NUMBER(vm_pop()); \
        const double a = AS_NUMBER(vm_pop()); \
        vm_push(value_type_wrapper(a op b)); \
    } while (false)

    // TODO revisit with jump table, computed goto, or direct threaded code techniques
    for (;;) {
        if (vm.flags & VM_FLAG_STACK_TRACE) {
            printf("                    ");
            for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
                printf("[ ");
                value_t_print(*slot);
                printf(" ]");
            }
            printf("\n");

            // TODO on errors we might end up with nothing here and blow up, including the instruction READ_BYTE below...
            if (!frame->ip) {
                fprintf(stderr, "FIXME I am a work in progress!\n");
                exit(EXIT_FAILURE);
            }
            chunk_t_disassemble_instruction(
                &frame->closure->function->chunk,
                (int)(frame->ip - frame->closure->function->chunk.code)
            );
        }
        assert(frame);
        assert(frame->ip);

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {
            case OP_CONSTANT: {
                const value_t v = READ_CONSTANT();
                vm_push(v);
                break;
            }
            case OP_NIL: vm_push(NIL_VAL); break;
            case OP_TRUE: vm_push(TRUE_VAL); break;
            case OP_FALSE: vm_push(FALSE_VAL); break;
            case OP_POP: vm_pop(); break;
            case OP_GET_LOCAL: {
                const uint8_t slot = READ_BYTE();
                vm_push(frame->slots[slot]);
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
                if (!table_t_get(&vm.globals, name, &value)) {
                    runtime_error(gettext("Undefined variable '%s'."), name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm_push(value);
                break;
            }
            case OP_DEFINE_GLOBAL: {
                obj_string_t *name = READ_STRING();
                table_t_set(&vm.globals, name, peek(0));
                vm_pop();
                break;
            }
            case OP_SET_GLOBAL: {
                obj_string_t *name = READ_STRING();
                if (table_t_set(&vm.globals, name, peek(0))) {
                    table_t_delete(&vm.globals, name);
                    runtime_error(gettext("Undefined variable '%s'."), name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_UPVALUE: {
                const uint8_t slot = READ_BYTE();
                vm_push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE: {
                const uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }
            case OP_GET_PROPERTY: {
                // native helpers
                if (IS_STRING(peek(0))) {
                    obj_string_t *name = READ_STRING();
                    obj_bound_native_method_t *m = obj_bound_native_method_t_allocate(peek(0), name, string_method_invoke);
                    vm_pop();
                    vm_push(OBJ_VAL(m));
                    break;
                }
                else if (IS_LIST(peek(0))) {
                    obj_string_t *name = READ_STRING();
                    obj_bound_native_method_t *m = obj_bound_native_method_t_allocate(peek(0), name, list_method_invoke);
                    vm_pop();
                    vm_push(OBJ_VAL(m));
                    break;
                }
                else if (IS_MAP(peek(0))) {
                    obj_string_t *name = READ_STRING();
                    obj_bound_native_method_t *m = obj_bound_native_method_t_allocate(peek(0), name, map_method_invoke);
                    vm_pop();
                    vm_push(OBJ_VAL(m));
                    break;
                }
                // TODO number, bool?

                // otherwise lox
                else if (IS_INSTANCE(peek(0))) {
                    obj_instance_t *instance = AS_INSTANCE(peek(0));
                    const obj_string_t *name = READ_STRING();

                    // fields (priority, may shadow methods)
                    value_t value;
                    if (table_t_get(&instance->fields, name, &value)) {
                        vm_pop(); // instance
                        vm_push(value);
                        break;
                    }

                    // otherwise methods
                    if (!bind_method(instance->cls, name)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    runtime_error(gettext("Only instances have properties."));
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
                table_t_set(&instance->fields, READ_STRING(), peek(0)); // read name, peek the value to set
                const value_t value = vm_pop(); // pop the value
                vm_pop(); // pop the instance
                vm_push(value); // push the value so we leave the value as the return
                break;
            }
            case OP_GET_SUPER: {
                const obj_string_t *method_name = READ_STRING();
                obj_class_t *superclass = AS_CLASS(vm_pop());
                // NOTE this is only for methods, not fields
                if (!bind_method(superclass, method_name)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_EQUAL: {
                const value_t b = vm_pop();
                const value_t a = vm_pop();
                vm_push(BOOL_VAL(value_t_equal(a,b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD: {
                if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                    concatenate();
                } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                    const double b = AS_NUMBER(vm_pop());
                    const double a = AS_NUMBER(vm_pop());
                    vm_push(NUMBER_VAL(a + b));
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
            case OP_NOT: vm_push(BOOL_VAL(is_falsey(vm_pop()))); break;
            case OP_NEGATE: { // TODO this can optimize by changing the value in place w/o push/pop
                if (!IS_NUMBER(peek(0))) {
                    runtime_error(gettext("Operand must be a number."));
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm_push(NUMBER_VAL(-AS_NUMBER(vm_pop())));
                break;
            }
            case OP_PRINT: {
                value_t_print(vm_pop());
                printf("\n");
                break;
            }
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
                obj_class_t *superclass = AS_CLASS(vm_pop());
                if (!invoke_from_class(superclass, method_name, arg_count)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frame_count - 1]; // move to new call_frame_t
                break;
            }
            case OP_CLOSURE: {
                obj_closure_t *closure = obj_closure_t_allocate(AS_FUNCTION(READ_CONSTANT()));
                vm_push(OBJ_VAL(closure));
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
                vm_pop();
                break;
            }
            case OP_RETURN: {
                const value_t result = vm_pop();
                close_upvalues(frame->slots); // close the remaining open upvalues owned by the returning function
                vm.frame_count--;
                if (vm.frame_count == 0) {
                    vm_pop();
                    return INTERPRET_OK;
                }
                vm.stack_top = frame->slots;
                vm_push(result);
                frame = &vm.frames[vm.frame_count - 1]; // move to new call_frame_t
                break;
            }
            case OP_EXIT: {
                const value_t exit_code = vm_pop();
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
                vm_push(OBJ_VAL(obj_class_t_allocate(READ_STRING())));
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
                table_t_copy_to(&AS_CLASS(superclass)->methods, &subclass->methods);
                vm_pop();
                break;
            }
            case OP_METHOD: {
                define_method(READ_STRING());
                break;
            }
            case OP_CONSTANT_LONG: {
                assert(false);
                const uint8_t p1 = READ_BYTE();
                const uint8_t p2 = READ_BYTE();
                const uint8_t p3 = READ_BYTE();
                const uint32_t idx = p1 | (p2 << 8) | (p3 << 16);
                const value_t v = frame->closure->function->chunk.constants.values[idx];
                vm_push(v);
                break;
            }
            case OP_POPN: { uint8_t pop_count = READ_BYTE(); popn(pop_count); break;}
            case OP_DUP: vm_push(peek(0)); break;
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

vm_t_interpret_result_t vm_t_interpret(const char *source)
{
    obj_function_t *function = compiler_t_compile(source, vm.flags & VM_FLAG_STACK_TRACE);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    vm_push(OBJ_VAL(function));
    obj_closure_t *closure = obj_closure_t_allocate(function);
    vm_pop();
    vm_push(OBJ_VAL(closure));
    call(closure, 0);

    return run();
}

static void mark_array(value_array_t *array)
{
    for (int i = 0; i < array->count; i++) {
        value_t_mark(array->values[i]);
    }
}

static void blacken_object(obj_t *object)
{
    if (vm.flags & VM_FLAG_GC_TRACE) {
        printf("%p blacken ", (void*)object);
        value_t_print(OBJ_VAL(object));
        printf("\n");
    }

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            obj_bound_method_t *bound_method = (obj_bound_method_t*)object;
            value_t_mark(bound_method->receiving_instance); // should be an obj_instance_t that is already marked... but insurance here
            obj_t_mark((obj_t*)bound_method->method);
            break;
        }
        case OBJ_BOUND_NATIVE_METHOD: {
            obj_bound_native_method_t *bound_native_method = (obj_bound_native_method_t*)object;
            obj_t_mark((obj_t*)bound_native_method->name);
            value_t_mark(bound_native_method->receiving_instance); // should be an obj_instance_t that is already marked... but insurance here
            break;
        }
        case OBJ_CLASS: {
            obj_class_t *cls = (obj_class_t*)object;
            obj_t_mark((obj_t*)cls->name);
            table_t_mark(&cls->methods);
            break;
        }
        case OBJ_CLOSURE: {
            obj_closure_t *closure = (obj_closure_t*)object;
            obj_t_mark((obj_t*)closure->function);
            for (int i = 0; i < closure->upvalue_count; i++) {
                obj_t_mark((obj_t*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)object;
            obj_t_mark((obj_t*)function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            obj_instance_t *instance = (obj_instance_t*)object;
            obj_t_mark((obj_t*)instance->cls);
            table_t_mark(&instance->fields);
            break;
        }
        case OBJ_LIST: {
            obj_list_t *list = (obj_list_t*)object;
            mark_array(&list->elements);
            break;
        }
        case OBJ_MAP: {
            obj_map_t *map = (obj_map_t*)object;
            table_t_mark(&map->table);
            break;
        }
        case OBJ_UPVALUE: {
            value_t_mark(((obj_upvalue_t*)object)->closed);
            break;
        }
        case OBJ_NATIVE: {
            obj_native_t *native = (obj_native_t*)object;
            obj_t_mark((obj_t*)native->name);
        }
        case OBJ_STRING:
            break;
        default: return;
    }
}

static void vm_t_free_object(obj_t *o)
{
    if (vm.flags & VM_FLAG_GC_TRACE) {
        printf("%p free type %s\n", (void*)o, obj_type_t_to_str(o->type));
    }
    switch (o->type) {
        case OBJ_BOUND_METHOD: {
            FREE(obj_bound_method_t, o);
            break;
        }
        case OBJ_BOUND_NATIVE_METHOD: {
            FREE(obj_bound_native_method_t, o);
            break;
        }
        case OBJ_CLASS: {
            obj_class_t *cls = (obj_class_t*)o;
            table_t_free(&cls->methods);
            FREE(obj_class_t, o); // TODO obj_string_t ?
            break;
        }
        case OBJ_CLOSURE: {
            obj_closure_t *closure = (obj_closure_t*)o;
            // free the containing array, not the actual upvalues themselves
            FREE_ARRAY(obj_upvalue_t*, closure->upvalues, closure->upvalue_count);
            FREE(obj_closure_t, o); // leave the function
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)o;
            chunk_t_free(&function->chunk);
            FREE(obj_function_t, o);
            break;
        }
        case OBJ_INSTANCE: {
            obj_instance_t *instance = (obj_instance_t*)o;
            table_t_free(&instance->fields);
            FREE(obj_instance_t, o);
            break;
        }
        case OBJ_NATIVE: {
            FREE(obj_native_t, o);
            break;
        }
        case OBJ_STRING: {
            obj_string_t *s = (obj_string_t*)o;
            //reallocate(o, sizeof(obj_string_t) + s->length + 1, 0);
            FREE_ARRAY(char, s->chars, s->length + 1);
            FREE(obj_string_t, o);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(obj_upvalue_t, o); // leave location value_t reference
            break;
        }
        case OBJ_LIST: {
            obj_list_t *t = (obj_list_t*)o;
            value_array_t_free(&t->elements);
            FREE(obj_list_t, o);
            break;
        }
        case OBJ_MAP: {
            obj_map_t *m = (obj_map_t*)o;
            table_t_free(&m->table);
            FREE(obj_map_t, o);
            break;
        }
        default: return; // unreachable
    }
}

static void mark_roots(void)
{
    for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
        value_t_mark(*slot);
    }

    for (int i =0; i < vm.frame_count; i++) {
        obj_t_mark((obj_t*)vm.frames[i].closure);
    }

    for (obj_upvalue_t *upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        obj_t_mark((obj_t*)upvalue);
    }

    table_t_mark(&vm.globals);
    compiler_t_mark_roots();
    obj_t_mark((obj_t*)vm.init_string);
}

static void trace_references(void)
{
    while (vm.gray_count > 0) {
        obj_t *object = vm.gray_stack[--vm.gray_count];
        blacken_object(object);
    }
}

static void sweep(void)
{
    obj_t *previous = NULL;
    obj_t *object = vm.objects;
    while (object != NULL) {

        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            obj_t *unreached = object;
            object = object->next;

            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }
            vm_t_free_object(unreached);
        }
    }
}
