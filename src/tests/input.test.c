#define ASTF_STRIP_PREFIX
#include "input.h"
#include "astf.h"
#include "input.test.h"
#include "platform.h"

static void
input_test_keys_and_commands (void)
{
  start_test_suite ("Input keys, modifiers, and commands");

  input_set_key_pressed (ONE);
  assert_true (input_is_key_pressed (ONE));
  assert_true (input_is_key_just_pressed (ONE));
  assert_false (input_is_key_just_pressed (ONE));

  input_set_key_pressed (ONE);
  assert_true (input_is_key_pressed (ONE));
  assert_false (input_is_key_just_pressed (ONE));

  input_set_key_pressed (TWO);
  assert_true (input_is_key_pressed (ONE));
  assert_true (input_is_key_pressed (TWO));
  assert_true (input_is_key_just_pressed (TWO));

  input_set_key_released (ONE);
  assert_false (input_is_key_pressed (ONE));
  assert_true (input_is_key_pressed (TWO));
  input_set_key_released (TWO);

  input_set_key_pressed (CTRL);
  input_set_key_pressed (P);
  assert_true (input_is_command_pressed (CTRL_P));
  assert_false (input_is_command_pressed (CTRL_SHIFT_P));

  input_set_key_pressed (SHIFT);
  assert_true (input_is_command_pressed (CTRL_SHIFT_P));
  input_set_key_released (CTRL);
  assert_false (input_is_command_pressed (CTRL_P));
  assert_false (input_is_command_pressed (CTRL_SHIFT_P));
  input_set_key_released (SHIFT);
  input_set_key_released (P);

  assert_false (input_is_key_pressed ((KeyValue)-1));
  assert_false (input_is_key_just_pressed ((KeyValue)-1));
  assert_false (input_is_key_pressed ((KeyValue)11));
  assert_false (input_is_key_just_pressed ((KeyValue)11));

  retrieve_results ();
}

static void
input_test_mouse_buttons_and_position (void)
{
  start_test_suite ("Input mouse buttons and position");

  input_set_mouse_button_pressed (MOUSE_LEFT);
  assert_true (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_pressed (MOUSE_LEFT));
  assert_false (input_is_mouse_button_just_pressed (MOUSE_LEFT));

  input_set_mouse_button_pressed (MOUSE_LEFT);
  assert_true (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_false (input_is_mouse_button_just_pressed (MOUSE_LEFT));

  input_set_mouse_button_pressed (MOUSE_RIGHT);
  assert_true (input_is_mouse_button_pressed (MOUSE_RIGHT));
  assert_true (input_is_mouse_button_just_pressed (MOUSE_RIGHT));

  input_set_mouse_button_released (MOUSE_LEFT);
  assert_false (input_is_mouse_button_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_released (MOUSE_LEFT));
  assert_false (input_is_mouse_button_just_released (MOUSE_LEFT));
  input_set_mouse_button_released (MOUSE_LEFT);
  assert_false (input_is_mouse_button_just_released (MOUSE_LEFT));

  input_set_mouse_button_released (MOUSE_RIGHT);
  assert_true (input_is_mouse_button_just_released (MOUSE_RIGHT));

  input_set_mouse_button_pressed ((MouseButton)-1);
  input_set_mouse_button_pressed ((MouseButton)2);
  assert_false (input_is_mouse_button_pressed ((MouseButton)-1));
  assert_false (input_is_mouse_button_just_pressed ((MouseButton)2));
  assert_false (input_is_mouse_button_just_released ((MouseButton)-1));

  input_set_mouse_pos (12, -7);
  s32 x = 0, y = 0;
  input_get_mouse_pos (&x, &y);
  assert_equal (12, x);
  assert_equal (-7, y);
  input_get_mouse_pos (&x, NULL);
  assert_equal (12, x);
  input_get_mouse_pos (NULL, &y);
  assert_equal (-7, y);

  retrieve_results ();
}

static void
input_test_platform_event_dispatch (void)
{
  start_test_suite ("Input platform event dispatch");

  static PlatformEvent events[16];
  event_queue = dyn_arr_init_fixed (events, 16, sizeof (PlatformEvent));

  platform_event_push (KeyEscPress, 0, 0);
  platform_event_push (KeyOnePress, 0, 0);
  platform_event_push (MouseMove, 42, 99);
  platform_dispatch_events ();

  assert_true (input_is_key_pressed (ESC));
  assert_true (input_is_key_just_pressed (ESC));
  assert_true (input_is_key_pressed (ONE));
  assert_true (input_is_key_just_pressed (ONE));
  assert_equal (0, event_queue.len);

  s32 x = 0, y = 0;
  input_get_mouse_pos (&x, &y);
  assert_equal (42, x);
  assert_equal (99, y);

  platform_event_push (KeyEscRelease, 0, 0);
  platform_event_push (KeyOneRelease, 0, 0);
  platform_event_push (MouseLeftPress, 100, 200);
  platform_event_push (MouseLeftRelease, 150, 250);
  platform_dispatch_events ();

  assert_false (input_is_key_pressed (ESC));
  assert_false (input_is_key_pressed (ONE));
  assert_true (input_is_mouse_button_just_pressed (MOUSE_LEFT));
  assert_true (input_is_mouse_button_just_released (MOUSE_LEFT));
  assert_false (input_is_mouse_button_pressed (MOUSE_LEFT));

  input_get_mouse_pos (&x, &y);
  assert_equal (150, x);
  assert_equal (250, y);

  dyn_arr_free (&event_queue);

  retrieve_results ();
}

void
input_all_tests (void)
{
  start_group ("Input Tests");

  input_test_keys_and_commands ();
  input_test_mouse_buttons_and_position ();
  input_test_platform_event_dispatch ();

  end_group ();
}
