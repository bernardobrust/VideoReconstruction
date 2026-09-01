#include "platform.h"

// Placeholder definitions
bool platform_init (PlatformState *platform_state, const char *window_name, int x,
               int y, int w, int h, char *image_buffer)
{
  return false;
}

void platform_shutdown(PlatformState* platform_state)
{
  return;
}

bool platform_update(PlatformState* platform_state)
{
  return false;
}

void platform_present(PlatformState* platform_state)
{
  return;
}

double platform_get_time(void)
{
  return 0.0f;
}

void platform_sleep(double ms)
{
  return;
}

int
platform_file_exists (char *filepath)
{
  return 0;
}

void platform_stop(PlatformState* platform_state)
{
  return;
}