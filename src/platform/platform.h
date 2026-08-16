// General platform interface

#pragma once

#include "dyn_arr.h"

#include <stdbool.h>

typedef struct
{
  void *internal_state;
  bool running;
} PlatformState;

typedef enum
{
  KeyCtrlPress,
  KeyCtrlRelease,
  KeyShiftPress,
  KeyShiftRelease,
  KeyEscPress,
  KeyEscRelease,
  KeyOnePress,
  KeyOneRelease,
  KeyTwoPress,
  KeyTwoRelease,
  KeyThreePress,
  KeyThreeRelease,
  KeyPPress,
  KeyPRelease,
} EventType;

// platform_update will push the events onto the queue, from where
// platform_dispatch_events will call the associated function (such as
// input_set_key_pressed)
extern DynArr event_queue;

bool platform_init (PlatformState *platform_state, const char *window_name,
                    int x, int y, int w, int h, char *image_buffer);

void platform_shutdown (PlatformState *platform_state);

bool platform_update (PlatformState *platform_state);

void platform_dispatch_events (PlatformState *platform_state);

void platform_stop (PlatformState *platform_state);

void platform_present (PlatformState *platform_state);

double platform_get_time ();

void platform_sleep (double ms);
