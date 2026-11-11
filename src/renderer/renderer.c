#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "basic.h"
#include "performance.h"
#include "renderer.h"
#include "renderer_internal.h"

// Pack channels as 0xAARRGGBB. Opaque drawing ignores the alpha channel
u32
rgba (u32 r, u32 g, u32 b, u32 a)
{
  return a << 24 | r << 16 | g << 8 | b;
}

u8
blend_channel (u8 src, u8 dst, u8 alpha)
{
  s32 inverse_alpha = 255 - alpha, t = src * alpha + dst * inverse_alpha;
  return DIV_255 (t);
}

u32
blend_rgba_pixel (u32 src, u32 dst)
{
  u8 a = (src >> 24) & 0xFF;
  if (a == 0)
    return dst;
  if (a == 255)
    return src;

  u8 r = blend_channel ((src >> 16) & 0xFF, (dst >> 16) & 0xFF, a),
     g = blend_channel ((src >> 8) & 0xFF, (dst >> 8) & 0xFF, a),
     b = blend_channel (src & 0xFF, dst & 0xFF, a);

  return rgba (r, g, b, a);
}

// Interpolate blue-to-green-to-red across the unit interval
u32
interpolate_color_br (f32 interp_term)
{
  assert (interp_term >= 0 && interp_term <= 1.0f);

  u8 r, g, b;

  if (interp_term < 0.5f)
    {
      // The lower half transitions from blue to green
      float factor = interp_term * 2.0f; // Scale [0.0, 0.5] -> [0.0, 1.0]
      r = 0;
      g = (u8)(factor * 255.0f);
      b = (u8)((1.0f - factor) * 255.0f);
    }
  else
    {
      // The upper half transitions from green to red
      float factor
          = (interp_term - 0.5f) * 2.0f; // Scale [0.5, 1.0] -> [0.0, 1.0]
      r = (u8)(factor * 255.0f);
      g = (u8)((1.0f - factor) * 255.0f);
      b = 0;
    }

  return rgba (r, g, b, 180);
}

// Allocate a renderer and its tightly packed pixel buffer from the arena
RendererPlex *
init_renderer (Arena *arena, s32 w, s32 h)
{
  assert (arena != NULL && w > 0 && h > 0);
  assert ((u64)w <= UINT64_MAX / (u64)h / sizeof (u32));

  RendererPlex *renderer;

  renderer_backend_init ();
  renderer = arena_alloc (arena, sizeof (*renderer), _Alignof (RendererPlex));
  assert (renderer != NULL);

  renderer->w = w;
  renderer->h = h;
  renderer->image_buffer
      = arena_alloc (arena, (u64)w * h * sizeof (u32), _Alignof (u32));
  assert (renderer->image_buffer != NULL);

  return renderer;
}

void
draw_hline (s32 x0, s32 x1, s32 y, u32 color, RendererPlex *renderer)
{
  s32 width;

  assert (renderer != NULL);
  if (y < 0 || y >= renderer->h)
    return;

  // Normalize the endpoints so clipping and width calculation use one order
  if (unlikely (x0 > x1))
    {
      s32 t = x0;
      x0 = x1;
      x1 = t;
    }

  if (x1 < 0 || x0 >= renderer->w)
    return;

  x0 = clamp_s32 (x0, 0, renderer->w - 1);
  x1 = clamp_s32 (x1, 0, renderer->w - 1);

  width = x1 - x0 + 1;
  renderer_fill_rectangle (renderer->image_buffer + y * renderer->w + x0,
                           renderer->w, width, 1, color);
}

// Standard shapes

// Rasterize triangles with edge functions and pass each row to the backend
local void
draw_triangle_internal (s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3,
                        u32 color, bool transparent, RendererPlex *renderer)
{
  s32 min_x, max_x, min_y, max_y, area;
  s32 edge_step_x_0, edge_step_y_0, edge_step_x_1, edge_step_y_1;
  s32 edge_step_x_2, edge_step_y_2;
  s32 edge_value_0_row, edge_value_1_row, edge_value_2_row;
  s32 row_width, y, row_offset;
  RendererTriangleRowFn draw_row;

  assert (renderer != NULL);

  if (transparent && (color >> 24) == 0)
    return;

  min_x = MIN3 (x1, x2, x3);
  max_x = MAX3 (x1, x2, x3);
  min_y = MIN3 (y1, y2, y3);
  max_y = MAX3 (y1, y2, y3);

  if (max_x < 0 || min_x >= renderer->w || max_y < 0 || min_y >= renderer->h)
    return;

  min_x = clamp_s32 (min_x, 0, renderer->w - 1);
  max_x = clamp_s32 (max_x, 0, renderer->w - 1);
  min_y = clamp_s32 (min_y, 0, renderer->h - 1);
  max_y = clamp_s32 (max_y, 0, renderer->h - 1);

  area = determinant_ab_ap_s32 (x1, y1, x2, y2, x3, y3);
  if (area == 0)
    return;

  edge_step_x_0 = y3 - y2;
  edge_step_y_0 = x2 - x3;
  edge_step_x_1 = y1 - y3;
  edge_step_y_1 = x3 - x1;
  edge_step_x_2 = y2 - y1;
  edge_step_y_2 = x1 - x2;
  edge_value_0_row = determinant_ab_ap_s32 (x2, y2, x3, y3, min_x, min_y);
  edge_value_1_row = determinant_ab_ap_s32 (x3, y3, x1, y1, min_x, min_y);
  edge_value_2_row = determinant_ab_ap_s32 (x1, y1, x2, y2, min_x, min_y);

  // Just invert everything
  if (area < 0)
    {
      edge_step_x_0 = -edge_step_x_0;
      edge_step_y_0 = -edge_step_y_0;
      edge_value_0_row = -edge_value_0_row;
      edge_step_x_1 = -edge_step_x_1;
      edge_step_y_1 = -edge_step_y_1;
      edge_value_1_row = -edge_value_1_row;
      edge_step_x_2 = -edge_step_x_2;
      edge_step_y_2 = -edge_step_y_2;
      edge_value_2_row = -edge_value_2_row;
    }

  row_width = max_x - min_x + 1;
  draw_row = renderer_get_triangle_row_renderer (row_width);
  for (y = min_y; y <= max_y; ++y)
    {
      row_offset = y * renderer->w + min_x;
      draw_row (renderer->image_buffer + row_offset, row_width, color,
                edge_value_0_row, edge_value_1_row, edge_value_2_row,
                edge_step_x_0, edge_step_x_1, edge_step_x_2, transparent);

      edge_value_0_row += edge_step_y_0;
      edge_value_1_row += edge_step_y_1;
      edge_value_2_row += edge_step_y_2;
    }
}

void
draw_triangle (s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u32 color,
               RendererPlex *renderer)
{
  draw_triangle_internal (x1, y1, x2, y2, x3, y3, color, false, renderer);
}

void
draw_rectangle (s32 x1, s32 y1, s32 x2, s32 y2, u32 color,
                RendererPlex *renderer)
{
  s32 min_x, max_x, min_y, max_y, width, height;

  assert (renderer != NULL);

  min_x = MIN (x1, x2);
  max_x = MAX (x1, x2);
  min_y = MIN (y1, y2);
  max_y = MAX (y1, y2);

  if (max_x < 0 || min_x >= renderer->w || max_y < 0 || min_y >= renderer->h)
    return;

  min_x = clamp_s32 (min_x, 0, renderer->w - 1);
  max_x = clamp_s32 (max_x, 0, renderer->w - 1);
  min_y = clamp_s32 (min_y, 0, renderer->h - 1);
  max_y = clamp_s32 (max_y, 0, renderer->h - 1);

  if ((max_x - min_x) * (max_y - min_y) == 0)
    return;

  width = max_x - min_x + 1;
  height = max_y - min_y + 1;
  renderer_fill_rectangle (renderer->image_buffer + min_y * renderer->w
                               + min_x,
                           renderer->w, width, height, color);
}

void
draw_circle (s32 cx, s32 cy, s32 r, u32 color, RendererPlex *renderer)
{
  s32 x = r, y = 0, err = 1 - r;

  while (x >= y)
    {
      draw_hline (cx - x, cx + x, cy + y, color, renderer);
      draw_hline (cx - x, cx + x, cy - y, color, renderer);
      draw_hline (cx - y, cx + y, cy + x, color, renderer);
      draw_hline (cx - y, cx + y, cy - x, color, renderer);

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

// Build rotated rectangle corners from its center, dimensions, and angle
void
draw_rotated_rectangle (s32 cx, s32 cy, s32 w, s32 h, f32 theta, u32 color,
                        RendererPlex *renderer)
{
  f32 c = cosf (theta), s = sinf (theta);

  // Width direction
  f32 ux = c, uy = s;

  // Height direction
  f32 vx = -s, vy = c;

  f32 hw = w / 2.0f, hh = h / 2.0f;

  // Top-left
  f32 x0 = cx - hw * ux - hh * vx;
  f32 y0 = cy - hw * uy - hh * vy;

  // Top-right
  f32 x1 = cx + hw * ux - hh * vx;
  f32 y1 = cy + hw * uy - hh * vy;

  // Bottom-right
  f32 x2 = cx + hw * ux + hh * vx;
  f32 y2 = cy + hw * uy + hh * vy;

  // Bottom-left
  f32 x3 = cx - hw * ux + hh * vx;
  f32 y3 = cy - hw * uy + hh * vy;

  // Split the rectangle into two triangles along one diagonal
  draw_triangle ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                 renderer);
  draw_triangle ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                 renderer);
}

// Build a rectangle around the line segment between the two endpoints
void
draw_rotated_oriented_rectangle (s32 dx1, s32 dy1, s32 dx2, s32 dy2, s32 width,
                                 u32 color, RendererPlex *renderer)
{
  s32 dx = dx2 - dx1, dy = dy2 - dy1;
  f32 length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  // Unit vector along the center line
  f32 ux = dx / length, uy = dy / length;

  // Unit vector perpendicular to the center line
  f32 nx = -uy, ny = ux;

  // Offset from center line to either edge
  f32 hw = width / 2.0f;
  f32 ox = nx * hw, oy = ny * hw;

  // Four corners
  f32 x0 = dx1 + ox, y0 = dy1 + oy;
  f32 x1 = dx2 + ox, y1 = dy2 + oy;
  f32 x2 = dx2 - ox, y2 = dy2 - oy;
  f32 x3 = dx1 - ox, y3 = dy1 - oy;

  // Split the rectangle into two triangles along one diagonal
  draw_triangle ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                 renderer);
  draw_triangle ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                 renderer);
}

// Draw an arrow from a rectangular shaft and a triangular head
void
draw_arrow (s32 startx, s32 starty, s32 endx, s32 endy, s32 thickness,
            u32 color, RendererPlex *renderer)
{
  // Same geometry from draw_rotated_oriented_rectangle
  s32 dx = endx - startx, dy = endy - starty;
  f32 length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  f32 ux = dx / length, uy = dy / length;
  f32 nx = -uy, ny = ux;

  // Arbitrary scale, it should keep proportions to the shaft
  f32 head_thickness = thickness * 1.4f;

  f32 basex = endx - ux * head_thickness, basey = endy - uy * head_thickness;
  f32 leftx = basex + nx * head_thickness, lefty = basey + ny * head_thickness;
  f32 rightx = basex - nx * head_thickness,
      righty = basey - ny * head_thickness;

  f32 shaft_end_x = endx - ux * head_thickness,
      shaft_end_y = endy - uy * head_thickness;

  draw_rotated_oriented_rectangle ((int)startx, (int)starty, (int)shaft_end_x,
                                   (int)shaft_end_y, thickness, color,
                                   renderer);
  draw_triangle ((int)endx, (int)endy, (int)leftx, (int)lefty, (int)rightx,
                 (int)righty, color, renderer);
}

// Transparent shapes blend source color with existing pixels using source alpha
void
draw_triangle_t (s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3, u32 color,
                 RendererPlex *renderer)
{
  draw_triangle_internal (x1, y1, x2, y2, x3, y3, color, true, renderer);
}

void
draw_rectangle_t (s32 x1, s32 y1, s32 x2, s32 y2, u32 color,
                  RendererPlex *renderer)
{
  s32 min_x, max_x, min_y, max_y, width, height;

  assert (renderer != NULL);

  if ((color >> 24) == 0)
    return;

  min_x = MIN (x1, x2);
  max_x = MAX (x1, x2);
  min_y = MIN (y1, y2);
  max_y = MAX (y1, y2);

  if (max_x < 0 || min_x >= renderer->w || max_y < 0 || min_y >= renderer->h)
    return;

  min_x = clamp_s32 (min_x, 0, renderer->w - 1);
  max_x = clamp_s32 (max_x, 0, renderer->w - 1);
  min_y = clamp_s32 (min_y, 0, renderer->h - 1);
  max_y = clamp_s32 (max_y, 0, renderer->h - 1);

  if (max_x < min_x || max_y < min_y)
    return;

  width = max_x - min_x + 1;
  height = max_y - min_y + 1;
  renderer_blend_rectangle (renderer->image_buffer + min_y * renderer->w
                                + min_x,
                            renderer->w, width, height, color);
}

local void
draw_hline_t (s32 x0, s32 x1, s32 y, u32 color, RendererPlex *renderer)
{
  s32 width;

  assert (renderer != NULL);
  if (y < 0 || y >= renderer->h || x1 < 0 || x0 >= renderer->w)
    return;

  x0 = MAX (x0, 0);
  x1 = MIN (x1, renderer->w - 1);
  if (x0 > x1)
    return;

  width = x1 - x0 + 1;
  renderer_blend_rectangle (renderer->image_buffer + y * renderer->w + x0,
                            renderer->w, width, 1, color);
}

void
draw_circle_t (s32 cx, s32 cy, s32 r, u32 color, RendererPlex *renderer)
{
  assert (renderer != NULL);
  s32 x = r, y = 0, err = 1 - r;

  while (x >= y)
    {
      draw_hline_t (cx - x, cx + x, cy + y, color, renderer);
      if (y != 0)
        draw_hline_t (cx - x, cx + x, cy - y, color, renderer);

      if (x != y)
        {
          draw_hline_t (cx - y, cx + y, cy + x, color, renderer);
          if (x != 0)
            draw_hline_t (cx - y, cx + y, cy - x, color, renderer);
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
draw_rotated_rectangle_t (s32 cx, s32 cy, s32 w, s32 h, f32 theta, u32 color,
                          RendererPlex *renderer)
{
  f32 c = cosf (theta);
  f32 s = sinf (theta);

  // Width direction
  f32 ux = c, uy = s;

  // Height direction
  f32 vx = -s, vy = c;

  f32 hw = w / 2.0f;
  f32 hh = h / 2.0f;

  // Top-left
  f32 x0 = cx - hw * ux - hh * vx;
  f32 y0 = cy - hw * uy - hh * vy;

  // Top-right
  f32 x1 = cx + hw * ux - hh * vx;
  f32 y1 = cy + hw * uy - hh * vy;

  // Bottom-right
  f32 x2 = cx + hw * ux + hh * vx;
  f32 y2 = cy + hw * uy + hh * vy;

  // Bottom-left
  f32 x3 = cx - hw * ux + hh * vx;
  f32 y3 = cy - hw * uy + hh * vy;

  // Triangle 1 points:
  // (x0, y0), (x1, y1), (x2, y2)
  // Triangle 2 points:
  // (x0, y0), (x2, y2), (x3, y3)
  draw_triangle_t ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                   renderer);
  draw_triangle_t ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                   renderer);
}

void
draw_rotated_oriented_rectangle_t (s32 dx1, s32 dy1, s32 dx2, s32 dy2,
                                   s32 width, u32 color,
                                   RendererPlex *renderer)
{
  s32 dx = dx2 - dx1, dy = dy2 - dy1;
  f32 length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  // Unit vector along the center line
  f32 ux = dx / length, uy = dy / length;

  // Unit vector perpendicular to the center line
  f32 nx = -uy, ny = ux;

  // Offset from center line to either edge
  f32 hw = width / 2.0f;
  f32 ox = nx * hw, oy = ny * hw;

  // Four corners
  f32 x0 = dx1 + ox, y0 = dy1 + oy;
  f32 x1 = dx2 + ox, y1 = dy2 + oy;
  f32 x2 = dx2 - ox, y2 = dy2 - oy;
  f32 x3 = dx1 - ox, y3 = dy1 - oy;

  // Triangle 1 points:
  // (x0, y0), (x1, y1), (x2, y2)
  // Triangle 2 points:
  // (x0, y0), (x2, y2), (x3, y3)
  draw_triangle_t ((int)x0, (int)y0, (int)x1, (int)y1, (int)x2, (int)y2, color,
                   renderer);
  draw_triangle_t ((int)x0, (int)y0, (int)x2, (int)y2, (int)x3, (int)y3, color,
                   renderer);
}

void
draw_arrow_t (s32 startx, s32 starty, s32 endx, s32 endy, s32 thickness,
              u32 color, RendererPlex *renderer)
{
  // Same geometry from draw_rotated_oriented_rectangle
  s32 dx = endx - startx, dy = endy - starty;
  f32 length = sqrtf ((float)dx * (float)dx + (float)dy * (float)dy);

  if (length == 0.0f)
    return;

  f32 ux = dx / length, uy = dy / length;
  f32 nx = -uy, ny = ux;

  // Arbitrary scale, it should keep proportions to the shaft
  f32 head_thickness = thickness * 1.4f;

  f32 basex = endx - ux * head_thickness, basey = endy - uy * head_thickness;
  f32 leftx = basex + nx * head_thickness, lefty = basey + ny * head_thickness;
  f32 rightx = basex - nx * head_thickness,
      righty = basey - ny * head_thickness;

  f32 shaft_end_x = endx - ux * head_thickness,
      shaft_end_y = endy - uy * head_thickness;

  draw_rotated_oriented_rectangle_t ((int)startx, (int)starty,
                                     (int)shaft_end_x, (int)shaft_end_y,
                                     thickness, color, renderer);
  draw_triangle_t ((int)endx, (int)endy, (int)leftx, (int)lefty, (int)rightx,
                   (int)righty, color, renderer);
}

// Present the current image and keep it available until a newer frame replaces
// it. The platform buffer is persistent across UI updates.
void
renderer_present (PlatformState *platform_state, RendererPlex *renderer)
{
  assert (renderer != NULL);
  platform_present (platform_state);
  (void)renderer;
}
