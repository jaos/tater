#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

static obj_t *allocate_object(const size_t size, const obj_type_t type)
{
    obj_t *object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    object->next = vm.objects; // add to our vm's linked list of objects so we always have a reference to it
    vm.objects = object;
    #ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %s\n", (void*)object, size, obj_type_t_to_str(type));
    #endif
    return object;
}

obj_bound_method_t *obj_bound_method_t_allocate(value_t receiving_instance, obj_closure_t *method)
{
    obj_bound_method_t *bound_method = ALLOCATE_OBJ(obj_bound_method_t, OBJ_BOUND_METHOD);
    bound_method->receiving_instance = receiving_instance;
    bound_method->method = method;
    return bound_method;
}

obj_class_t *obj_class_t_allocate(obj_string_t *name)
{
    obj_class_t *cls = ALLOCATE_OBJ(obj_class_t, OBJ_CLASS);
    cls->name = name;
    table_t_init(&cls->methods);
    return cls;
}

obj_instance_t *obj_instance_t_allocate(obj_class_t *cls)
{
    obj_instance_t *instance = ALLOCATE_OBJ(obj_instance_t, OBJ_INSTANCE);
    instance->cls = cls;
    table_t_init(&instance->fields);
    return instance;
}

obj_closure_t *obj_closure_t_allocate(obj_function_t *function)
{
    obj_upvalue_t **upvalues = ALLOCATE(obj_upvalue_t*, function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++) {
        upvalues[i] = NULL;
    }
    obj_closure_t *closure = ALLOCATE_OBJ(obj_closure_t, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;
    return closure;
}

obj_function_t *obj_function_t_allocate(void)
{
    obj_function_t *function = ALLOCATE_OBJ(obj_function_t, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    chunk_t_init(&function->chunk);
    return function;
}

obj_native_t *obj_native_t_allocate(native_fn_t function)
{
    obj_native_t *native = ALLOCATE_OBJ(obj_native_t, OBJ_NATIVE);
    native->function = function;
    return native;
}

static obj_string_t *allocate_string(char *chars, const int length, const uint32_t hash) {
    obj_string_t *string = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    vm_push(OBJ_VAL(string));
    table_t_set(&vm.strings, string, NIL_VAL);
    vm_pop();

    return string;
}

static uint32_t hash_string(const char *key, const int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

obj_string_t *obj_string_t_copy_own(char *chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = table_t_find_key_by_str(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

obj_string_t *obj_string_t_copy_from(const char *chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = table_t_find_key_by_str(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    char *str = ALLOCATE(char, length + 1);
    memcpy(str, chars, length);
    str[length] = '\0';
    return allocate_string(str, length, hash);
}

obj_upvalue_t *obj_upvalue_t_allocate(value_t *slot)
{
    obj_upvalue_t *upvalue = ALLOCATE_OBJ(obj_upvalue_t, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    return upvalue;
}

static void print_function(const obj_function_t *function)
{
    if (function->name == NULL) {
        printf("<script>"); // or main ?
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

void obj_t_print(const value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: print_function(AS_BOUND_METHOD(value)->method->function); break;
        case OBJ_CLASS: printf("<cls %s>", AS_CLASS(value)->name->chars); break;
        case OBJ_CLOSURE: print_function(AS_CLOSURE(value)->function); break;
        case OBJ_FUNCTION: print_function(AS_FUNCTION(value)); break;
        case OBJ_INSTANCE: printf("<cls %s instance %p>", AS_INSTANCE(value)->cls->name->chars, (void*)AS_OBJ(value)); break;
        case OBJ_NATIVE: printf("<native fn>"); break;
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_UPVALUE: printf("<upvalue>"); break;
        default: {
            DEBUG_LOGGER("Unhandled default for object type %d (%p)\n", OBJ_TYPE(value), (void *)&value);
            exit(EXIT_FAILURE);
        }
    }
}
