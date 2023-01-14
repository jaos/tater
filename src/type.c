/*
 * Copyright (C) 2022-2023 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "memory.h"
#include "type.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

static obj_t *allocate_object(const size_t size, const obj_type_t type)
{
    assert(!vm_gc_active()); // attempt to catch us allocating during garbage collection
    obj_t *object = (obj_t*)reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;
    object->next = vm.objects; // add to our vm's linked list of objects so we always have a reference to it
    vm.objects = object;
    if (vm.flags & VM_FLAG_GC_TRACE) {
        printf("%p allocate %zu for %s\n", (void*)object, size, obj_type_names[type]);
    }
    return object;
}

obj_bound_method_t *obj_bound_method_t_allocate(value_t receiving_instance, obj_closure_t *method)
{
    obj_bound_method_t *bound_method = ALLOCATE_OBJ(obj_bound_method_t, OBJ_BOUND_METHOD);
    bound_method->receiving_instance = receiving_instance;
    bound_method->method = method;
    return bound_method;
}

obj_bound_native_method_t * obj_bound_native_method_t_allocate(value_t receiving_instance, obj_string_t *name, native_method_fn_t function)
{
    obj_bound_native_method_t *bound_native_method = ALLOCATE_OBJ(obj_bound_native_method_t, OBJ_BOUND_NATIVE_METHOD);
    bound_native_method->receiving_instance = receiving_instance;
    bound_native_method->name = name;
    bound_native_method->function = function;
    return bound_native_method;
}

obj_typeobj_t *obj_typeobj_t_allocate(obj_string_t *name)
{
    obj_typeobj_t *typeobj = ALLOCATE_OBJ(obj_typeobj_t, OBJ_TYPECLASS);
    typeobj->name = name;
    typeobj->super = NULL;
    table_t_init(&typeobj->fields);
    table_t_init(&typeobj->methods);
    return typeobj;
}

obj_instance_t *obj_instance_t_allocate(obj_typeobj_t *typeobj)
{
    obj_instance_t *instance = ALLOCATE_OBJ(obj_instance_t, OBJ_INSTANCE);
    instance->typeobj = typeobj;
    table_t_init(&instance->fields);
    return instance;
}

obj_list_t *obj_list_t_allocate(void)
{
    obj_list_t *list = ALLOCATE_OBJ(obj_list_t, OBJ_LIST);
    value_list_t_init(&list->elements);
    return list;
}

obj_map_t *obj_map_t_allocate(void)
{
    obj_map_t *map = ALLOCATE_OBJ(obj_map_t, OBJ_MAP);
    table_t_init(&map->table);
    return map;
}

obj_file_t *obj_file_t_allocate(obj_string_t *path, obj_string_t *mode)
{
    obj_file_t *file = ALLOCATE_OBJ(obj_file_t, OBJ_FILE);

    int flags = 0;
    if (index(mode->chars, 'a') != NULL)
        flags |= O_APPEND;
    if (index(mode->chars, 'w') != NULL) {
        flags |= O_CREAT;
        if (index(mode->chars, 'a') == NULL && index(mode->chars, 'r') == NULL)
            flags |= O_TRUNC;
    }
    if (index(mode->chars, 'r') != NULL && index(mode->chars, 'w') == NULL)
        flags |= O_RDONLY;
    else if (index(mode->chars, 'r') != NULL && index(mode->chars, 'w') != NULL)
        flags |= O_RDWR;
    else if (index(mode->chars, 'r') == NULL && index(mode->chars, 'w') != NULL)
        flags |= O_WRONLY;

    int fd = open(path->chars, flags, S_IRWXU | S_IRGRP | S_IWGRP | S_IROTH);
    if (fd == -1) {
        perror(path->chars);
        exit(EXIT_FAILURE);
    }

    file->fd = fd;
    file->path = path;
    file->mode = mode;

    return file;
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

obj_native_t *obj_native_t_allocate(native_fn_t function, const obj_string_t *name, const int arity)
{
    obj_native_t *native = ALLOCATE_OBJ(obj_native_t, OBJ_NATIVE);
    native->name = name;
    native->arity = arity;
    native->function = function;
    return native;
}

static obj_string_t *allocate_string(char *chars, const int length, const uint32_t hash, const bool intern) {
    obj_string_t *string = ALLOCATE_OBJ(obj_string_t, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    if (intern) {
        vm_push(OBJ_VAL(string));
        table_t_set(&vm.strings, OBJ_VAL(string), NIL_VAL);
        vm_pop();
    }

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

obj_string_t *obj_string_t_copy_own(char *chars, const int length, const bool intern)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = table_t_find_key_by_str(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash, intern);
}

obj_string_t *obj_string_t_copy_from(const char *chars, const int length, const bool intern)
{
    const uint32_t hash = hash_string(chars, length);
    obj_string_t *interned = table_t_find_key_by_str(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        return interned;
    }

    char *str = ALLOCATE(char, length + 1);
    memcpy(str, chars, length);
    str[length] = '\0';
    return allocate_string(str, length, hash, intern);
}

obj_upvalue_t *obj_upvalue_t_allocate(value_t *slot)
{
    obj_upvalue_t *upvalue = ALLOCATE_OBJ(obj_upvalue_t, OBJ_UPVALUE);
    upvalue->location = slot;
    upvalue->next = NULL;
    upvalue->closed = NIL_VAL;
    return upvalue;
}

static void obj_function_t_print(FILE *stream, const obj_function_t *function)
{
    if (function->name == NULL) {
        fprintf(stream, "<main>");
    } else {
        fprintf(stream, "<fn %s(%d)>", function->name->chars, function->arity);
    }
}

obj_string_t *obj_t_to_obj_string_t(const value_t value)
{
    char buffer[255];
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: {
            obj_function_t *function = AS_BOUND_METHOD(value)->method->function;
            if (function->name == NULL) {
                snprintf(buffer, 255, "<main>");
            } else {
                snprintf(buffer, 255, "<boundmethod %s>", function->name->chars);
            }
            break;
        }
        case OBJ_BOUND_NATIVE_METHOD: {
            snprintf(buffer, 255, "<nativemethod %s>", AS_BOUND_NATIVE_METHOD(value)->name->chars);
            break;
        }
        case OBJ_CLOSURE: {
            obj_function_t *function = AS_CLOSURE(value)->function;
            if (function->name == NULL) {
                snprintf(buffer, 255, "<main>");
            } else {
                snprintf(buffer, 255, "<closure %s>", function->name->chars);
            }
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = AS_FUNCTION(value);
            if (function->name == NULL) {
                snprintf(buffer, 255, "<main>");
            } else {
                snprintf(buffer, 255, "<fn %s>", function->name->chars);
            }
            break;
        }
        case OBJ_TYPECLASS: {
            snprintf(buffer, 255, "<type %s>", AS_TYPECLASS(value)->name->chars);
            break;
        }
        case OBJ_INSTANCE: {
            snprintf(buffer, 255, "<type %s instance %p>", AS_INSTANCE(value)->typeobj->name->chars, (void*)AS_OBJ(value));
            break;
        }
        case OBJ_NATIVE: {
            snprintf(buffer, 255, "<native fn %s>", AS_NATIVE(value)->name->chars);
            break;
        }
        case OBJ_STRING: {
            snprintf(buffer, 255, "%s", AS_CSTRING(value));
            break;
        }
        case OBJ_UPVALUE: {
            snprintf(buffer, 255, "<upvalue>");
            break;
        }
        case OBJ_LIST: {
            obj_list_t *list = AS_LIST(value);
            snprintf(buffer, 255, "<list %d>", list->elements.count);
            break;
        }
        case OBJ_MAP: {
            obj_map_t *map = AS_MAP(value);
            snprintf(buffer, 255, "<map %d>", map->table.count);
            break;
        }
        case OBJ_FILE: {
            obj_file_t *f = AS_FILE(value);
            if (f->fd == -1) {
                snprintf(buffer, 255, "<file closed>");
            } else {
                snprintf(buffer, 255, "<file %s(%s)>", f->path->chars, f->mode->chars);
            }
            break;
        }
        default: {
            DEBUG_LOGGER("Unhandled default for object type %d (%p)\n", OBJ_TYPE(value), (void *)&value);
            exit(EXIT_FAILURE);
        }
    }
    return obj_string_t_copy_from(buffer, strlen(buffer), true);
}

void obj_t_print(FILE *stream, const value_t value)
{
    if (stream == NULL) stream = stdout;

    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: obj_function_t_print(stream, AS_BOUND_METHOD(value)->method->function); break;
        case OBJ_BOUND_NATIVE_METHOD: fprintf(stream, "<nativemethod %s>", AS_BOUND_NATIVE_METHOD(value)->name->chars); break;
        case OBJ_TYPECLASS: fprintf(stream, "<type %s>", AS_TYPECLASS(value)->name->chars); break;
        case OBJ_CLOSURE: obj_function_t_print(stream, AS_CLOSURE(value)->function); break;
        case OBJ_FUNCTION: obj_function_t_print(stream, AS_FUNCTION(value)); break;
        case OBJ_INSTANCE: fprintf(stream, "<type %s instance %p>", AS_INSTANCE(value)->typeobj->name->chars, (void*)AS_OBJ(value)); break;
        case OBJ_NATIVE: fprintf(stream, "<native fn %s>", AS_NATIVE(value)->name->chars); break;
        case OBJ_STRING: fprintf(stream, "%s", AS_CSTRING(value)); break;
        case OBJ_UPVALUE: fprintf(stream, "<upvalue>"); break;
        case OBJ_LIST: {
            obj_list_t *list = AS_LIST(value);
            if (list->elements.count > 64) {
                fprintf(stream, "<list %d>", list->elements.count);
            } else {
                fprintf(stream, "[");
                for (int i = 0; i < list->elements.count; i++) {
                    if (i > 0) fprintf(stream, ",");
                    value_t_print(stream, list->elements.values[i]);
                }
                fprintf(stream, "]");
            }
            break;
        }
        case OBJ_MAP: {
            obj_map_t *map = AS_MAP(value);
            if (map->table.count > 24) {
                fprintf(stream, "<map %d>", map->table.count);
            } else {
                bool comma = false;
                fprintf(stream, "{");
                for (int i = 0; i < map->table.capacity; i++) {
                    table_entry_t e = map->table.entries[i];
                    if (IS_EMPTY(e.key)) continue;
                    if (comma)
                        fprintf(stream, ",");
                    else
                        comma = true;
                    value_t_print(stream, e.key);
                    fprintf(stream, ":");
                    value_t_print(stream, e.value);
                }
                fprintf(stream, "}");
            }
            break;
        }
        case OBJ_FILE: {
            obj_file_t *f = AS_FILE(value);
            if (f->fd == -1) {
                fprintf(stream, "<file closed>");
            } else {
                fprintf(stream, "<file %s(%s)>", f->path->chars, f->mode->chars);
            }
            break;
        }
        default: {
            DEBUG_LOGGER("Unhandled default for object type %d (%p)\n", OBJ_TYPE(value), (void *)&value);
            exit(EXIT_FAILURE);
        }
    }
}

bool value_t_equal(const value_t a, const value_t b)
{
    if (a.type != b.type) return false;
    switch (a.type) {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        case VAL_EMPTY: return true;
        default: return false; // unreachable
    }
}

static uint32_t hash_double(const double value)
{
    union bitcast {
        double value;
        uint32_t ints[2];
    };
    union bitcast cast;
    cast.value = (value) + 1.0;
    return cast.ints[0] + cast.ints[1];
}

uint32_t value_t_hash(const value_t value)
{
    switch (value.type) {
        case VAL_BOOL: return AS_BOOL(value) ? 3 : 5; // arbitrary hash values
        case VAL_NIL: return 7; // arbitrary hash value
        case VAL_NUMBER: return hash_double(AS_NUMBER(value));
        case VAL_OBJ: return AS_STRING(value)->hash;
        case VAL_EMPTY: return 0; // arbitrary hash value
        default: return 0; // unreachable
    }
}

void value_list_t_init(value_list_t *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void value_list_t_add(value_list_t *array, const value_t value)
{
    if (array->capacity < array->count + 1) {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(value_t, array->values, old_capacity, array->capacity);
        if (array->values == NULL) {
            fprintf(stderr, gettext("Could not allocate value array storage."));
            exit(EXIT_FAILURE);
        }
    }

    array->values[array->count] = value;
    array->count++;
}

void value_list_t_free(value_list_t *array)
{
    FREE_ARRAY(value_t, array->values, array->capacity);
    value_list_t_init(array);
}

obj_string_t *value_t_to_obj_string_t(const value_t value)
{
    char buffer[255];
    switch (value.type) {
        case VAL_BOOL: snprintf(buffer, 255, AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: snprintf(buffer, 255, "nil"); break;
        case VAL_NUMBER: snprintf(buffer, 255, "%g", AS_NUMBER(value)); break;
        case VAL_OBJ: return obj_t_to_obj_string_t(value);
        case VAL_EMPTY: snprintf(buffer, 255, "<empty>"); break;
        default: DEBUG_LOGGER("Unhandled default\n",); exit(EXIT_FAILURE);
    }
    return obj_string_t_copy_from(buffer, strlen(buffer), true);
}

void value_t_print(FILE *stream, const value_t value)
{
    if (stream == NULL) stream = stdout;
    switch (value.type) {
        case VAL_BOOL: fprintf(stream, AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: fprintf(stream, "nil"); break;
        case VAL_NUMBER: fprintf(stream, "%.16g", AS_NUMBER(value)); break;
        case VAL_OBJ: obj_t_print(stream, value); break;
        case VAL_EMPTY: fprintf(stream, "<empty>"); break;
        default: DEBUG_LOGGER("Unhandled default\n",); exit(EXIT_FAILURE);
    }
}

#define TABLE_MAX_LOAD 0.75

void table_t_init(table_t *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void table_t_free(table_t *table)
{
    FREE_ARRAY(table_entry_t, table->entries, table->capacity);
    table_t_init(table);
}

static table_entry_t *find_table_entry(table_entry_t *entries, const int capacity, const value_t key)
{
    uint32_t index = value_t_hash(key) & (capacity - 1);
    table_entry_t *tombstone = NULL;

    for (;;) {
        table_entry_t *table_entry = &entries[index];
        if (IS_EMPTY(table_entry->key)) {
            if (IS_NIL(table_entry->value)) {
                return tombstone != NULL ? tombstone : table_entry;
            } else {
                if (tombstone == NULL) {
                    tombstone = table_entry;
                }
            }
        } else if (value_t_equal(table_entry->key, key)) {
            return table_entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

bool table_t_get(table_t *table, const value_t key, value_t *value)
{
    if (table->count == 0)
        return false;
    table_entry_t *table_entry = find_table_entry(table->entries, table->capacity, key);
    if (IS_EMPTY(table_entry->key))
        return false;
    *value = table_entry->value;
    return true;
}

static void adjust_capacity(table_t *table, const int capacity)
{
    table_entry_t *entries = ALLOCATE(table_entry_t, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = EMPTY_VAL;
        entries[i].value = NIL_VAL;
    }
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        table_entry_t *table_entry = &table->entries[i];
        if (IS_EMPTY(table_entry->key))
            continue;
        table_entry_t *dest = find_table_entry(entries, capacity, table_entry->key);
        dest->key = table_entry->key;
        dest->value = table_entry->value;
        table->count++;
    }

    FREE_ARRAY(table_entry_t, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool table_t_set(table_t *table, const value_t key, const value_t value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        const int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    table_entry_t *table_entry = find_table_entry(table->entries, table->capacity, key);
    const bool is_new_key = IS_EMPTY(table_entry->key);
    if (is_new_key && IS_NIL(table_entry->value))
        table->count++; // we only increment if not a tombstone

    table_entry->key = key;
    table_entry->value = value;
    return is_new_key;
}

bool table_t_delete(table_t *table, const value_t key)
{
    if (table->count == 0)
        return false;

    table_entry_t *table_entry = find_table_entry(table->entries, table->capacity, key);
    if (IS_EMPTY(table_entry->key))
        return false;

    // place a tombstone table_entry
    table_entry->key = EMPTY_VAL;
    table_entry->value = NIL_VAL;
    return true;
}

void table_t_copy_to(const table_t *from, table_t *to)
{
    for (int i = 0; i < from->capacity; i++) {
        const table_entry_t *table_entry = &from->entries[i];
        if (!IS_EMPTY(table_entry->key)) {
            table_t_set(to, table_entry->key, table_entry->value);
        }
    }
}

obj_string_t *table_t_find_key_by_str(const table_t *table, const char *chars, const int length, const uint32_t hash)
{
    if (table->count == 0)
        return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        table_entry_t *table_entry = &table->entries[index];

        if (IS_EMPTY(table_entry->key)) {
            return NULL;
        }
        obj_string_t *string = AS_STRING(table_entry->key);
        if (string->hash == hash && string->length == length && memcmp(string->chars, chars, length) == 0) {
            return string;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void table_t_remove_unmarked(table_t *table)
{
    for (int i = 0; i < table->capacity; i++) {
        table_entry_t *table_entry = &table->entries[i];
        if (!IS_EMPTY(table_entry->key) && IS_OBJ(table_entry->key) && !AS_OBJ(table_entry->key)->is_marked) {
            table_t_delete(table, table_entry->key);
        }
    }
}

void table_t_mark(table_t *table)
{
    for (int i = 0; i < table->capacity; i++) {
        table_entry_t *table_entry = &table->entries[i];
        value_t_mark(table_entry->key);
        value_t_mark(table_entry->value);
    }
}


void chunk_t_init(chunk_t *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    value_list_t_init(&chunk->constants);
    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;
}

void chunk_t_free(chunk_t *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(line_info_t, chunk->lines, chunk->line_capacity);
    value_list_t_free(&chunk->constants);
    chunk_t_init(chunk);
}

void chunk_t_write(chunk_t *chunk, const uint8_t byte, const int line)
{
    if (chunk->capacity < chunk->count + 1) {
        const int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        if (chunk->code == NULL) {
            fprintf(stderr, gettext("Could not allocate chunk code storage."));
            exit(EXIT_FAILURE);
        }
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    if (chunk->line_count > 0 && chunk->lines[chunk->line_count - 1].line == line) {
        return;
    }

    if (chunk->line_capacity < chunk->line_count + 1) {
        const int old_capacity = chunk->line_capacity;
        chunk->line_capacity = GROW_CAPACITY(old_capacity);
        chunk->lines = GROW_ARRAY(line_info_t, chunk->lines, old_capacity, chunk->line_capacity);
        if (chunk->lines == NULL) {
            fprintf(stderr, gettext("Could not allocate chunk line storage."));
            exit(EXIT_FAILURE);
        }
    }
    line_info_t *line_info = &chunk->lines[chunk->line_count++];
    line_info->offset = chunk->count - 1;
    line_info->line = line;
}

int chunk_t_get_line(const chunk_t *chunk, const int instruction)
{
    int start = 0;
    int end = chunk->line_count - 1;
    for (;;) {
        const int mid = (start + end) / 2;
        line_info_t *line = &chunk->lines[mid];
        if (instruction < line->offset) {
            end = mid - 1;
        } else if (mid == chunk->line_count - 1 || instruction < chunk->lines[mid + 1].offset) {
            return line->line;
        } else {
            start = mid + 1;
        }
    }
}

int chunk_t_add_constant(chunk_t *chunk, const value_t value)
{
    vm_push(value); // make GC happy
    value_list_t_add(&chunk->constants, value);
    vm_pop(); // make GC happy
    return chunk->constants.count - 1;
}

void obj_t_mark(obj_t *obj)
{
    if (obj == NULL)
        return;
    if (obj->is_marked)
        return;

    if (vm.flags & VM_FLAG_GC_TRACE) {
        printf("%p mark ", (void*)obj);
        value_t_print(stdout, OBJ_VAL(obj));
        printf("\n");
    }

    obj->is_marked = true;

    if (vm.gray_capacity < vm.gray_count + 1) {
        vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
        vm.gray_stack = (obj_t **)realloc(vm.gray_stack, sizeof(obj_t*) * vm.gray_capacity);
        if(vm.gray_stack == NULL) {
            fprintf(stderr, "Failed to reallocate GC stack.\n");
            exit(EXIT_FAILURE);
        }
    }

    vm.gray_stack[vm.gray_count++] = obj;
}

void value_t_mark(value_t value)
{
    if (IS_OBJ(value))
        obj_t_mark(AS_OBJ(value));
}

/* Used by OP_CONSTANT_LONG
void write_constant(chunk_t *chunk, const value_t value, const int line)
{
    int index = chunk_t_add_constant(chunk, value);
    if (index < 256) {
        chunk_t_write(chunk, OP_CONSTANT, line);
        chunk_t_write(chunk, (uint8_t)index, line);
    } else {
        chunk_t_write(chunk, OP_CONSTANT_LONG, line);
        chunk_t_write(chunk, (uint8_t)(index & 0xff), line);
        chunk_t_write(chunk, (uint8_t)((index >> 8) & 0xff), line);
        chunk_t_write(chunk, (uint8_t)((index >> 16) & 0xff), line);
    }
}
*/
