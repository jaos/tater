#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

// TODO consider a more robust way to avoid stack overflows
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    obj_closure_t *closure;
    uint8_t *ip;
    value_t *slots;
} call_frame_t;

typedef struct {
    call_frame_t frames[FRAMES_MAX];
    int frame_count;
    value_t stack[STACK_MAX];
    value_t *stack_top;
    table_t globals;
    table_t strings;
    obj_string_t *init_string;
    obj_upvalue_t *open_upvalues;
    size_t bytes_allocated;
    size_t next_garbage_collect;
    obj_t *objects;
    int gray_count;
    int gray_capacity;
    obj_t **gray_stack;
    int exit_status;
} vm_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_EXIT_OK,
    INTERPRET_EXIT,
} interpret_result_t;

extern vm_t vm;

void init_vm(void);
void free_vm(void);
interpret_result_t interpret(const char *source);
void push(const value_t value);
value_t pop(void);
void define_native(const char *name, const native_fn_t function);

#endif
