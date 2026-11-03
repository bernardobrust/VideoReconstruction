#include <assert.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef PF_AVX2_INSTRUCTIONS_AVAILABLE
#define PF_AVX2_INSTRUCTIONS_AVAILABLE 40
#endif
#else
#include <pthread.h>
#endif

#include "performance.h"
#include "renderer_internal.h"

void
renderer_blend_row_sse2 (u32 *pixels, s32 count, u32 color)
{
  s32 x;
  u8 alpha, inverse_alpha;
  simd128i source, alpha_mask, source_alpha, alpha_v, inverse_alpha_v, one;

  assert (pixels != NULL && count >= 0);

  alpha = (u8)(color >> 24);
  if (alpha == 0)
    return;
  if (alpha == 255)
    {
      for (x = 0; x < count; ++x)
        pixels[x] = color;

      return;
    }

  inverse_alpha = 255 - alpha;
  source = SIMD128_SET1_I32 ((s32)color);
  alpha_mask = SIMD128_SET1_I32 ((s32)0xFF000000);
  source_alpha = SIMD128_AND (source, alpha_mask);
  alpha_v = SIMD128_SET1_I16 (alpha);
  inverse_alpha_v = SIMD128_SET1_I16 (inverse_alpha);
  one = SIMD128_SET1_I16 (1);

  for (x = 0; x + 4 <= count; x += 4)
    {
      simd128i destination, zero, dst_lo, dst_hi, src_lo, src_hi, lo, hi;

      destination = SIMD128_LOADU ((simd128i *)(pixels + x));
      zero = SIMD128_ZERO ();
      dst_lo = SIMD128_UNPACKLO_U8 (destination, zero);
      dst_hi = SIMD128_UNPACKHI_U8 (destination, zero);
      src_lo = SIMD128_UNPACKLO_U8 (source, zero);
      src_hi = SIMD128_UNPACKHI_U8 (source, zero);
      lo = SIMD128_ADD_I16 (SIMD128_MULLO_I16 (src_lo, alpha_v),
                            SIMD128_MULLO_I16 (dst_lo, inverse_alpha_v));
      hi = SIMD128_ADD_I16 (SIMD128_MULLO_I16 (src_hi, alpha_v),
                            SIMD128_MULLO_I16 (dst_hi, inverse_alpha_v));
      lo = SIMD128_SHIFT_RIGHT_U16 (
          SIMD128_ADD_I16 (SIMD128_ADD_I16 (lo, one),
                           SIMD128_SHIFT_RIGHT_U16 (lo, 8)),
          8);
      hi = SIMD128_SHIFT_RIGHT_U16 (
          SIMD128_ADD_I16 (SIMD128_ADD_I16 (hi, one),
                           SIMD128_SHIFT_RIGHT_U16 (hi, 8)),
          8);
      destination = SIMD128_PACKUS_I16 (lo, hi);
      destination = SIMD128_OR (SIMD128_ANDNOT (alpha_mask, destination),
                                source_alpha);
      SIMD128_STOREU ((simd128i *)(pixels + x), destination);
    }

  for (; x < count; ++x)
    pixels[x] = blend_rgba_pixel (color, pixels[x]);
}

void
renderer_triangle_row_sse2 (u32 *pixels, s32 count, u32 color, s32 w0, s32 w1,
                            s32 w2, s32 a0, s32 a1, s32 a2, bool transparent)
{
  s32 x;
  u8 alpha, inverse_alpha;
  simd128i source, alpha_mask, source_alpha, alpha_v, inverse_alpha_v, one,
      negative_one;

  if (transparent && (color >> 24) == 0)
    return;

  alpha = (u8)(color >> 24);
  inverse_alpha = 255 - alpha;
  source = SIMD128_SET1_I32 ((s32)color);
  alpha_mask = SIMD128_SET1_I32 ((s32)0xFF000000);
  source_alpha = SIMD128_AND (source, alpha_mask);
  alpha_v = SIMD128_SET1_I16 (alpha);
  inverse_alpha_v = SIMD128_SET1_I16 (inverse_alpha);
  one = SIMD128_SET1_I16 (1);
  negative_one = SIMD128_SET1_I32 (-1);

  for (x = 0; x + 4 <= count; x += 4)
    {
      simd128i destination, w0v, w1v, w2v, coverage, zero, dst_lo, dst_hi,
          src_lo, src_hi, lo, hi, blended;

      w0v = SIMD128_SETR_I32 (w0, w0 + a0, w0 + 2 * a0, w0 + 3 * a0);
      w1v = SIMD128_SETR_I32 (w1, w1 + a1, w1 + 2 * a1, w1 + 3 * a1);
      w2v = SIMD128_SETR_I32 (w2, w2 + a2, w2 + 2 * a2, w2 + 3 * a2);
      coverage = SIMD128_COMPARE_GT_I32 (
          SIMD128_OR (SIMD128_OR (w0v, w1v), w2v), negative_one);
      destination = SIMD128_LOADU ((simd128i *)(pixels + x));

      if (transparent)
        {
          zero = SIMD128_ZERO ();
          dst_lo = SIMD128_UNPACKLO_U8 (destination, zero);
          dst_hi = SIMD128_UNPACKHI_U8 (destination, zero);
          src_lo = SIMD128_UNPACKLO_U8 (source, zero);
          src_hi = SIMD128_UNPACKHI_U8 (source, zero);
          lo = SIMD128_ADD_I16 (SIMD128_MULLO_I16 (src_lo, alpha_v),
                                SIMD128_MULLO_I16 (dst_lo, inverse_alpha_v));
          hi = SIMD128_ADD_I16 (SIMD128_MULLO_I16 (src_hi, alpha_v),
                                SIMD128_MULLO_I16 (dst_hi, inverse_alpha_v));
          lo = SIMD128_SHIFT_RIGHT_U16 (
              SIMD128_ADD_I16 (SIMD128_ADD_I16 (lo, one),
                               SIMD128_SHIFT_RIGHT_U16 (lo, 8)),
              8);
          hi = SIMD128_SHIFT_RIGHT_U16 (
              SIMD128_ADD_I16 (SIMD128_ADD_I16 (hi, one),
                               SIMD128_SHIFT_RIGHT_U16 (hi, 8)),
              8);
          blended = SIMD128_PACKUS_I16 (lo, hi);
          blended = SIMD128_OR (SIMD128_ANDNOT (alpha_mask, blended),
                                source_alpha);
          destination = SIMD128_OR (SIMD128_AND (coverage, blended),
                                    SIMD128_ANDNOT (coverage, destination));
        }
      else
        destination = SIMD128_OR (SIMD128_AND (coverage, source),
                                  SIMD128_ANDNOT (coverage, destination));

      SIMD128_STOREU ((simd128i *)(pixels + x), destination);
      w0 += 4 * a0;
      w1 += 4 * a1;
      w2 += 4 * a2;
    }

  for (; x < count; ++x)
    {
      if ((w0 | w1 | w2) >= 0)
        pixels[x] = transparent ? blend_rgba_pixel (color, pixels[x]) : color;

      w0 += a0;
      w1 += a1;
      w2 += a2;
    }
}

void
renderer_blend_rectangle_sse2 (u32 *pixels, s32 stride, s32 width, s32 height,
                               u32 color)
{
  s32 y;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  for (y = 0; y < height; ++y)
    renderer_blend_row_sse2 (pixels + y * stride, width, color);
}

void
renderer_fill_rectangle_sse2 (u32 *pixels, s32 stride, s32 width, s32 height,
                              u32 color)
{
  s32 x, y;
  simd128i color_v;
  u32 *row;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  color_v = SIMD128_SET1_I32 ((s32)color);
  for (y = 0; y < height; ++y)
    {
      row = pixels + y * stride;
      for (x = 0; x + 4 <= width; x += 4)
        SIMD128_STOREU ((simd128i *)(row + x), color_v);

      for (; x < width; ++x)
        row[x] = color;
    }
}

typedef void (*RendererBlendRectangleFn) (u32 *pixels, s32 stride, s32 width,
                                          s32 height, u32 color);
typedef void (*RendererFillRectangleFn) (u32 *pixels, s32 stride, s32 width,
                                         s32 height, u32 color);

local RendererBlendRectangleFn renderer_blend_rectangle_impl;
local RendererFillRectangleFn renderer_fill_rectangle_impl;
local RendererTriangleRowFn renderer_triangle_row_impl;

#if defined(_WIN32)
local INIT_ONCE renderer_blend_rectangle_once = INIT_ONCE_STATIC_INIT;

local BOOL CALLBACK
renderer_blend_rectangle_init (PINIT_ONCE once, PVOID parameter,
                               PVOID *context)
{
  (void)once;
  (void)parameter;
  (void)context;

  renderer_blend_rectangle_impl
      = IsProcessorFeaturePresent (PF_AVX2_INSTRUCTIONS_AVAILABLE)
            ? renderer_blend_rectangle_avx2
            : renderer_blend_rectangle_sse2;
  renderer_fill_rectangle_impl
      = IsProcessorFeaturePresent (PF_AVX2_INSTRUCTIONS_AVAILABLE)
            ? renderer_fill_rectangle_avx2
            : renderer_fill_rectangle_sse2;
  renderer_triangle_row_impl
      = IsProcessorFeaturePresent (PF_AVX2_INSTRUCTIONS_AVAILABLE)
            ? renderer_triangle_row_avx2
            : renderer_triangle_row_sse2;
  return TRUE;
}
#else
local pthread_once_t renderer_blend_rectangle_once = PTHREAD_ONCE_INIT;

local void
renderer_blend_rectangle_init (void)
{
  __builtin_cpu_init ();
  renderer_blend_rectangle_impl = __builtin_cpu_supports ("avx2")
                                      ? renderer_blend_rectangle_avx2
                                      : renderer_blend_rectangle_sse2;
  renderer_fill_rectangle_impl = __builtin_cpu_supports ("avx2")
                                     ? renderer_fill_rectangle_avx2
                                     : renderer_fill_rectangle_sse2;
  renderer_triangle_row_impl = __builtin_cpu_supports ("avx2")
                                   ? renderer_triangle_row_avx2
                                   : renderer_triangle_row_sse2;
}
#endif

void
renderer_blend_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                          u32 color)
{
  bool initialized;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

#if defined(_WIN32)
  initialized
      = InitOnceExecuteOnce (&renderer_blend_rectangle_once,
                             renderer_blend_rectangle_init, NULL, NULL);
#else
  initialized = pthread_once (&renderer_blend_rectangle_once,
                              renderer_blend_rectangle_init)
                == 0;
#endif

  assert (initialized);
  if (!initialized)
    {
      renderer_blend_rectangle_sse2 (pixels, stride, width, height, color);
      return;
    }

  if (width < 8
      && renderer_blend_rectangle_impl == renderer_blend_rectangle_avx2)
    renderer_blend_rectangle_sse2 (pixels, stride, width, height, color);
  else
    renderer_blend_rectangle_impl (pixels, stride, width, height, color);
}

void
renderer_fill_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                         u32 color)
{
  bool initialized;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

#if defined(_WIN32)
  initialized
      = InitOnceExecuteOnce (&renderer_blend_rectangle_once,
                             renderer_blend_rectangle_init, NULL, NULL);
#else
  initialized = pthread_once (&renderer_blend_rectangle_once,
                              renderer_blend_rectangle_init)
                == 0;
#endif

  assert (initialized);
  if (!initialized)
    {
      renderer_fill_rectangle_sse2 (pixels, stride, width, height, color);
      return;
    }

  if (width < 8
      && renderer_fill_rectangle_impl == renderer_fill_rectangle_avx2)
    renderer_fill_rectangle_sse2 (pixels, stride, width, height, color);
  else
    renderer_fill_rectangle_impl (pixels, stride, width, height, color);
}

RendererTriangleRowFn
renderer_get_triangle_row_renderer (s32 count)
{
  bool initialized;

#if defined(_WIN32)
  initialized
      = InitOnceExecuteOnce (&renderer_blend_rectangle_once,
                             renderer_blend_rectangle_init, NULL, NULL);
#else
  initialized = pthread_once (&renderer_blend_rectangle_once,
                              renderer_blend_rectangle_init)
                == 0;
#endif

  assert (initialized);
  if (!initialized || count < 8
      || renderer_triangle_row_impl == renderer_triangle_row_sse2)
    return renderer_triangle_row_sse2;

  return renderer_triangle_row_impl;
}
