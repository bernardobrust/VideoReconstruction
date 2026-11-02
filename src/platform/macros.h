/*
 * Utility Macros:
 * - Better data types
 * - Better pointer access
 * - Better "static" definitions
 * - Data sizes
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Static is confusing
#define local static
#define persist static
#define global static

#define byte char
#define ubyte unsigned byte

// Fixed data types without weird names
#define s8 int8_t
#define u8 uint8_t
#define s16 int16_t
#define u16 uint16_t
#define s32 int32_t
#define u32 uint32_t
#define s64 int64_t
#define u64 uint64_t
#define f32 float
#define f64 double
#define f128 long double

// Sizes, be sure that the datatype is big enough for gigabyte
#define KILOBYTE(x) (x * 1024)
#define MEGABYTE(x) (KILOBYTE (x) * 1024)
#define GIGABYTE(x) (MEGABYTE (x) * 1024)