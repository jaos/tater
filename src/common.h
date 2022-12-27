#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef __unused__
    #define __unused__ __attribute__((unused))
#endif

#ifndef DEBUG
#define DEBUG_LOGGER(fmt, ...)  ((void)0)
#else
#define DEBUG_LOGGER(fmt, ...) do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__); } while (0) // Flawfinder: disable

// #define DEBUG_STRESS_GC
// #define DEBUG_LOG_GC
#define DEBUG_TRACE_EXECUTION

#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#define CPP_STRINGIFY_NX(s) #s
#define CPP_STRINGIFY(s) CPP_STRINGIFY_NX(s)

// #define DEBUG_TRACE_EXECUTION

#endif
