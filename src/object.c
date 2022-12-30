#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "memory.h"
#include "object.h"
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
        printf("%p allocate %zu for %s\n", (void*)object, size, obj_type_t_to_str(type));
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

obj_list_t *obj_list_t_allocate(void)
{
    obj_list_t *list = ALLOCATE_OBJ(obj_list_t, OBJ_LIST);
    value_array_t_init(&list->elements);
    return list;
}

obj_map_t *obj_map_t_allocate(void)
{
    obj_map_t *map = ALLOCATE_OBJ(obj_map_t, OBJ_MAP);
    table_t_init(&map->table);
    return map;
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

obj_string_t *obj_t_to_obj_string_t(const value_t value)
{
    char buffer[255];
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: {
            obj_function_t *function = AS_BOUND_METHOD(value)->method->function;
            if (function->name == NULL) {
                snprintf(buffer, 255, "<script>"); // or main ?
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
                snprintf(buffer, 255, "<script>"); // or main ?
            } else {
                snprintf(buffer, 255, "<closure %s>", function->name->chars);
            }
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = AS_FUNCTION(value);
            if (function->name == NULL) {
                snprintf(buffer, 255, "<script>"); // or main ?
            } else {
                snprintf(buffer, 255, "<fn %s>", function->name->chars);
            }
            break;
        }
        case OBJ_CLASS: {
            snprintf(buffer, 255, "<cls %s>", AS_CLASS(value)->name->chars);
            break;
        }
        case OBJ_INSTANCE: {
            snprintf(buffer, 255, "<cls %s instance %p>", AS_INSTANCE(value)->cls->name->chars, (void*)AS_OBJ(value));
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
        default: {
            DEBUG_LOGGER("Unhandled default for object type %d (%p)\n", OBJ_TYPE(value), (void *)&value);
            exit(EXIT_FAILURE);
        }
    }
    return obj_string_t_copy_from(buffer, strlen(buffer));
}

void obj_t_print(const value_t value)
{
    switch (OBJ_TYPE(value)) {
        case OBJ_BOUND_METHOD: print_function(AS_BOUND_METHOD(value)->method->function); break;
        case OBJ_BOUND_NATIVE_METHOD: printf("<nativemethod %s>", AS_BOUND_NATIVE_METHOD(value)->name->chars); break;
        case OBJ_CLASS: printf("<cls %s>", AS_CLASS(value)->name->chars); break;
        case OBJ_CLOSURE: print_function(AS_CLOSURE(value)->function); break;
        case OBJ_FUNCTION: print_function(AS_FUNCTION(value)); break;
        case OBJ_INSTANCE: printf("<cls %s instance %p>", AS_INSTANCE(value)->cls->name->chars, (void*)AS_OBJ(value)); break;
        case OBJ_NATIVE: printf("<native fn %s>", AS_NATIVE(value)->name->chars); break;
        case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
        case OBJ_UPVALUE: printf("<upvalue>"); break;
        case OBJ_LIST: {
            obj_list_t *list = AS_LIST(value);
            printf("<list %d>", list->elements.count);
            break;
        }
        case OBJ_MAP: {
            obj_map_t *map = AS_MAP(value);
            printf("<map %d>", map->table.count);
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

void value_array_t_init(value_array_t *array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void value_array_t_add(value_array_t *array, const value_t value)
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

void value_array_t_free(value_array_t *array)
{
    FREE_ARRAY(value_t, array->values, array->capacity);
    value_array_t_init(array);
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
    return obj_string_t_copy_from(buffer, strlen(buffer));
}

void value_t_print(const value_t value)
{
    switch (value.type) {
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: obj_t_print(value); break;
        case VAL_EMPTY: printf("<empty>"); break;
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
    FREE_ARRAY(entry_t, table->entries, table->capacity);
    table_t_init(table);
}

static entry_t *find_entry(entry_t *entries, const int capacity, const obj_string_t *key)
{
    uint32_t index = key->hash & (capacity - 1);
    entry_t *tombstone = NULL;

    for (;;) {
        entry_t *entry = &entries[index];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) {
                    tombstone = entry;
                }
            }
        } else if (entry->key == key) {
            return entry;
        }
        index = (index + 1) & (capacity - 1);
    }
}

bool table_t_get(table_t *table, const obj_string_t *key, value_t *value)
{
    if (table->count == 0)
        return false;
    entry_t *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;
    // if using an object instead of string keys
    // if (IS_NIL(entry->key))
    //     return false;
    // or IS_EMPTY() if empty values
    *value = entry->value;
    return true;
}

static void adjust_capacity(table_t *table, const int capacity)
{
    entry_t *entries = ALLOCATE(entry_t, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        entry_t *entry = &table->entries[i];
        if (entry->key == NULL)
            continue;
        entry_t *dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(entry_t, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool table_t_set(table_t *table, obj_string_t *key, const value_t value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        const int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    entry_t *entry = find_entry(table->entries, table->capacity, key);
    const bool is_new_key = entry->key == NULL;
    if (is_new_key && IS_NIL(entry->value))
        table->count++; // we only increment if not a tombstone

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool table_t_delete(table_t *table, const obj_string_t *key)
{
    if (table->count == 0)
        return false;

    entry_t *entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    // place a tombstone entry
    entry->key = NULL;
    entry->value = TRUE_VAL;
    return true;
}

void table_t_copy_to(const table_t *from, table_t *to)
{
    for (int i = 0; i < from->capacity; i++) {
        const entry_t *entry = &from->entries[i];
        if (entry->key != NULL) {
            table_t_set(to, entry->key, entry->value);
        }
    }
}

obj_string_t *table_t_find_key_by_str(const table_t *table, const char *chars, const int length, const uint32_t hash)
{
    if (table->count == 0)
        return NULL;

    uint32_t index = hash & (table->capacity - 1);
    for (;;) {
        entry_t *entry = &table->entries[index];

        if (entry->key == NULL) {
            // stop if we find an empty non-tombstone entry
            if (IS_NIL(entry->value))
                return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0) {
            // we found it
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void table_t_remove_white(table_t *table)
{
    for (int i = 0; i < table->capacity; i++) {
        entry_t *entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            table_t_delete(table, entry->key);
        }
    }
}

void table_t_mark(table_t *table)
{
    for (int i = 0; i < table->capacity; i++) {
        entry_t *entry = &table->entries[i];
        obj_t_mark((obj_t*)entry->key);
        value_t_mark(entry->value);
    }
}


void chunk_t_init(chunk_t *chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    value_array_t_init(&chunk->constants);
    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;
}

void chunk_t_free(chunk_t *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(line_info_t, chunk->lines, chunk->line_capacity);
    value_array_t_free(&chunk->constants);
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
    value_array_t_add(&chunk->constants, value);
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
        value_t_print(OBJ_VAL(obj));
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
