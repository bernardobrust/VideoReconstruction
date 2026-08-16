#include "renderer.h"
#include "basic.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>

unsigned
rgba (unsigned r, unsigned g, unsigned b, unsigned a)
{
  return a << 24 | r << 16 | g << 8 | b;
}

// For now it's this simple alocation, latter we can check for aspect ratio and
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
  if (x0 > x1)
    {
      // Trust me it's not worth swapping with XOR
      int t = x0;
      x0 = x1;
      x1 = t;
    }

  x0 = clamp_int (x0, 0, rp.w - 1);
  x1 = clamp_int (x1, 0, rp.w - 1);

  for (int i = 0; i < x1 - x0; ++i)
    *(rp.image_buffer + y * rp.w + x0 + i) = color;
}

// Call platform present to put image then zero out the buffer to clear it
inline void
renderer_present (PlatformState *platform_state, RendererPlex rp)
{
  platform_present (platform_state);
  memset (rp.image_buffer, 0, rp.w * rp.h * sizeof (unsigned));
}