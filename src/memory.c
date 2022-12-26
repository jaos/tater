#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, const size_t old_size __unused__, const size_t new_size)
{
    vm.bytes_allocated += new_size - old_size;
    if (new_size > old_size) {
    #ifdef DEBUG_STRESS_GC
        collect_garbage();
    #endif
        if (vm.bytes_allocated > vm.next_garbage_collect) {
            collect_garbage();
        }
    }

    if (new_size == 0) {
        free(pointer);
        return NULL;
    }

    void *result = realloc(pointer, new_size);
    if (result == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

void mark_object(obj_t *obj)
{
    if (obj == NULL)
        return;
    if (obj->is_marked)
        return;

    #ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)obj);
    print_value(OBJ_VAL(obj));
    printf("\n");
    #endif

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

void mark_value(value_t value)
{
    if (IS_OBJ(value))
        mark_object(AS_OBJ(value));
}

static void mark_array(value_array_t *array)
{
    for (int i = 0; i < array->count; i++) {
        mark_value(array->values[i]);
    }
}

static void blacken_object(obj_t *object)
{
    #ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    print_value(OBJ_VAL(object));
    printf("\n");
    #endif

    switch (object->type) {
        case OBJ_CLOSURE: {
            obj_closure_t *closure = (obj_closure_t*)object;
            mark_object((obj_t*)closure->function);
            for (int i = 0; i < closure->upvalue_count; i++) {
                mark_object((obj_t*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)object;
            mark_object((obj_t*)function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE: mark_value(((obj_upvalue_t*)object)->closed); break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
        default: return;
    }
}

static void free_object(obj_t *o)
{
    #ifdef DEBUG_LOG_GC
    printf("%p free type %s\n", (void*)o, obj_type_t_to_str(o->type));
    #endif
    switch (o->type) {
        case OBJ_CLOSURE: {
            obj_closure_t *closure = (obj_closure_t*)o;
            // free the containing array, not the actual upvalues themselves
            FREE_ARRAY(obj_upvalue_t*, closure->upvalues, closure->upvalue_count);
            FREE(obj_closure_t, o); // leave the function
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)o;
            free_chunk(&function->chunk);
            FREE(obj_function_t, o);
            break;
        }
        case OBJ_NATIVE: {
            FREE(obj_native_t, o);
            break;
        }
        case OBJ_STRING: {
            obj_string_t *s = (obj_string_t*)o;
            //reallocate(o, sizeof(obj_string_t) + s->length + 1, 0);
            FREE_ARRAY(char, s->chars, s->length + 1);
            FREE(obj_string_t, o);
            break;
        }
        case OBJ_UPVALUE: {
            FREE(obj_upvalue_t, o); // leave location value_t reference
            break;
        }
        default: return; // unreachable
    }
}

static void mark_roots(void)
{
    for (value_t *slot = vm.stack; slot < vm.stack_top; slot++) {
        mark_value(*slot);
    }

    for (int i =0; i < vm.frame_count; i++) {
        mark_object((obj_t*)vm.frames[i].closure);
    }

    for (obj_upvalue_t *upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        mark_object((obj_t*)upvalue);
    }

    mark_table(&vm.globals);
    mark_compiler_roots();
}

static void trace_references(void)
{
    while (vm.gray_count > 0) {
        obj_t *object = vm.gray_stack[--vm.gray_count];
        blacken_object(object);
    }
}

static void sweep(void)
{
    // TODO XXX something bad is happening with this running
    obj_t *previous = NULL;
    obj_t *object = vm.objects;
    while (object != NULL) {

        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;

        } else {
            obj_t *unreached = object;
            object = object->next;

            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }

            free_object(unreached);
        }
    }
}

void collect_garbage(void)
{
    #ifdef DEBUG_LOG_GC
    printf("== start gc\n");
    size_t before = vm.bytes_allocated;
    #endif

    mark_roots();
    trace_references();
    table_remove_white(&vm.strings);
    sweep();

    vm.next_garbage_collect = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

    #ifdef DEBUG_LOG_GC
    printf("==   end gc\n");
    printf("           collected %zu bytes (from %zu to %zu) next at %zu\n",
        before - vm.bytes_allocated,
        before,
        vm.bytes_allocated,
        vm.next_garbage_collect);
    #endif
}

void free_objects(void)
{
    obj_t *o = vm.objects;
    while (o != NULL) {
        obj_t *next = o->next;
        free_object(o);
        o = next;
    }
}
