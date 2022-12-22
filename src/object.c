#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

static obj_t *allocate_object(size_t size, obj_type_t type) {
    obj_t *object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects; // add to our vm's linked list of objects so we always have a reference to it
    vm.objects = object;
    return object;
}

static obj_string_t *make_string(int length)
{
    obj_string_t *str = (obj_string_t *)allocate_object(sizeof(obj_string_t) + length + 1, OBJ_STRING);
    str->length = length;
    str->hash = 0;
    return str;
}

static uint32_t hash_string(const char *key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

obj_string_t *copy_string(const char *chars, const int length)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = find_string_table_t(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    obj_string_t *str = make_string(length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    str->hash = hash;
    set_table_t(&vm.strings, OBJ_VAL(str), NIL_VAL);
    return str;
}

obj_string_t *concatenate_string(const obj_string_t *a, const obj_string_t *b)
{
    const int length = a->length + b->length;
    obj_string_t *str = make_string(length);

    memcpy(str->chars, a->chars, a->length);
    memcpy(str->chars + a->length, b->chars, b->length);
    str->chars[length] = '\0';
    str->hash = hash_string(str->chars, length);

    obj_string_t *interned = find_string_table_t(&vm.strings, str->chars, length, str->hash);
    if (interned != NULL) {
        FREE(obj_string_t, str);
        return interned;
    }

    set_table_t(&vm.strings, OBJ_VAL(str), NIL_VAL);
    return str;
}

void print_object(const value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        default: DEBUG_LOGGER("Unhandled default\n",); exit(EXIT_FAILURE);
    }
}
