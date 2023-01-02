#ifndef tater_vm_h
#define tater_vm_h

#include "type.h"
#include "vmopcodes.h"

// TODO consider a more robust way to avoid stack overflows
#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    obj_closure_t *closure;
    uint8_t *ip;
    value_t *slots;
} call_frame_t;

typedef enum vm_flag {
    VM_FLAG_NONE = 0x0,
    VM_FLAG_STACK_TRACE = 0x1,
    VM_FLAG_GC_TRACE = 0x2,
    VM_FLAG_GC_STRESS = 0x4,
    VM_FLAG_GC_ACTIVE = 0x8,
} vm_flag_t;

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
    uint64_t flags;
    int exit_status;
} vm_t;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
    INTERPRET_EXIT_OK,
    INTERPRET_EXIT,
} vm_t_interpret_result_t;

extern vm_t vm;

void vm_t_init(void);
void vm_t_free(void);
vm_t_interpret_result_t vm_t_interpret(const char *source);
void vm_push(const value_t value);
value_t vm_pop(void);

void vm_define_native(const char *name, const native_fn_t function, const int arity);

void vm_set_argc_argv(const int argc, const char *argv[]);
void vm_inherit_env(void);

void vm_toggle_gc_stress(void);
void vm_toggle_gc_trace(void);
void vm_toggle_stack_trace(void);
void vm_collect_garbage(void);

static inline bool vm_gc_active(void)
{
    return vm.flags & VM_FLAG_GC_ACTIVE;
}

static inline void vm_gc_toggle_active(void)
{
    vm.flags ^= VM_FLAG_GC_ACTIVE;
}

#endif
