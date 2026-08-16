#pragma once

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
} KeyValue;

// Just these ones for now
typedef enum
{
    CTRL_P,
    CTRL_SHIFT_P,
} Command;

typedef struct {
    int keys_pressed[5];
    bool ctrl_mod, shift_mod;
} InputPlex;

bool input_is_command_pressed(Command c);

void input_set_key_pressed(KeyValue k);
void input_set_key_released(KeyValue k);

