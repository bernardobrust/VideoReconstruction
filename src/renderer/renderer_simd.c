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
      // Widen byte channels before multiplying to keep intermediate values.
      // within 16 bits, then round the division by 255
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
renderer_triangle_row_sse2 (u32 *pixels, s32 count, u32 color,
                            s32 edge_value_0, s32 edge_value_1,
                            s32 edge_value_2, s32 edge_step_x_0,
                            s32 edge_step_x_1, s32 edge_step_x_2,
                            bool transparent)
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
      simd128i destination, edge_value_0_lanes, edge_value_1_lanes,
          edge_value_2_lanes, coverage, zero, dst_lo, dst_hi, src_lo, src_hi,
          lo, hi, blended;

      edge_value_0_lanes = SIMD128_SETR_I32 (
          edge_value_0, edge_value_0 + edge_step_x_0,
          edge_value_0 + 2 * edge_step_x_0, edge_value_0 + 3 * edge_step_x_0);
      edge_value_1_lanes = SIMD128_SETR_I32 (
          edge_value_1, edge_value_1 + edge_step_x_1,
          edge_value_1 + 2 * edge_step_x_1, edge_value_1 + 3 * edge_step_x_1);
      edge_value_2_lanes = SIMD128_SETR_I32 (
          edge_value_2, edge_value_2 + edge_step_x_2,
          edge_value_2 + 2 * edge_step_x_2, edge_value_2 + 3 * edge_step_x_2);
      // A pixel is covered only when all three signed edge values are
      // nonnegative
      coverage = SIMD128_COMPARE_GT_I32 (
          SIMD128_OR (SIMD128_OR (edge_value_0_lanes, edge_value_1_lanes),
                      edge_value_2_lanes),
          negative_one);
      destination = SIMD128_LOADU ((simd128i *)(pixels + x));

      if (transparent)
        {
          // Widen byte channels before multiplying to keep intermediate
          // values. within 16 bits, then round the division by 255
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
      edge_value_0 += 4 * edge_step_x_0;
      edge_value_1 += 4 * edge_step_x_1;
      edge_value_2 += 4 * edge_step_x_2;
    }

  for (; x < count; ++x)
    {
      if ((edge_value_0 | edge_value_1 | edge_value_2) >= 0)
        pixels[x] = transparent ? blend_rgba_pixel (color, pixels[x]) : color;

      edge_value_0 += edge_step_x_0;
      edge_value_1 += edge_step_x_1;
      edge_value_2 += edge_step_x_2;
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
local INIT_ONCE renderer_backend_once = INIT_ONCE_STATIC_INIT;

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
local pthread_once_t renderer_backend_once = PTHREAD_ONCE_INIT;

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
renderer_backend_init (void)
{
#if defined(_WIN32)
  BOOL initialized = InitOnceExecuteOnce (
      &renderer_backend_once, renderer_blend_rectangle_init, NULL, NULL);
  assert (initialized);
#else
  int result
      = pthread_once (&renderer_backend_once, renderer_blend_rectangle_init);
  assert (result == 0);
  (void)result;
#endif
}

void
renderer_blend_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                          u32 color)
{
  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

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
  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  if (width < 8
      && renderer_fill_rectangle_impl == renderer_fill_rectangle_avx2)
    renderer_fill_rectangle_sse2 (pixels, stride, width, height, color);
  else
    renderer_fill_rectangle_impl (pixels, stride, width, height, color);
}

RendererTriangleRowFn
renderer_get_triangle_row_renderer (s32 count)
{
  assert (count >= 0);
  if (count < 8 || renderer_triangle_row_impl == renderer_triangle_row_sse2)
    return renderer_triangle_row_sse2;

  return renderer_triangle_row_impl;
}
