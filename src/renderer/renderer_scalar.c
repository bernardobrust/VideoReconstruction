#include <assert.h>

#include "renderer_internal.h"

void
renderer_backend_init (void)
{
  // This is intentionally a no-op. Check renderer_simd.c for actual init code
}

void
renderer_fill_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                         u32 color)
{
  s32 x, y;
  u32 *pixel;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  for (y = 0; y < height; ++y)
    {
      pixel = pixels + y * stride;
      for (x = 0; x < width; ++x)
        pixel[x] = color;
    }
}

local void
renderer_triangle_row_scalar (u32 *pixels, s32 count, u32 color,
                              s32 edge_value_0, s32 edge_value_1,
                              s32 edge_value_2, s32 edge_step_x_0,
                              s32 edge_step_x_1, s32 edge_step_x_2,
                              bool transparent)
{
  s32 x;
  u32 weights;

  for (x = 0; x < count; ++x)
    {
      weights = (u32)(edge_value_0 | edge_value_1 | edge_value_2);
      if ((weights & 0x80000000u) == 0)
        pixels[x] = transparent ? blend_rgba_pixel (color, pixels[x]) : color;

      edge_value_0 += edge_step_x_0;
      edge_value_1 += edge_step_x_1;
      edge_value_2 += edge_step_x_2;
    }
}

RendererTriangleRowFn
renderer_get_triangle_row_renderer (s32 count)
{
  (void)count;
  return renderer_triangle_row_scalar;
}

void
renderer_blend_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                          u32 color)
{
  s32 x, y;
  u8 alpha;
  u32 *pixel;

  assert (pixels != NULL && stride >= width && width >= 0 && height >= 0);

  alpha = (u8)(color >> 24);
  if (alpha == 0)
    return;
  if (alpha == 255)
    {
      for (y = 0; y < height; ++y)
        {
          pixel = pixels + y * stride;
          for (x = 0; x < width; ++x)
            pixel[x] = color;
        }

      return;
    }

  for (y = 0; y < height; ++y)
    {
      pixel = pixels + y * stride;
      for (x = 0; x < width; ++x)
        pixel[x] = blend_rgba_pixel (color, pixel[x]);
    }
}
