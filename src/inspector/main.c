#include "platform.h"

int
main ()
{
  // This is likely running at a few thoused FPS and there is no way to close
  // it, but it works
  PlatformState platform_state = { 0 };
  unsigned *image_buffer[680 * 460] = { 0 };

  platform_init (&platform_state, "Inspector", 0, 0, 680, 460,
                 (char *)image_buffer);

  while (platform_update (&platform_state))
    {
      platform_present (&platform_state);
    }

  platform_stop (&platform_state);

  return 0;
}
