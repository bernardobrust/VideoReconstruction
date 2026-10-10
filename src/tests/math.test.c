#define ASTF_STRIP_PREFIX
#include "math.test.h"
#include "astf.h"
#include "basic.h"

static void
math_test_min_max (void)
{
  start_test_suite ("Math macros: min and max");

  assert_equal (3, MIN (3, 5));
  assert_equal (-3, MIN (-3, 5));
  assert_equal (-2, MIN (0, -2));
  assert_equal (5, MAX (3, 5));
  assert_equal (0, MAX (0, -1));
  assert_equal (2, MIN3 (2, 4, 6));
  assert_equal (-4, MIN3 (2, -4, 0));
  assert_equal (6, MAX3 (2, 4, 6));
  assert_equal (4, MAX3 (0, 4, -6));

  retrieve_results ();
}

static void
math_test_bit_and_string_macros (void)
{
  start_test_suite ("Math macros: alignment, strings, and division");

  assert_equal (0, ROUNDUP_4 (0));
  assert_equal (4, ROUNDUP_4 (1));
  assert_equal (4, ROUNDUP_4 (4));
  assert_equal (8, ROUNDUP_4 (5));
  assert_equal (16, ROUNDUP_4 (15));
  assert_equal (4, CSTRING_LEN ("test"));
  assert_equal (0, CSTRING_LEN (""));
  assert_equal (0, DIV_255 (0));
  assert_equal (1, DIV_255 (255));
  assert_equal (127, DIV_255 (127 * 255));
  assert_equal (255, DIV_255 (255 * 255));

  retrieve_results ();
}

static void
math_test_clamp (void)
{
  start_test_suite ("Math functions: clamp boundaries");

  assert_equal (-2, clamp_s32 (-5, -2, 4));
  assert_equal (-2, clamp_s32 (-2, -2, 4));
  assert_equal (4, clamp_s32 (4, -2, 4));
  assert_equal (4, clamp_s32 (9, -2, 4));
  assert_equal (2, clamp_s32 (2, -2, 4));
  assert_equal (7, clamp_s32 (0, 7, 7));
  assert_approx (0.0, clamp_f32 (-1.0f, 0.0f, 5.0f), 1e-6);
  assert_approx (0.0, clamp_f32 (0.0f, 0.0f, 5.0f), 1e-6);
  assert_approx (5.0, clamp_f32 (5.0f, 0.0f, 5.0f), 1e-6);
  assert_approx (5.0, clamp_f32 (9.0f, 0.0f, 5.0f), 1e-6);
  assert_approx (3.0, clamp_f32 (3.0f, 3.0f, 3.0f), 1e-6);

  retrieve_results ();
}

static void
math_test_hypot (void)
{
  start_test_suite ("Math functions: approximate hypotenuse");

  assert_approx (0.0, hypot_f32 (0.0f, 0.0f), 1e-6);
  assert_approx (5.0, hypot_f32 (3.0f, 4.0f), 0.3);
  assert_approx (5.0, hypot_f32 (4.0f, 3.0f), 0.3);
  assert_approx (5.0, hypot_f32 (-3.0f, 4.0f), 0.3);
  assert_approx (5.0, hypot_f32 (3.0f, -4.0f), 0.3);
  assert_approx (10.0, hypot_f32 (10.0f, 0.0f), 1e-6);
  assert_approx ((f64)hypot_f32 (3.0f, 4.0f), (f64)hypot_f32 (4.0f, 3.0f),
                 1e-6);
  assert_true (hypot_f32 (3.0f, 4.0f) > 4.7f);
  assert_true (hypot_f32 (3.0f, 4.0f) < 5.3f);

  retrieve_results ();
}

static void
math_test_determinant (void)
{
  start_test_suite ("Math geometry: oriented determinant");

  assert_equal (-2, determinant_ab_ap_s32 (0, 0, 2, 0, 0, 1));
  assert_equal (2, determinant_ab_ap_s32 (0, 0, 2, 0, 0, -1));
  assert_equal (0, determinant_ab_ap_s32 (0, 0, 2, 0, 1, 0));
  assert_equal (2, determinant_ab_ap_s32 (2, 0, 0, 0, 0, 1));
  assert_equal (1, determinant_ab_ap_s32 (10, 20, 12, 21, 11, 20));
  assert_equal (0, determinant_ab_ap_s32 (3, -4, 3, -4, 9, 10));
  assert_equal (0, determinant_ab_ap_s32 (1, 1, 3, 3, 5, 5));

  retrieve_results ();
}

void
math_macro_tests (void)
{
  math_test_min_max ();
  math_test_bit_and_string_macros ();
}

void
math_function_tests (void)
{
  math_test_clamp ();
  math_test_hypot ();
}

void
math_geometry_tests (void)
{
  math_test_determinant ();
}

void
math_all_tests (void)
{
  math_macro_tests ();
  math_function_tests ();
  math_geometry_tests ();
}
