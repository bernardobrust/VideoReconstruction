#include "input.h"

static InputPlex input_state = { 0 };

void
input_set_key_pressed (KeyValue k)
{
  if (k == CTRL)
    input_state.ctrl_mod = true;
  else if (k == SHIFT)
    input_state.shift_mod = true;
  else if (k >= 0 && k < 5)
    input_state.keys_pressed[k] = 1;
}

void
input_set_key_released (KeyValue k)
{
  if (k == CTRL)
    input_state.ctrl_mod = false;
  else if (k == SHIFT)
    input_state.shift_mod = false;
  else if (k >= 0 && k < 5)
    input_state.keys_pressed[k] = 0;
}

bool
input_is_key_pressed (KeyValue k)
{
  if (k == CTRL)
    return input_state.ctrl_mod;
  if (k == SHIFT)
    return input_state.shift_mod;
  if (k >= 0 && k < 5)
    return input_state.keys_pressed[k] != 0;
  return false;
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