#define ASTF_STRIP_PREFIX
#include "renderer.h"
#include "astf.h"
#include "basic.h"
#include "renderer.test.h"

#include <string.h>

#define TEST_W 7
#define TEST_H 7
#define TEST_PIXELS (TEST_W * TEST_H)

static void
renderer_reference_triangle_t (s32 x1, s32 y1, s32 x2, s32 y2, s32 x3,
                                s32 y3, u32 color, bool transparent,
                                RendererPlex *rp)
{
  s32 min_x, max_x, min_y, max_y, area, a0, b0, a1, b1, a2, b2, w0_row,
      w1_row, w2_row, x, y;

  min_x = clamp_s32 (MIN3 (x1, x2, x3), 0, rp->w - 1);
  max_x = clamp_s32 (MAX3 (x1, x2, x3), 0, rp->w - 1);
  min_y = clamp_s32 (MIN3 (y1, y2, y3), 0, rp->h - 1);
  max_y = clamp_s32 (MAX3 (y1, y2, y3), 0, rp->h - 1);
  area = determinant_ab_ap_s32 (x1, y1, x2, y2, x3, y3);
  if (area == 0)
    return;

  a0 = y3 - y2;
  b0 = x2 - x3;
  a1 = y1 - y3;
  b1 = x3 - x1;
  a2 = y2 - y1;
  b2 = x1 - x2;
  w0_row = determinant_ab_ap_s32 (x2, y2, x3, y3, min_x, min_y);
  w1_row = determinant_ab_ap_s32 (x3, y3, x1, y1, min_x, min_y);
  w2_row = determinant_ab_ap_s32 (x1, y1, x2, y2, min_x, min_y);

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

  for (y = min_y; y <= max_y; ++y)
    {
      s32 w0, w1, w2, row_offset;

      w0 = w0_row;
      w1 = w1_row;
      w2 = w2_row;
      row_offset = y * rp->w;
      for (x = min_x; x <= max_x; ++x)
        {
          if ((w0 | w1 | w2) >= 0)
            rp->image_buffer[row_offset + x]
                = transparent
                      ? blend_rgba_pixel (color,
                                         rp->image_buffer[row_offset + x])
                      : color;

          w0 += a0;
          w1 += a1;
          w2 += a2;
        }

      w0_row += b0;
      w1_row += b1;
      w2_row += b2;
    }
}

static void
renderer_test_colors (void)
{
  start_test_suite ("Renderer colors: packing, blending, interpolation");

  assert_equal (0xFF123456, rgba (0x12, 0x34, 0x56, 0xFF));

  assert_equal (0xFF123456, blend_rgba_pixel (0xFF123456, 0xFFABCDEF));
  assert_equal (0xFFABCDEF, blend_rgba_pixel (0x00123456, 0xFFABCDEF));
  assert_equal (0x8080007F, blend_rgba_pixel (0x80FF0000, 0xFF0000FF));

  assert_equal (0, blend_channel (0, 0, 0));
  assert_equal (255, blend_channel (255, 255, 128));

  assert_equal (0xB40000FF, interpolate_color_br (0.0f));
  assert_equal (0xB4007F7F, interpolate_color_br (0.25f));
  assert_equal (0xB400FF00, interpolate_color_br (0.5f));
  assert_equal (0xB47F7F00, interpolate_color_br (0.75f));
  assert_equal (0xB4FF0000, interpolate_color_br (1.0f));

  retrieve_results ();
}

static void
renderer_test_lines_and_rectangles (void)
{
  start_test_suite ("Renderer rasterization: lines and rectangles");

  u32 pixels[TEST_PIXELS] = { 0 };
  RendererPlex rp = { TEST_W, TEST_H, pixels };

  draw_hline (5, 1, 2, 0xFF010203, &rp);
  for (s32 x = 0; x < TEST_W; ++x)
    assert_equal (x >= 1 && x <= 5 ? 0xFF010203 : 0, pixels[2 * TEST_W + x]);

  draw_hline (-5, 3, 3, 0xFF040506, &rp);
  for (s32 x = 0; x < TEST_W; ++x)
    assert_equal (x <= 3 ? 0xFF040506 : 0, pixels[3 * TEST_W + x]);

  draw_hline (0, 6, -1, 0xFFFFFFFF, &rp);
  draw_hline (0, 6, TEST_H, 0xFFFFFFFF, &rp);
  draw_hline (-5, 20, 6, 0xFFABCDEF, &rp);
  for (s32 x = 0; x < TEST_W; ++x)
    assert_equal (0xFFABCDEF, pixels[6 * TEST_W + x]);

  draw_hline (-5, -1, 4, 0xFFFFFFFF, &rp);
  draw_hline (TEST_W, TEST_W + 4, 4, 0xFFFFFFFF, &rp);
  for (s32 x = 0; x < TEST_W; ++x)
    assert_equal (x <= 3 ? 0xFF040506 : 0, pixels[3 * TEST_W + x]);

  assert_equal (0, pixels[0]);

  memset (pixels, 0, sizeof pixels);
  draw_rectangle (5, 5, 2, 2, 0xFF112233, &rp);
  for (s32 y = 0; y < TEST_H; ++y)
    for (s32 x = 0; x < TEST_W; ++x)
      assert_equal (x >= 2 && x <= 5 && y >= 2 && y <= 5 ? 0xFF112233 : 0,
                    pixels[y * TEST_W + x]);

  memset (pixels, 0, sizeof pixels);
  draw_rectangle (-3, -2, 2, 1, 0xFF445566, &rp);

  assert_equal (0xFF445566, pixels[0]);
  assert_equal (0xFF445566, pixels[1]);
  assert_equal (0xFF445566, pixels[TEST_W]);
  assert_equal (0, pixels[2 * TEST_W]);
  assert_equal (0, pixels[2 * TEST_W]);

  memset (pixels, 0, sizeof pixels);
  draw_rectangle (3, 1, 3, 5, 0xFFFFFFFF, &rp);
  assert_equal (0, pixels[1 * TEST_W + 3]);

  memset (pixels, 0, sizeof pixels);
  draw_rectangle (-20, -20, -1, -1, 0xFFFFFFFF, &rp);
  draw_rectangle (TEST_W, TEST_H, TEST_W + 2, TEST_H + 2, 0xFFFFFFFF, &rp);
  for (s32 i = 0; i < TEST_PIXELS; ++i)
    assert_equal (0, pixels[i]);

  retrieve_results ();
}

static void
renderer_test_triangles (void)
{
  start_test_suite ("Renderer rasterization: triangle winding and edges");

  u32 pixels[TEST_PIXELS] = { 0 };
  u32 reversed[TEST_PIXELS] = { 0 };
  RendererPlex rp = { TEST_W, TEST_H, pixels };
  const u32 color = 0xFFABCDEF;

  draw_triangle (1, 1, 5, 1, 1, 5, color, &rp);

  assert_equal (color, pixels[1 * TEST_W + 1]);
  assert_equal (color, pixels[2 * TEST_W + 2]);
  assert_equal (color, pixels[5 * TEST_W + 1]);
  assert_equal (0, pixels[5 * TEST_W + 5]);

  memcpy (reversed, pixels, sizeof pixels);
  memset (pixels, 0, sizeof pixels);
  draw_triangle (1, 5, 5, 1, 1, 1, color, &rp);

  assert_equal (0, memcmp (reversed, pixels, sizeof pixels));

  memset (pixels, 0, sizeof pixels);
  draw_triangle (1, 1, 3, 3, 5, 5, color, &rp);
  for (s32 i = 0; i < TEST_PIXELS; ++i)
    assert_equal (0, pixels[i]);

  draw_triangle (-3, 1, 3, 1, 0, 5, color, &rp);
  assert_equal (color, pixels[1 * TEST_W]);
  assert_equal (color, pixels[3 * TEST_W]);
  assert_equal (0, pixels[6 * TEST_W + 6]);

  memset (pixels, 0, sizeof pixels);
  draw_circle (3, 3, 2, color, &rp);
  assert_equal (color, pixels[1 * TEST_W + 3]);
  assert_equal (color, pixels[5 * TEST_W + 3]);
  assert_equal (color, pixels[3 * TEST_W + 1]);
  assert_equal (color, pixels[3 * TEST_W + 5]);
  assert_equal (0, pixels[0]);

  retrieve_results ();
}

static void
renderer_test_circles (void)
{
  start_test_suite ("Renderer rasterization: circles and clipping");

  u32 pixels[TEST_PIXELS] = { 0 };
  RendererPlex rp = { TEST_W, TEST_H, pixels };
  const u32 color = 0xFF778899;

  draw_circle (3, 3, 0, color, &rp);
  assert_equal (color, pixels[3 * TEST_W + 3]);
  assert_equal (0, pixels[3 * TEST_W + 2]);

  memset (pixels, 0, sizeof pixels);
  draw_circle (3, 3, 1, color, &rp);

  assert_equal (color, pixels[3 * TEST_W + 3]);
  assert_equal (color, pixels[3 * TEST_W + 2]);
  assert_equal (color, pixels[3 * TEST_W + 4]);
  assert_equal (color, pixels[2 * TEST_W + 3]);
  assert_equal (color, pixels[4 * TEST_W + 3]);
  assert_equal (0, pixels[2 * TEST_W + 2]);

  memset (pixels, 0, sizeof pixels);
  draw_circle (0, 0, 2, color, &rp);

  assert_equal (color, pixels[0]);
  assert_equal (color, pixels[2]);
  assert_equal (color, pixels[2 * TEST_W]);
  assert_equal (0, pixels[6 * TEST_W + 6]);

  memset (pixels, 0, sizeof pixels);
  const u32 translucent = 0x800000FF;
  draw_circle_t (3, 3, 1, translucent, &rp);
  assert_equal (blend_rgba_pixel (translucent, 0), pixels[3 * TEST_W + 3]);

  enum
  {
    GUARD_SIZE = 32
  };
  const u32 guard_color = 0xDEADBEEF;
  u32 guarded[TEST_PIXELS + 2 * GUARD_SIZE];
  for (s32 i = 0; i < TEST_PIXELS + 2 * GUARD_SIZE; ++i)
    guarded[i] = guard_color;
  u32 *guarded_pixels = guarded + GUARD_SIZE;
  memset (guarded_pixels, 0, TEST_PIXELS * sizeof (*guarded_pixels));
  RendererPlex guarded_rp = { TEST_W, TEST_H, guarded_pixels };
  draw_circle_t (0, 0, 2, translucent, &guarded_rp);
  draw_circle_t (TEST_W - 1, TEST_H - 1, 2, translucent, &guarded_rp);
  for (s32 i = 0; i < GUARD_SIZE; ++i)
    {
      assert_equal (guard_color, guarded[i]);
      assert_equal (guard_color, guarded[GUARD_SIZE + TEST_PIXELS + i]);
    }

  retrieve_results ();
}

static void
renderer_test_transformed_shapes (void)
{
  start_test_suite ("Renderer geometry: rotated and oriented shapes");

  u32 pixels[TEST_PIXELS] = { 0 };
  RendererPlex rp = { TEST_W, TEST_H, pixels };
  const u32 color = 0xFF13579B;

  draw_rotated_rectangle (3, 3, 4, 2, 0.0f, color, &rp);
  assert_equal (color, pixels[3 * TEST_W + 3]);
  assert_equal (color, pixels[2 * TEST_W + 2]);
  assert_equal (0, pixels[0]);

  memset (pixels, 0, sizeof pixels);
  draw_rotated_rectangle (3, 3, 4, 2, 1.57079632679f, color, &rp);
  assert_equal (color, pixels[3 * TEST_W + 3]);
  assert_equal (color, pixels[2 * TEST_W + 3]);
  assert_equal (0, pixels[0]);

  memset (pixels, 0, sizeof pixels);
  draw_rotated_oriented_rectangle (1, 3, 5, 3, 2, color, &rp);

  assert_true (pixels[3 * TEST_W + 3] == color
               || pixels[2 * TEST_W + 3] == color
               || pixels[4 * TEST_W + 3] == color);

  memset (pixels, 0, sizeof pixels);
  draw_rotated_oriented_rectangle (3, 3, 3, 3, 2, color, &rp);
  for (s32 i = 0; i < TEST_PIXELS; ++i)
    assert_equal (0, pixels[i]);

  retrieve_results ();
}

static void
renderer_test_transparency (void)
{
  u32 pixels[TEST_PIXELS], wide_pixels[17], expected[17];
  u32 triangle_pixels[17 * 17], reference_pixels[17 * 17];
  RendererPlex rp = { TEST_W, TEST_H, pixels }, wide_rp = { 17, 1, wide_pixels };
  RendererPlex triangle_rp = { 17, 17, triangle_pixels },
      reference_rp = { 17, 17, reference_pixels };
  const s32 triangles[4][6] = { { 1, 1, 15, 3, 4, 15 },
                                { 15, 3, 1, 1, 4, 15 },
                                { -4, 1, 12, 3, 2, 16 },
                                { 2, 2, 10, 10, 16, 16 } };
  s32 i, triangle, pixel;

  start_test_suite ("Renderer transparency: alpha and compositing");

  for (i = 0; i < TEST_PIXELS; ++i)
    pixels[i] = 0xFF204060;

  draw_rectangle_t (1, 1, 3, 3, 0x00112233, &rp);
  assert_equal (0xFF204060, pixels[2 * TEST_W + 2]);

  draw_rectangle_t (1, 1, 3, 3, 0xFFFF0000, &rp);
  assert_equal (0xFFFF0000, pixels[2 * TEST_W + 2]);
  assert_equal (0xFF204060, pixels[0]);

  for (i = 0; i < TEST_PIXELS; ++i)
    pixels[i] = 0xFF0000FF;

  draw_rectangle_t (1, 1, 3, 3, 0x80FF0000, &rp);

  assert_equal (0x8080007F, pixels[2 * TEST_W + 2]);
  assert_equal (0xFF0000FF, pixels[0]);

  for (i = 0; i < 17; ++i)
    {
      wide_pixels[i] = 0xFF000000 | (u32)i * 0x00010101;
      expected[i] = blend_rgba_pixel (0x8090A0B0, wide_pixels[i]);
    }

  draw_rectangle_t (0, 0, 16, 0, 0x8090A0B0, &wide_rp);

  for (i = 0; i < 17; ++i)
    assert_equal (expected[i], wide_pixels[i]);

  draw_hline (0, 16, 0, 0xFFABCDEF, &wide_rp);
  for (i = 0; i < 17; ++i)
    assert_equal (0xFFABCDEF, wide_pixels[i]);

  for (triangle = 0; triangle < 4; ++triangle)
    {
      for (pixel = 0; pixel < 17 * 17; ++pixel)
        triangle_pixels[pixel] = reference_pixels[pixel]
            = 0xFF000000 | ((u32)pixel * 0x00010101 & 0x00FFFFFF);

      renderer_reference_triangle_t (
          triangles[triangle][0], triangles[triangle][1],
          triangles[triangle][2], triangles[triangle][3],
          triangles[triangle][4], triangles[triangle][5], 0x7780C040, true,
          &reference_rp);
      draw_triangle_t (triangles[triangle][0], triangles[triangle][1],
                       triangles[triangle][2], triangles[triangle][3],
                       triangles[triangle][4], triangles[triangle][5],
                       0x7780C040, &triangle_rp);

      for (pixel = 0; pixel < 17 * 17; ++pixel)
        assert_equal (reference_pixels[pixel], triangle_pixels[pixel]);
    }

  for (pixel = 0; pixel < 17 * 17; ++pixel)
    triangle_pixels[pixel] = reference_pixels[pixel]
        = 0xFF000000 | ((u32)pixel * 0x00010101 & 0x00FFFFFF);

  renderer_reference_triangle_t (1, 1, 15, 3, 4, 15, 0xFF80C040, false,
                                 &reference_rp);
  draw_triangle (1, 1, 15, 3, 4, 15, 0xFF80C040, &triangle_rp);
  for (pixel = 0; pixel < 17 * 17; ++pixel)
    assert_equal (reference_pixels[pixel], triangle_pixels[pixel]);

  retrieve_results ();
}

void
renderer_color_tests (void)
{
  renderer_test_colors ();
}

void
renderer_raster_tests (void)
{
  renderer_test_lines_and_rectangles ();
  renderer_test_triangles ();
  renderer_test_circles ();
}

void
renderer_geometry_tests (void)
{
  renderer_test_transformed_shapes ();
}

void
renderer_transparency_tests (void)
{
  renderer_test_transparency ();
}

void
renderer_all_tests (void)
{
  start_group ("Renderer Color Tests");
  renderer_color_tests ();
  end_group ();

  start_group ("Renderer Rasterization Tests");
  renderer_raster_tests ();
  end_group ();

  start_group ("Renderer Geometry Tests");
  renderer_geometry_tests ();
  end_group ();

  start_group ("Renderer Transparency Tests");
  renderer_transparency_tests ();
  end_group ();
}

// Just for good practice
#undef TEST_W
#undef TEST_H
#undef TEST_PIXELS
