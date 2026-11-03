#include <assert.h>

#include "renderer_internal.h"

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
renderer_triangle_row_scalar (u32 *pixels, s32 count, u32 color, s32 w0,
                              s32 w1, s32 w2, s32 a0, s32 a1, s32 a2,
                              bool transparent)
{
  s32 x;
  u32 weights;

  for (x = 0; x < count; ++x)
    {
      weights = (u32)(w0 | w1 | w2);
      if ((weights & 0x80000000u) == 0)
        pixels[x] = transparent ? blend_rgba_pixel (color, pixels[x]) : color;

      w0 += a0;
      w1 += a1;
      w2 += a2;
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
