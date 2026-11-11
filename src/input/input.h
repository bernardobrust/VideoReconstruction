#pragma once

#include "macros.h"

#include <stdbool.h>

// We'll add keys here as we need them
typedef enum
{
  CTRL = 100,
  SHIFT = 101,
  ESC = 0,
  ONE = 1,
  TWO = 2,
  THREE = 3,
  P = 4,
  LEFT = 5,
  RIGHT = 6,
  UP = 7,
  DOWN = 8,
  SPACE = 9,
  F = 10,
  T = 11,
} KeyValue;

typedef enum
{
  MOUSE_LEFT,
  MOUSE_RIGHT
} MouseButton;

// Just these ones for now (we don't even use them, just for testing)
typedef enum
{
  CTRL_P,
  CTRL_SHIFT_P,
} Command;

typedef struct
{
  s32 keys_pressed[12];
  bool keys_just_pressed[12];
  bool mouse_buttons_pressed[2];
  bool mouse_buttons_just_pressed[2];
  bool mouse_buttons_just_released[2];
  s32 mouse_x, mouse_y;
  bool ctrl_mod, shift_mod;
} InputPlex;

bool input_is_command_pressed (Command c);
bool input_is_key_pressed (KeyValue k);
bool input_is_key_just_pressed (KeyValue k);
bool input_is_mouse_button_pressed (MouseButton button);
bool input_is_mouse_button_just_pressed (MouseButton button);
bool input_is_mouse_button_just_released (MouseButton button);

void input_get_mouse_pos (s32 *x, s32 *y);
void input_set_mouse_pos (s32 x, s32 y);

void input_set_key_pressed (KeyValue k);
void input_set_key_released (KeyValue k);
void input_set_mouse_button_pressed (MouseButton button);
void input_set_mouse_button_released (MouseButton button);
