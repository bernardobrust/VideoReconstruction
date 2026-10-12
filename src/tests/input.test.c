#define ASTF_STRIP_PREFIX
#include "input.h"
#include "astf.h"
#include "input.test.h"
#include "platform.h"

void
input_all_tests (void)
{
  start_test_suite ("Input state");

  input_set_key_pressed (ONE);
  assert_true (input_is_key_pressed (ONE));
  assert_true (input_is_key_just_pressed (ONE));
  // Important test!
  assert_false (input_is_key_just_pressed (ONE));

  // Important test!
  input_set_key_pressed (ONE);
  assert_false (input_is_key_just_pressed (ONE));

  input_set_key_released (ONE);
  assert_false (input_is_key_pressed (ONE));

  // Coumpound
  input_set_key_pressed (CTRL);
  input_set_key_pressed (P);
  assert_true (input_is_command_pressed (CTRL_P));

  input_set_key_pressed (SHIFT);
  assert_true (input_is_command_pressed (CTRL_SHIFT_P));
  input_set_key_released (CTRL);
  assert_false (input_is_command_pressed (CTRL_SHIFT_P));
  input_set_key_released (SHIFT);

  input_set_mouse_button_pressed (MOUSE_LEFT);
  assert_true (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_pressed (MOUSE_LEFT));
  assert_false (input_is_mouse_button_just_pressed (MOUSE_LEFT));

  input_set_mouse_button_released (MOUSE_LEFT);
  assert_false (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_released (MOUSE_LEFT));

  // Mouse movement
  input_set_mouse_pos (12, -7);
  s32 x = 0, y = 0;
  input_get_mouse_pos (&x, &y);
  assert_equal (12, x);
  assert_equal (-7, y);

  // Platform event dispatcher mouse tracking
  event_queue = *dyn_arr_init (16, sizeof (PlatformEvent));
  platform_event_push (MouseMove, 42, 99);
  platform_dispatch_events ();
  input_get_mouse_pos (&x, &y);
  assert_equal (42, x);
  assert_equal (99, y);

  platform_event_push (MouseLeftPress, 100, 200);
  platform_dispatch_events ();
  input_get_mouse_pos (&x, &y);
  assert_equal (100, x);
  assert_equal (200, y);
  assert_true (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_pressed (MOUSE_LEFT));

  platform_event_push (MouseLeftRelease, 150, 250);
  platform_dispatch_events ();
  input_get_mouse_pos (&x, &y);

  assert_equal (150, x);
  assert_equal (250, y);
  assert_false (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_released (MOUSE_LEFT));

  retrieve_results ();
}
