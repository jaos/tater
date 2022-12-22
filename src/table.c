#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void init_table_t(table_t *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table_t(table_t *table)
{
    FREE_ARRAY(entry_t, table->entries, table->capacity);
    init_table_t(table);
}

static entry_t *find_entry(entry_t *entries, const int capacity, const value_t key)
{
    uint32_t index = hash_value(key) % capacity;
    entry_t *tombstone = NULL;

    for (;;) {
        entry_t *entry = &entries[index];
        if (IS_EMPTY(entry->key)) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else {
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (values_equal(key, entry->key)) {
            return entry;
        }
        index = (index + 1) % capacity;
    }
}

static void adjust_capacity(table_t *table, const int capacity)
{
    entry_t *entries = ALLOCATE(entry_t, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = EMPTY_VAL;
        entries[i].value = NIL_VAL;
    }
    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        entry_t *entry = &table->entries[i];
        if (IS_EMPTY(entry->key)) continue;
        entry_t *dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(entry_t, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool set_table_t(table_t *table, const value_t key, value_t value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    entry_t *entry = find_entry(table->entries, table->capacity, key);
    const bool is_new_key = IS_EMPTY(entry->key);
    if (is_new_key && IS_NIL(entry->value)) table->count++; // we only increment if not a tombstone

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool get_table_t(table_t *table, const value_t key, value_t *value)
{
    if (table->count == 0) return false;
    entry_t *entry = find_entry(table->entries, table->capacity, key);
    if (IS_NIL(entry->key)) return false;
    *value = entry->value;
    return true;
}

bool delete_table_t(table_t *table, const value_t key)
{
    if (table->count == 0) return false;
    entry_t *entry = find_entry(table->entries, table->capacity, key);
    if (IS_EMPTY(entry->key)) return false;
    // place a tombstone entry
    entry->key = EMPTY_VAL;
    entry->value = BOOL_VAL(true);
    return true;
}

void add_all_table_t(const table_t *from, table_t *to)
{
    for (int i = 0; i < from->capacity; i++) {
        entry_t *entry = &from->entries[i];
        if (IS_EMPTY(entry->key)) {
            set_table_t(to, entry->key, entry->value);
        }
    }
}

obj_string_t *find_string_table_t(const table_t *table, const char *chars, const int length, const uint32_t hash)
{
    if (table->count == 0) return NULL;
    uint32_t index = hash % table->capacity;
    for (;;) {
        entry_t *entry = &table->entries[index];

        if (IS_EMPTY(entry->key)) return NULL;
        obj_string_t *string = AS_STRING(entry->key);
        if (string->length == length && memcmp(string->chars, chars, length) == 0) {
            // we found it
            return string;
        }

        index = (index + 1) % table->capacity;
    }
}
