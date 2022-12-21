#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
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

static obj_string_t *allocate_string(char *chars, const int length)
{
    obj_string_t *str = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    str->length = length;
    str->chars = chars;
    return str;
}

obj_string_t *take_string(char *chars, int length)
{
    return allocate_string(chars, length);
}

obj_string_t *copy_string(const char *chars, const int length)
{
    char *c = ALLOCATE(char, length + 1);
    memcpy(c, chars, length);
    c[length] = '\0';
    return allocate_string(c, length);
}

void print_object(value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}
