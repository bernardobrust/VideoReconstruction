#include <assert.h>

#include "performance.h"
#include "renderer_internal.h"

#ifndef simd256i
#error AVX2 compiler target is required for renderer_simd_avx2.c
#endif

void
renderer_triangle_row_avx2 (u32 *pixels, s32 count, u32 color,
                            s32 edge_value_0, s32 edge_value_1,
                            s32 edge_value_2, s32 edge_step_x_0,
                            s32 edge_step_x_1, s32 edge_step_x_2,
                            bool transparent)
{
  s32 x;
  u8 alpha, inverse_alpha;
  simd256i source, alpha_mask, source_alpha, alpha_v, inverse_alpha_v, one,
      negative_one;

  if (transparent && (color >> 24) == 0)
    return;

  alpha = (u8)(color >> 24);
  inverse_alpha = 255 - alpha;
  source = SIMD256_SET1_I32 ((s32)color);
  alpha_mask = SIMD256_SET1_I32 ((s32)0xFF000000);
  source_alpha = SIMD256_AND (source, alpha_mask);
  alpha_v = SIMD256_SET1_I16 (alpha);
  inverse_alpha_v = SIMD256_SET1_I16 (inverse_alpha);
  one = SIMD256_SET1_I16 (1);
  negative_one = SIMD256_SET1_I32 (-1);

  for (x = 0; x + 8 <= count; x += 8)
    {
      simd256i destination, edge_value_0_lanes, edge_value_1_lanes,
          edge_value_2_lanes, coverage, zero, dst_lo, dst_hi, src_lo, src_hi,
          lo, hi, blended;

      edge_value_0_lanes = SIMD256_SETR_I32 (
          edge_value_0, edge_value_0 + edge_step_x_0,
          edge_value_0 + 2 * edge_step_x_0, edge_value_0 + 3 * edge_step_x_0,
          edge_value_0 + 4 * edge_step_x_0, edge_value_0 + 5 * edge_step_x_0,
          edge_value_0 + 6 * edge_step_x_0, edge_value_0 + 7 * edge_step_x_0);
      edge_value_1_lanes = SIMD256_SETR_I32 (
          edge_value_1, edge_value_1 + edge_step_x_1,
          edge_value_1 + 2 * edge_step_x_1, edge_value_1 + 3 * edge_step_x_1,
          edge_value_1 + 4 * edge_step_x_1, edge_value_1 + 5 * edge_step_x_1,
          edge_value_1 + 6 * edge_step_x_1, edge_value_1 + 7 * edge_step_x_1);
      edge_value_2_lanes = SIMD256_SETR_I32 (
          edge_value_2, edge_value_2 + edge_step_x_2,
          edge_value_2 + 2 * edge_step_x_2, edge_value_2 + 3 * edge_step_x_2,
          edge_value_2 + 4 * edge_step_x_2, edge_value_2 + 5 * edge_step_x_2,
          edge_value_2 + 6 * edge_step_x_2, edge_value_2 + 7 * edge_step_x_2);
      // A pixel is covered only when all three signed edge values are
      // nonnegative
      coverage = SIMD256_COMPARE_GT_I32 (
          SIMD256_OR (SIMD256_OR (edge_value_0_lanes, edge_value_1_lanes),
                      edge_value_2_lanes),
          negative_one);
      destination = SIMD256_LOADU ((simd256i *)(pixels + x));

      if (transparent)
        {
          // Widen byte channels before multiplying to keep intermediate
          // values. within 16 bits, then round the division by 255
          zero = SIMD256_ZERO ();
          dst_lo = SIMD256_UNPACKLO_U8 (destination, zero);
          dst_hi = SIMD256_UNPACKHI_U8 (destination, zero);
          src_lo = SIMD256_UNPACKLO_U8 (source, zero);
          src_hi = SIMD256_UNPACKHI_U8 (source, zero);
          lo = SIMD256_ADD_I16 (SIMD256_MULLO_I16 (src_lo, alpha_v),
                                SIMD256_MULLO_I16 (dst_lo, inverse_alpha_v));
          hi = SIMD256_ADD_I16 (SIMD256_MULLO_I16 (src_hi, alpha_v),
                                SIMD256_MULLO_I16 (dst_hi, inverse_alpha_v));
          lo = SIMD256_SHIFT_RIGHT_U16 (
              SIMD256_ADD_I16 (SIMD256_ADD_I16 (lo, one),
                               SIMD256_SHIFT_RIGHT_U16 (lo, 8)),
              8);
          hi = SIMD256_SHIFT_RIGHT_U16 (
              SIMD256_ADD_I16 (SIMD256_ADD_I16 (hi, one),
                               SIMD256_SHIFT_RIGHT_U16 (hi, 8)),
              8);
          blended = SIMD256_PACKUS_I16 (lo, hi);
          blended = SIMD256_OR (SIMD256_ANDNOT (alpha_mask, blended),
                                source_alpha);
          destination = SIMD256_OR (SIMD256_AND (coverage, blended),
                                    SIMD256_ANDNOT (coverage, destination));
        }
      else
        destination = SIMD256_OR (SIMD256_AND (coverage, source),
                                  SIMD256_ANDNOT (coverage, destination));

      SIMD256_STOREU ((simd256i *)(pixels + x), destination);
      edge_value_0 += 8 * edge_step_x_0;
      edge_value_1 += 8 * edge_step_x_1;
      edge_value_2 += 8 * edge_step_x_2;
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

local void
renderer_blend_row_avx2 (u32 *pixels, s32 count, u32 color)
{
  s32 x;
  u8 alpha, inverse_alpha;
  simd256i source, alpha_mask, source_alpha, alpha_v, inverse_alpha_v, one;

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
  source = SIMD256_SET1_I32 ((s32)color);
  alpha_mask = SIMD256_SET1_I32 ((s32)0xFF000000);
  source_alpha = SIMD256_AND (source, alpha_mask);
  alpha_v = SIMD256_SET1_I16 (alpha);
  inverse_alpha_v = SIMD256_SET1_I16 (inverse_alpha);
  one = SIMD256_SET1_I16 (1);

  for (x = 0; x + 8 <= count; x += 8)
    {
      simd256i destination, zero, dst_lo, dst_hi, src_lo, src_hi, lo, hi;

      destination = SIMD256_LOADU ((simd256i *)(pixels + x));
      zero = SIMD256_ZERO ();
      dst_lo = SIMD256_UNPACKLO_U8 (destination, zero);
      dst_hi = SIMD256_UNPACKHI_U8 (destination, zero);
      src_lo = SIMD256_UNPACKLO_U8 (source, zero);
      src_hi = SIMD256_UNPACKHI_U8 (source, zero);
      lo = SIMD256_ADD_I16 (SIMD256_MULLO_I16 (src_lo, alpha_v),
                            SIMD256_MULLO_I16 (dst_lo, inverse_alpha_v));
      hi = SIMD256_ADD_I16 (SIMD256_MULLO_I16 (src_hi, alpha_v),
                            SIMD256_MULLO_I16 (dst_hi, inverse_alpha_v));
      lo = SIMD256_SHIFT_RIGHT_U16 (
          SIMD256_ADD_I16 (SIMD256_ADD_I16 (lo, one),
                           SIMD256_SHIFT_RIGHT_U16 (lo, 8)),
          8);
      hi = SIMD256_SHIFT_RIGHT_U16 (
          SIMD256_ADD_I16 (SIMD256_ADD_I16 (hi, one),
                           SIMD256_SHIFT_RIGHT_U16 (hi, 8)),
          8);
      destination = SIMD256_PACKUS_I16 (lo, hi);
      destination = SIMD256_OR (SIMD256_ANDNOT (alpha_mask, destination),
                                source_alpha);
      SIMD256_STOREU ((simd256i *)(pixels + x), destination);
    }

  for (; x < count; ++x)
    pixels[x] = blend_rgba_pixel (color, pixels[x]);
}

void
renderer_blend_rectangle_avx2 (u32 *pixels, s32 stride, s32 width, s32 height,
                               u32 color)
{
  s32 y;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  for (y = 0; y < height; ++y)
    renderer_blend_row_avx2 (pixels + y * stride, width, color);
}

void
renderer_fill_rectangle_avx2 (u32 *pixels, s32 stride, s32 width, s32 height,
                              u32 color)
{
  s32 x, y;
  simd256i color_v;
  u32 *row;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  color_v = SIMD256_SET1_I32 ((s32)color);
  for (y = 0; y < height; ++y)
    {
      row = pixels + y * stride;
      for (x = 0; x + 8 <= width; x += 8)
        SIMD256_STOREU ((simd256i *)(row + x), color_v);

      for (; x < width; ++x)
        row[x] = color;
    }
}
