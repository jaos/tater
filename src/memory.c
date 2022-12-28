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

void obj_t_mark(obj_t *obj)
{
    if (obj == NULL)
        return;
    if (obj->is_marked)
        return;

    #ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)obj);
    value_t_print(OBJ_VAL(obj));
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

void value_t_mark(value_t value)
{
    if (IS_OBJ(value))
        obj_t_mark(AS_OBJ(value));
}

static void mark_array(value_array_t *array)
{
    for (int i = 0; i < array->count; i++) {
        value_t_mark(array->values[i]);
    }
}

static void blacken_object(obj_t *object)
{
    #ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    value_t_print(OBJ_VAL(object));
    printf("\n");
    #endif

    switch (object->type) {
        case OBJ_BOUND_METHOD: {
            obj_bound_method_t *bound_method = (obj_bound_method_t*)object;
            value_t_mark(bound_method->receiving_instance); // should be an obj_instance_t that is already marked... but insurance here
            obj_t_mark((obj_t*)bound_method->method);
            break;
        }
        case OBJ_CLASS: {
            obj_class_t *cls = (obj_class_t*)object;
            obj_t_mark((obj_t*)cls->name);
            table_t_mark(&cls->methods);
            break;
        }
        case OBJ_CLOSURE: {
            obj_closure_t *closure = (obj_closure_t*)object;
            obj_t_mark((obj_t*)closure->function);
            for (int i = 0; i < closure->upvalue_count; i++) {
                obj_t_mark((obj_t*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)object;
            obj_t_mark((obj_t*)function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE: {
            obj_instance_t *instance = (obj_instance_t*)object;
            obj_t_mark((obj_t*)instance->cls);
            table_t_mark(&instance->fields);
            break;
        }
        case OBJ_UPVALUE: value_t_mark(((obj_upvalue_t*)object)->closed); break;
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
        case OBJ_BOUND_METHOD: {
            FREE(obj_bound_method_t, o);
            break;
        }
        case OBJ_CLASS: {
            obj_class_t *cls = (obj_class_t*)o;
            table_t_free(&cls->methods);
            FREE(obj_class_t, o); // TODO obj_string_t ?
            break;
        }
        case OBJ_CLOSURE: {
            obj_closure_t *closure = (obj_closure_t*)o;
            // free the containing array, not the actual upvalues themselves
            FREE_ARRAY(obj_upvalue_t*, closure->upvalues, closure->upvalue_count);
            FREE(obj_closure_t, o); // leave the function
            break;
        }
        case OBJ_FUNCTION: {
            obj_function_t *function = (obj_function_t*)o;
            chunk_t_free(&function->chunk);
            FREE(obj_function_t, o);
            break;
        }
        case OBJ_INSTANCE: {
            obj_instance_t *instance = (obj_instance_t*)o;
            table_t_free(&instance->fields);
            FREE(obj_instance_t, o);
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
        value_t_mark(*slot);
    }

    for (int i =0; i < vm.frame_count; i++) {
        obj_t_mark((obj_t*)vm.frames[i].closure);
    }

    for (obj_upvalue_t *upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        obj_t_mark((obj_t*)upvalue);
    }

    table_t_mark(&vm.globals);
    compiler_t_mark_roots();
    obj_t_mark((obj_t*)vm.init_string);
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
    table_t_remove_white(&vm.strings);
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
