#pragma once

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