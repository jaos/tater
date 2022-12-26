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

obj_closure_t *new_obj_closure_t(obj_function_t *function)
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

obj_function_t *new_obj_function_t(void)
{
    obj_function_t *function = ALLOCATE_OBJ(obj_function_t, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    init_chunk(&function->chunk);
    return function;
}

obj_native_t *new_obj_native_t(native_fn_t function)
{
    obj_native_t *native = ALLOCATE_OBJ(obj_native_t, OBJ_NATIVE);
    native->function = function;
    return native;
}

/* FAM
static obj_string_t *make_string(const int length)
{
    obj_string_t *str = (obj_string_t *)allocate_object(sizeof(obj_string_t) + length + 1, OBJ_STRING);
    str->length = length;
    str->hash = 0;
    return str;
}
*/

static obj_string_t *allocate_string(char *chars, const int length, const uint32_t hash) {
    obj_string_t *string = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJ_VAL(string));
    set_table_t(&vm.strings, string, NIL_VAL);
    pop();

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

obj_string_t *take_string(char *chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = find_string_table_t(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

obj_string_t *copy_string(const char *chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = find_string_table_t(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    char *str = ALLOCATE(char, length + 1);
    memcpy(str, chars, length);
    str[length] = '\0';
    return allocate_string(str, length, hash);

    /* FAM
    obj_string_t *str = make_string(length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;
    push(OBJ_VAL(str)); // make GC happy
    set_table_t(&vm.strings, str, NIL_VAL);
    pop(); // make GC happy
    return str;
    */
}

/* FAM
obj_string_t *concatenate_string(const obj_string_t *a, const obj_string_t *b)
{
    const int length = a->length + b->length;
    obj_string_t *str = make_string(length);
    push(OBJ_VAL(str)); // make GC happy

    memcpy(str->chars, a->chars, a->length);
    memcpy(str->chars + a->length, b->chars, b->length);
    str->chars[length] = '\0';
    str->hash = hash_string(str->chars, length);

    obj_string_t *interned = find_string_table_t(&vm.strings, str->chars, length, str->hash);
    if (interned != NULL) {
        pop();
        FREE(obj_string_t, str);
        return interned;
    }

    set_table_t(&vm.strings, str, NIL_VAL);
    pop();
    return str;
}
*/

obj_upvalue_t *new_obj_upvalue_t(value_t *slot)
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
        printf("<script>");
    } else {
        printf("<fn %s>", function->name->chars);
    }
}

void print_object(const value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_CLOSURE: print_function(AS_CLOSURE(value)->function); break;
        case OBJ_FUNCTION: print_function(AS_FUNCTION(value)); break;
        case OBJ_NATIVE: printf("<native fn>"); break;
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_UPVALUE: printf("<upvalue>"); break;
        default: {
            DEBUG_LOGGER("Unhandled default for object type %d (%p)\n", OBJ_TYPE(value), (void *)&value);
            //exit(EXIT_FAILURE);
        }
    }
}
