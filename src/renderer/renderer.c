#include "renderer.h"
#include "basic.h"
#include "performance.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>

unsigned
rgba (unsigned r, unsigned g, unsigned b, unsigned a)
{
  return a << 24 | r << 16 | g << 8 | b;
}

// For now it's this simple alocation, latter we can check for aspect ratio &&
// all
RendererPlex
init_renderer (int w, int h)
{
  RendererPlex rp;

  rp.w = w;
  rp.h = h;
  rp.image_buffer = (unsigned *)malloc (w * h * sizeof (unsigned));

  return rp;
}

void
draw_hline (int x0, int x1, int y, unsigned color, RendererPlex rp)
{
  if (y < 0 || y >= rp.h)
    return;

  // Wrong order
  if (unlikely (x0 > x1))
    {
      // Trust me it's not worth swapping with XOR
      int t = x0;
      x0 = x1;
      x1 = t;
    }

  x0 = clamp_int (x0, 0, rp.w - 1);
  x1 = clamp_int (x1, 0, rp.w - 1);

  unsigned *p = rp.image_buffer + y * rp.w + x0;
  for (int x = x0; x <= x1; ++x)
    *p++ = color;
}

void
draw_triangle (int x1, int y1, int x2, int y2, int x3, int y3, unsigned color,
               RendererPlex rp)
{
  // We'll be using barycentric coordinates for the triangle

  int min_x = MIN3 (x1, x2, x3);
  int max_x = MAX3 (x1, x2, x3);
  int min_y = MIN3 (y1, y2, y3);
  int max_y = MAX3 (y1, y2, y3);

  // Clamp to image buffer
  min_x = clamp_int (min_x, 0, rp.w - 1);
  max_x = clamp_int (max_x, 0, rp.w - 1);
  min_y = clamp_int (min_y, 0, rp.h - 1);
  max_y = clamp_int (max_y, 0, rp.h - 1);

  int area = determinant_ab_ap_int (x1, y1, x2, y2, x3, y3);

  // No need to render
  if (area == 0)
    return;

  for (int y = min_y; y <= max_y; ++y)
    {
      for (int x = min_x; x <= max_x; ++x)
        {
          int w0 = determinant_ab_ap_int (x2, y2, x3, y3, x, y);
          int w1 = determinant_ab_ap_int (x3, y3, x1, y1, x, y);
          int w2 = determinant_ab_ap_int (x1, y1, x2, y2, x, y);

          if ((area > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0)
              || (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
            rp.image_buffer[y * rp.w + x] = color;
        }
    }
}

void
draw_rectangle (int x1, int y1, int x2, int y2, unsigned color,
                RendererPlex rp)
{
  int min_x = MIN (x1, x2);
  int max_x = MAX (x1, x2);
  int min_y = MIN (y1, y2);
  int max_y = MAX (y1, y2);

  // Clamp to image buffer
  min_x = clamp_int (min_x, 0, rp.w - 1);
  max_x = clamp_int (max_x, 0, rp.w - 1);
  min_y = clamp_int (min_y, 0, rp.h - 1);
  max_y = clamp_int (max_y, 0, rp.h - 1);

  // No need to render
  if ((max_x - min_x) * (max_y - min_y) == 0)
    return;

  for (int y = min_y; y <= max_y; ++y)
    draw_hline (min_x, max_x, y, color, rp);
}

void
draw_circle (int cx, int cy, int r, unsigned color, RendererPlex rp)
{
  int x = r;
  int y = 0;
  int err = 1 - r;

  while (x >= y)
    {
      draw_hline (cx - x, cx + x, cy + y, color, rp);
      draw_hline (cx - x, cx + x, cy - y, color, rp);
      draw_hline (cx - y, cx + y, cy + x, color, rp);
      draw_hline (cx - y, cx + y, cy - x, color, rp);

      ++y;

      if (err < 0)
        err += 2 * y + 1;
      else
        {
          --x;
          err += 2 * (y - x) + 1;
        }
    }
}

// Call platform present to put image then zero out the buffer to clear it
inline void
renderer_present (PlatformState *platform_state, RendererPlex rp)
{
  platform_present (platform_state);
  memset (rp.image_buffer, 0, rp.w * rp.h * sizeof (unsigned));
}