// General platform interface

#pragma once

#include <stdbool.h>

typedef struct
{
  void *internal_state;
  bool running;
} PlatformState;

bool platform_init (PlatformState *platform_state, const char *window_name,
                    int x, int y, int w, int h, char *image_buffer);

void platform_shutdown (PlatformState *platform_state);

bool platform_update (PlatformState *platform_state);

void platform_stop (PlatformState *platform_state);

void platform_present (PlatformState *platform_state);

double platform_get_time ();

void platform_sleep (double ms);
