#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basic.h"
#include "performance.h"
#include "renderer.h"

// The 'a' only affects transparent shapes
unsigned
rgba (unsigned r, unsigned g, unsigned b, unsigned a)
{
  return a << 24 | r << 16 | g << 8 | b;
}

uint8_t
blend_channel (uint8_t src, uint8_t dst, uint8_t alpha)
{
  int inverse_alpha = 255 - alpha, t = src * alpha + dst * inverse_alpha;
  return DIV_255 (t);
}

unsigned
blend_rgba_pixel (unsigned src, unsigned dst)
{
  uint8_t a = (src >> 24) & 0xFF;
  if (a == 0)
    return dst;
  if (a == 255)
    return src;

  uint8_t r = blend_channel ((src >> 16) & 0xFF, (dst >> 16) & 0xFF, a),
          g = blend_channel ((src >> 8) & 0xFF, (dst >> 8) & 0xFF, a),
          b = blend_channel (src & 0xFF, dst & 0xFF, a);

  return rgba (r, g, b, a);
}

// For now it's this simple alocation, latter we can check for aspect ratio &&
// all
RendererPlex *
init_renderer (int w, int h)
{
  RendererPlex *rp = (RendererPlex *)malloc (sizeof (RendererPlex));
  if (!rp)
    {
      fprintf (stderr, "Could not allocate RendererPlex.\n");
      return NULL;
    }

  rp->w = w;
  rp->h = h;
  rp->image_buffer = (unsigned *)malloc (w * h * sizeof (unsigned));

  return rp;
}

void
draw_hline (int x0, int x1, int y, unsigned color, RendererPlex *rp)
{
  if (y < 0 || y >= rp->h)
    return;

  // Wrong order
  if (unlikely (x0 > x1))
    {
      // Trust me it's not worth swapping with XOR
      int t = x0;
      x0 = x1;
      x1 = t;
    }

  x0 = clamp_int (x0, 0, rp->w - 1);
  x1 = clamp_int (x1, 0, rp->w - 1);

  unsigned *p = rp->image_buffer + y * rp->w + x0;
  for (int x = x0; x <= x1; ++x)
    *p++ = color;
}

// Standard shapes

// Previously I was using barycentric coordinates as TSoding used in Olive.c,
// but profiling with VTune pointed that that was highly inefficient. So I
// decided to use the edge function method, which, being honest, is also easier
// to reason about.
void
draw_triangle (int x1, int y1, int x2, int y2, int x3, int y3, unsigned color,
               RendererPlex *rp)
{
  int min_x = MIN3 (x1, x2, x3), max_x = MAX3 (x1, x2, x3);
  int min_y = MIN3 (y1, y2, y3), max_y = MAX3 (y1, y2, y3);

  min_x = clamp_int (min_x, 0, rp->w - 1);
  max_x = clamp_int (max_x, 0, rp->w - 1);
  min_y = clamp_int (min_y, 0, rp->h - 1);
  max_y = clamp_int (max_y, 0, rp->h - 1);

  int area = determinant_ab_ap_int (x1, y1, x2, y2, x3, y3);
  if (area == 0)
    return;

  int a0 = y3 - y2, b0 = x2 - x3;
  int a1 = y1 - y3, b1 = x3 - x1;
  int a2 = y2 - y1, b2 = x1 - x2;

  int w0_row = determinant_ab_ap_int (x2, y2, x3, y3, min_x, min_y),
      w1_row = determinant_ab_ap_int (x3, y3, x1, y1, min_x, min_y),
      w2_row = determinant_ab_ap_int (x1, y1, x2, y2, min_x, min_y);

  // Just invert everything
  if (area < 0)
    {
      a0 = -a0;
      b0 = -b0;
      w0_row = -w0_row;
      a1 = -a1;
      b1 = -b1;
      w1_row = -w1_row;
      a2 = -a2;
      b2 = -b2;
      w2_row = -w2_row;
    }

  for (int y = min_y; y <= max_y; ++y)
    {
      int w0 = w0_row, w1 = w1_row, w2 = w2_row;
      int row_offset = y * rp->w;

      for (int x = min_x; x <= max_x; ++x)
        {
          if ((w0 | w1 | w2) >= 0)
            rp->image_buffer[row_offset + x] = color;

          w0 += a0;
          w1 += a1;
          w2 += a2;
        }

      w0_row += b0;
      w1_row += b1;
      w2_row += b2;
    }
}

void
draw_rectangle (int x1, int y1, int x2, int y2, unsigned color,
                RendererPlex *rp)
{
  int min_x = MIN (x1, x2), max_x = MAX (x1, x2);
  int min_y = MIN (y1, y2), max_y = MAX (y1, y2);

  // Clamp to image buffer
  min_x = clamp_int (min_x, 0, rp->w - 1);
  max_x = clamp_int (max_x, 0, rp->w - 1);
  min_y = clamp_int (min_y, 0, rp->h - 1);
  max_y = clamp_int (max_y, 0, rp->h - 1);

  // No need to render
  if ((max_x - min_x) * (max_y - min_y) == 0)
    return;

  for (int y = min_y; y <= max_y; ++y)
    draw_hline (min_x, max_x, y, color, rp);
}

void
draw_circle (int cx, int cy, int r, unsigned color, RendererPlex *rp)
{
  int x = r, y = 0, err = 1 - r;

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

// For the rotated rectangle it makes more sense to take center, width, height
// and angle instead of four points. It's geometrically easier to reason about
// it this way
void
draw_rotated_rectangle (int cx, int cy, int w, int h, float theta,
                        unsigned color, RendererPlex *rp)
{
  float c = cosf (theta);
  float s = sinf (theta);

  // Width direction
  float ux = c, uy = s;

  // Height direction
  float vx = -s, vy = c;

  float hw = w / 2.0f;
  float hh = h / 2.0f;

  // Top-left
  float x0 = cx - hw * ux - hh * vx;
  float y0 = cy - hw * uy - hh * vy;

  // Top-right
  float x1 = cx + hw * ux - hh * vx;
  float y1 = cy + hw * uy - hh * vy;

  // Bottom-right
  float x2 = cx + hw * ux + hh * vx;
  float y2 = cy + hw * uy + hh * vy;

  // Bottom-left
  float x3 = cx - hw * ux + hh * vx;
  float y3 = cy - hw * uy + hh * vy;

  // Triangle 1 points:
  // (x0, y0), (x1, y1), (x2, y2)
  // Triangle 2 points:
  // (x0, y0), (x2, y2), (x3, y3)
  draw_triangle ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                 rp);
  draw_triangle ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                 rp);
}

// It's also reasonable to think about in terms of start, end + width. The
// start -> end vector gives a direction along the center line, this is usefull
// for the arrow
void
draw_rotated_oriented_rectangle (int dx1, int dy1, int dx2, int dy2, int width,
                                 unsigned color, RendererPlex *rp)
{
  int dx = dx2 - dx1, dy = dy2 - dy1;
  float length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  // Unit vector along the center line
  float ux = dx / length, uy = dy / length;

  // Unit vector perpendicular to the center line
  float nx = -uy, ny = ux;

  // Offset from center line to either edge
  float hw = width / 2.0f;
  float ox = nx * hw, oy = ny * hw;

  // Four corners
  float x0 = dx1 + ox, y0 = dy1 + oy;
  float x1 = dx2 + ox, y1 = dy2 + oy;
  float x2 = dx2 - ox, y2 = dy2 - oy;
  float x3 = dx1 - ox, y3 = dy1 - oy;

  // Triangle 1 points:
  // (x0, y0), (x1, y1), (x2, y2)
  // Triangle 2 points:
  // (x0, y0), (x2, y2), (x3, y3)
  draw_triangle ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                 rp);
  draw_triangle ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                 rp);
}

// It's just a rectangle with a triangle on top
void
draw_arrow (int startx, int starty, int endx, int endy, int thickness,
            unsigned color, RendererPlex *rp)
{
  // Same geometry from draw_rotated_oriented_rectangle
  int dx = endx - startx, dy = endy - starty;
  float length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  float ux = dx / length, uy = dy / length;
  float nx = -uy, ny = ux;

  // Arbitrary scale, it should keep proportions to the shaft
  float head_thickness = thickness * 1.4f;

  float basex = endx - ux * head_thickness, basey = endy - uy * head_thickness;
  float leftx = basex + nx * head_thickness,
        lefty = basey + ny * head_thickness;
  float rightx = basex - nx * head_thickness,
        righty = basey - ny * head_thickness;

  float shaft_end_x = endx - ux * head_thickness,
        shaft_end_y = endy - uy * head_thickness;

  draw_rotated_oriented_rectangle ((int)startx, (int)starty, (int)shaft_end_x,
                                   (int)shaft_end_y, thickness, color, rp);
  draw_triangle ((int)endx, (int)endy, (int)leftx, (int)lefty, (int)rightx,
                 (int)righty, color, rp);
}

// Transparent shapes
// In these shapes we blend the color passed with the already existing color in
// the image buffer. The 'a' channel of the color passed is used for blending
// Moreover, we can't just create a new color from 1 pixel or use "draw_hline"
// because the color of each pixel is different, so we have to blend each pixel
// individually
void
draw_triangle_t (int x1, int y1, int x2, int y2, int x3, int y3,
                 unsigned color, RendererPlex *rp)
{
  int min_x = MIN3 (x1, x2, x3), max_x = MAX3 (x1, x2, x3);
  int min_y = MIN3 (y1, y2, y3), max_y = MAX3 (y1, y2, y3);

  min_x = clamp_int (min_x, 0, rp->w - 1);
  max_x = clamp_int (max_x, 0, rp->w - 1);
  min_y = clamp_int (min_y, 0, rp->h - 1);
  max_y = clamp_int (max_y, 0, rp->h - 1);

  int area = determinant_ab_ap_int (x1, y1, x2, y2, x3, y3);
  if (area == 0)
    return;

  int a0 = y3 - y2, b0 = x2 - x3;
  int a1 = y1 - y3, b1 = x3 - x1;
  int a2 = y2 - y1, b2 = x1 - x2;

  int w0_row = determinant_ab_ap_int (x2, y2, x3, y3, min_x, min_y),
      w1_row = determinant_ab_ap_int (x3, y3, x1, y1, min_x, min_y),
      w2_row = determinant_ab_ap_int (x1, y1, x2, y2, min_x, min_y);

  // Just invert everything
  if (area < 0)
    {
      a0 = -a0;
      b0 = -b0;
      w0_row = -w0_row;
      a1 = -a1;
      b1 = -b1;
      w1_row = -w1_row;
      a2 = -a2;
      b2 = -b2;
      w2_row = -w2_row;
    }

  for (int y = min_y; y <= max_y; ++y)
    {
      int w0 = w0_row, w1 = w1_row, w2 = w2_row;
      int row_offset = y * rp->w;

      for (int x = min_x; x <= max_x; ++x)
        {
          if ((w0 | w1 | w2) >= 0)
            rp->image_buffer[row_offset + x]
                = blend_rgba_pixel (color, rp->image_buffer[row_offset + x]);

          w0 += a0;
          w1 += a1;
          w2 += a2;
        }

      w0_row += b0;
      w1_row += b1;
      w2_row += b2;
    }
}

void
draw_rectangle_t (int x1, int y1, int x2, int y2, unsigned color,
                  RendererPlex *rp)
{
  int min_x = MIN (x1, x2), max_x = MAX (x1, x2);
  int min_y = MIN (y1, y2), max_y = MAX (y1, y2);

  // Clamp to image buffer
  min_x = clamp_int (min_x, 0, rp->w - 1);
  max_x = clamp_int (max_x, 0, rp->w - 1);
  min_y = clamp_int (min_y, 0, rp->h - 1);
  max_y = clamp_int (max_y, 0, rp->h - 1);

  // No need to render
  if ((max_x - min_x) * (max_y - min_y) == 0)
    return;

  for (int y = min_y; y <= max_y; ++y)
    for (int x = min_x; x <= max_x; ++x)
      rp->image_buffer[y * rp->w + x]
          = blend_rgba_pixel (color, rp->image_buffer[y * rp->w + x]);
}

void
draw_circle_t (int cx, int cy, int r, unsigned color, RendererPlex *rp)
{
  int x = r, y = 0, err = 1 - r;

  while (x >= y)
    {
      // @TODO, @FIX: This is not the most efficient way to do this, we can do
      // better with a single loop and some math, but for now this is fine. It
      // looks kinda goofy anyway
      for (int i = cx - x; i <= cx + x; ++i)
        {
          rp->image_buffer[(cy + y) * rp->w + i] = blend_rgba_pixel (
              color, rp->image_buffer[(cy + y) * rp->w + i]);
          rp->image_buffer[(cy - y) * rp->w + i] = blend_rgba_pixel (
              color, rp->image_buffer[(cy - y) * rp->w + i]);
        }

      for (int i = cx - y; i <= cx + y; ++i)
        {
          rp->image_buffer[(cy + x) * rp->w + i] = blend_rgba_pixel (
              color, rp->image_buffer[(cy + x) * rp->w + i]);
          rp->image_buffer[(cy - x) * rp->w + i] = blend_rgba_pixel (
              color, rp->image_buffer[(cy - x) * rp->w + i]);
        }

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

void
draw_rotated_rectangle_t (int cx, int cy, int w, int h, float theta,
                          unsigned color, RendererPlex *rp)
{
  float c = cosf (theta);
  float s = sinf (theta);

  // Width direction
  float ux = c, uy = s;

  // Height direction
  float vx = -s, vy = c;

  float hw = w / 2.0f;
  float hh = h / 2.0f;

  // Top-left
  float x0 = cx - hw * ux - hh * vx;
  float y0 = cy - hw * uy - hh * vy;

  // Top-right
  float x1 = cx + hw * ux - hh * vx;
  float y1 = cy + hw * uy - hh * vy;

  // Bottom-right
  float x2 = cx + hw * ux + hh * vx;
  float y2 = cy + hw * uy + hh * vy;

  // Bottom-left
  float x3 = cx - hw * ux + hh * vx;
  float y3 = cy - hw * uy + hh * vy;

  // Triangle 1 points:
  // (x0, y0), (x1, y1), (x2, y2)
  // Triangle 2 points:
  // (x0, y0), (x2, y2), (x3, y3)
  draw_triangle_t ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                   rp);
  draw_triangle_t ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                   rp);
}

void
draw_rotated_oriented_rectangle_t (int dx1, int dy1, int dx2, int dy2,
                                   int width, unsigned color, RendererPlex *rp)
{
  int dx = dx2 - dx1, dy = dy2 - dy1;
  float length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  // Unit vector along the center line
  float ux = dx / length, uy = dy / length;

  // Unit vector perpendicular to the center line
  float nx = -uy, ny = ux;

  // Offset from center line to either edge
  float hw = width / 2.0f;
  float ox = nx * hw, oy = ny * hw;

  // Four corners
  float x0 = dx1 + ox, y0 = dy1 + oy;
  float x1 = dx2 + ox, y1 = dy2 + oy;
  float x2 = dx2 - ox, y2 = dy2 - oy;
  float x3 = dx1 - ox, y3 = dy1 - oy;

  // Triangle 1 points:
  // (x0, y0), (x1, y1), (x2, y2)
  // Triangle 2 points:
  // (x0, y0), (x2, y2), (x3, y3)
  draw_triangle_t ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                   rp);
  draw_triangle_t ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                   rp);
}

void
draw_arrow_t (int startx, int starty, int endx, int endy, int thickness,
              unsigned color, RendererPlex *rp)
{
  // Same geometry from draw_rotated_oriented_rectangle
  int dx = endx - startx, dy = endy - starty;
  float length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  float ux = dx / length, uy = dy / length;
  float nx = -uy, ny = ux;

  // Arbitrary scale, it should keep proportions to the shaft
  float head_thickness = thickness * 1.4f;

  float basex = endx - ux * head_thickness, basey = endy - uy * head_thickness;
  float leftx = basex + nx * head_thickness,
        lefty = basey + ny * head_thickness;
  float rightx = basex - nx * head_thickness,
        righty = basey - ny * head_thickness;

  float shaft_end_x = endx - ux * head_thickness,
        shaft_end_y = endy - uy * head_thickness;

  draw_rotated_oriented_rectangle_t ((int)startx, (int)starty,
                                     (int)shaft_end_x, (int)shaft_end_y,
                                     thickness, color, rp);
  draw_triangle_t ((int)endx, (int)endy, (int)leftx, (int)lefty, (int)rightx,
                   (int)righty, color, rp);
}

// Call platform present to put image then zero out the buffer to clear it
void
renderer_present (PlatformState *platform_state, RendererPlex *rp)
{
  platform_present (platform_state);
  memset (rp->image_buffer, 0, rp->w * rp->h * sizeof (unsigned));
}