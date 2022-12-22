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

void init_table_t(table_t *table);
void free_table_t(table_t *table);
bool set_table_t(table_t *table, obj_string_t *key, value_t value);
bool get_table_t(table_t *table, const obj_string_t *key, value_t *value);
bool delete_table_t(table_t *table, const obj_string_t *key);
obj_string_t *find_string_table_t(const table_t *table, const char *chars, const int length, const uint32_t hash);
void add_all_table_t(const table_t *from, table_t *to);

#endif
