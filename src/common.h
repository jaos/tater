#ifndef tater_common_h
#define tater_common_h
/*
 * Copyright (C) 2022-2023 Jason Woodward <woodwardj at jaos dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <libintl.h> // translations

#ifndef __unused__
    #define __unused__ __attribute__((unused))
#endif

#ifndef DEBUG
#define DEBUG_LOGGER(fmt, ...)  ((void)0)
#else
#define DEBUG_LOGGER(fmt, ...) do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__ __VA_OPT__(,) __VA_ARGS__); } while (0) // Flawfinder: disable
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#define CPP_STRINGIFY_NX(s) #s
#define CPP_STRINGIFY(s) CPP_STRINGIFY_NX(s)

// #define DEBUG_TRACE_EXECUTION

#endif
