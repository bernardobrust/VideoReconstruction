#include "platform.h"
#include "renderer.h"

int
main ()
{
  // This is likely running at a few thoused FPS and there is no way to close
  // it, but it works

  RendererPlex rp = init_renderer (680, 560);

  PlatformState platform_state = { 0 };

  platform_init (&platform_state, "Inspector", 0, 0, rp.w, rp.h,
                 (char *)rp.image_buffer);

  while (platform_update (&platform_state))
    {
      for (int i = 100; i < 300; ++i)
        {
          draw_hline (100, 400, i, rgba (255, 0, 0, 255), rp);
        }

      renderer_present (&platform_state, rp);
    }

  platform_stop (&platform_state);

  return 0;
}
