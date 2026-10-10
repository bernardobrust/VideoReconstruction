#include "input.h"

global InputPlex input_state = { 0 };

void
input_set_key_pressed (KeyValue k)
{
  if (k == CTRL)
    input_state.ctrl_mod = true;
  else if (k == SHIFT)
    input_state.shift_mod = true;
  else if (k >= 0 && k < 10)
    {
      if (!input_state.keys_pressed[k])
        input_state.keys_just_pressed[k] = true;
      input_state.keys_pressed[k] = 1;
    }
}

void
input_set_key_released (KeyValue k)
{
  if (k == CTRL)
    input_state.ctrl_mod = false;
  else if (k == SHIFT)
    input_state.shift_mod = false;
  else if (k >= 0 && k < 10)
    input_state.keys_pressed[k] = 0;
}

bool
input_is_key_pressed (KeyValue k)
{
  if (k == CTRL)
    return input_state.ctrl_mod;
  if (k == SHIFT)
    return input_state.shift_mod;
  if (k >= 0 && k < 10)
    return input_state.keys_pressed[k] != 0;
  return false;
}

bool
input_is_key_just_pressed (KeyValue k)
{
  if (k < 0 || k >= 10)
    return false;

  bool just_pressed = input_state.keys_just_pressed[k];
  input_state.keys_just_pressed[k] = false;
  return just_pressed;
}

bool
input_is_mouse_button_pressed (MouseButton button)
{
  if (button < 0 || button >= 2)
    return false;
  return input_state.mouse_buttons_pressed[button];
}

bool
input_is_mouse_button_just_pressed (MouseButton button)
{
  if (button < 0 || button >= 2)
    return false;

  bool just_pressed = input_state.mouse_buttons_just_pressed[button];
  input_state.mouse_buttons_just_pressed[button] = false;
  return just_pressed;
}

bool
input_is_mouse_button_just_released (MouseButton button)
{
  if (button < 0 || button >= 2)
    return false;

  bool just_released = input_state.mouse_buttons_just_released[button];
  input_state.mouse_buttons_just_released[button] = false;
  return just_released;
}

void
input_get_mouse_pos (s32 *x, s32 *y)
{
  if (x)
    *x = input_state.mouse_x;
  if (y)
    *y = input_state.mouse_y;
}

void
input_set_mouse_pos (s32 x, s32 y)
{
  input_state.mouse_x = x;
  input_state.mouse_y = y;
}

void
input_set_mouse_button_pressed (MouseButton button)
{
  if (button >= 0 && button < 2)
    {
      if (!input_state.mouse_buttons_pressed[button])
        input_state.mouse_buttons_just_pressed[button] = true;
      input_state.mouse_buttons_pressed[button] = true;
    }
}

void
input_set_mouse_button_released (MouseButton button)
{
  if (button >= 0 && button < 2)
    {
      if (input_state.mouse_buttons_pressed[button])
        input_state.mouse_buttons_just_released[button] = true;
      input_state.mouse_buttons_pressed[button] = false;
    }
}

bool
input_is_command_pressed (Command c)
{
  switch (c)
    {
    case CTRL_P:
      return input_state.ctrl_mod && input_state.keys_pressed[P];
    case CTRL_SHIFT_P:
      return input_state.ctrl_mod && input_state.shift_mod
             && input_state.keys_pressed[P];
    default:
      return false;
    }
}
