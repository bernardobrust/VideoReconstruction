// General platform interface

#pragma once

#include "dyn_arr.h"
#include "macros.h"

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

bool platform_init (PlatformState *platform_state, const byte *window_name,
                    s32 x, s32 y, s32 w, s32 h, byte *image_buffer);
void platform_shutdown (PlatformState *platform_state);

bool platform_update (PlatformState *platform_state);
void platform_dispatch_events (void);
void platform_present (PlatformState *platform_state);

f64 platform_get_time (void);
void platform_sleep (f64 ms);
s32 platform_file_exists (byte *filepath);

void platform_stop (PlatformState *platform_state);