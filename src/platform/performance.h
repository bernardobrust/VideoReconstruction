#pragma once

#include "macros.h"

// SIMD aliases and branch hints vary by compiler and target architecture

// Normalize the compiler-specific SSE2 intrinsics
#ifdef simd128i
#define PERFORMANCE_HAS_SSE2 1

#define SIMD128_SET1_I32 _mm_set1_epi32
#define SIMD128_SET1_I16 _mm_set1_epi16
#define SIMD128_SETR_I32 _mm_setr_epi32
#define SIMD128_ZERO _mm_setzero_si128
#define SIMD128_LOADU _mm_loadu_si128
#define SIMD128_STOREU _mm_storeu_si128
#define SIMD128_AND _mm_and_si128
#define SIMD128_ANDNOT _mm_andnot_si128
#define SIMD128_OR _mm_or_si128
#define SIMD128_UNPACKLO_U8 _mm_unpacklo_epi8
#define SIMD128_UNPACKHI_U8 _mm_unpackhi_epi8
#define SIMD128_ADD_I16 _mm_add_epi16
#define SIMD128_MULLO_I16 _mm_mullo_epi16
#define SIMD128_SHIFT_RIGHT_U16 _mm_srli_epi16
#define SIMD128_PACKUS_I16 _mm_packus_epi16
#define SIMD128_COMPARE_GT_I32 _mm_cmpgt_epi32
#endif

#ifdef simd256i
#define PERFORMANCE_HAS_AVX2 1

#define SIMD256_SET1_I32 _mm256_set1_epi32
#define SIMD256_SET1_I16 _mm256_set1_epi16
#define SIMD256_SETR_I32 _mm256_setr_epi32
#define SIMD256_ZERO _mm256_setzero_si256
#define SIMD256_LOADU _mm256_loadu_si256
#define SIMD256_STOREU _mm256_storeu_si256
#define SIMD256_AND _mm256_and_si256
#define SIMD256_ANDNOT _mm256_andnot_si256
#define SIMD256_OR _mm256_or_si256
#define SIMD256_UNPACKLO_U8 _mm256_unpacklo_epi8
#define SIMD256_UNPACKHI_U8 _mm256_unpackhi_epi8
#define SIMD256_ADD_I16 _mm256_add_epi16
#define SIMD256_MULLO_I16 _mm256_mullo_epi16
#define SIMD256_SHIFT_RIGHT_U16 _mm256_srli_epi16
#define SIMD256_PACKUS_I16 _mm256_packus_epi16
#define SIMD256_COMPARE_GT_I32 _mm256_cmpgt_epi32
#endif

// MSVC does not have __builtin_expect, so we define likely and unlikely as
// no-ops for MSVC. For other compilers, we use __builtin_expect to provide
// branch prediction hints.
#ifdef _MSC_VER
#define likely(x) (x)
#define unlikely(x) (x)

#else
#define likely(x) __builtin_expect (!!(x), 1)
#define unlikely(x) __builtin_expect (!!(x), 0)
#endif
