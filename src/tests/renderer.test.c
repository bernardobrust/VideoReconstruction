#define ASTF_STRIP_PREFIX
#include "renderer.h"
#include "astf.h"
#include "renderer.test.h"

#include <string.h>

#define TEST_W 7
#define TEST_H 7
#define TEST_PIXELS (TEST_W * TEST_H)

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
  start_test_suite ("Renderer transparency: alpha and compositing");

  u32 pixels[TEST_PIXELS];
  for (s32 i = 0; i < TEST_PIXELS; ++i)
    pixels[i] = 0xFF204060;
  RendererPlex rp = { TEST_W, TEST_H, pixels };

  draw_rectangle_t (1, 1, 3, 3, 0x00112233, &rp);
  assert_equal (0xFF204060, pixels[2 * TEST_W + 2]);

  draw_rectangle_t (1, 1, 3, 3, 0xFFFF0000, &rp);
  assert_equal (0xFFFF0000, pixels[2 * TEST_W + 2]);

  for (s32 i = 0; i < TEST_PIXELS; ++i)
    pixels[i] = 0xFF0000FF;

  draw_rectangle_t (1, 1, 3, 3, 0x80FF0000, &rp);

  assert_equal (0x8080007F, pixels[2 * TEST_W + 2]);
  assert_equal (0xFF0000FF, pixels[0]);

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
  renderer_color_tests ();
  renderer_raster_tests ();
  renderer_geometry_tests ();
  renderer_transparency_tests ();
}

// Just for good practice
#undef TEST_W
#undef TEST_H
#undef TEST_PIXELS