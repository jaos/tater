#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    obj_string_t *key;
    value_t value;
} entry_t;

typedef struct {
    int count;
    int capacity;
    entry_t *entries;
} table_t;

void table_t_init(table_t *table);
void table_t_free(table_t *table);
bool table_t_set(table_t *table, obj_string_t *key, const value_t value);
bool table_t_get(table_t *table, const obj_string_t *key, value_t *value);
bool table_t_delete(table_t *table, const obj_string_t *key);
obj_string_t *table_t_find_key_by_str(const table_t *table, const char *chars, const int length, const uint32_t hash);
void table_t_remove_white(table_t *table);
void table_t_mark(table_t *table);
void table_t_copy_to(const table_t *from, table_t *to);

#endif
