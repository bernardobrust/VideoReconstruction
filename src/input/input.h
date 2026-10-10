#pragma once

#include "macros.h"

#include <stdbool.h>

// We'll just add here some keys we'll likely need latter
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
} KeyValue;

typedef enum
{
  MOUSE_LEFT,
  MOUSE_RIGHT
} MouseButton;

// Just these ones for now
typedef enum
{
  CTRL_P,
  CTRL_SHIFT_P,
} Command;

typedef struct
{
  s32 keys_pressed[10];
  bool keys_just_pressed[10];
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
