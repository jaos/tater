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

obj_string_t *make_string(int length)
{
    obj_string_t *str = (obj_string_t *)allocate_object(sizeof(obj_string_t) + length + 1, OBJ_STRING);
    str->length = length;
    return str;
}

obj_string_t *copy_string(const char *chars, const int length)
{
    obj_string_t *str = make_string(length);
    memcpy(str->chars, chars, length);
    str->chars[length] = '\0';
    return str;
}

void print_object(value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    }
}
