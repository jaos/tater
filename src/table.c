#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

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
