// The reason this file is in platform/ is because some functions here are platform-specific

#pragma once

#include <stdint.h>

// MSVC does not have __builtin_expect, so we define likely and unlikely as
// no-ops for MSVC. For other compilers, we use __builtin_expect to provide
// branch prediction hints
#ifdef _MSC_VER
#define likely(x) (x)
#define unlikely(x) (x)

#else
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#endif

// Computes floor(x / 255), very usefull in the renderer for alpha blending
#define DIV_255(x) (uint8_t)((x + 1 + ((x) >> 8)) >> 8)