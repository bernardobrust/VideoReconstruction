#include "input.h"
#include "platform.h"
#include "renderer.h"

int
main ()
{
  RendererPlex rp = init_renderer (680, 560);

  PlatformState platform_state = { 0 };

  platform_init (&platform_state, "Inspector", 0, 0, rp.w, rp.h,
                 (char *)rp.image_buffer);

  while (platform_update (&platform_state))
    {
      if (input_is_key_pressed (ESC))
        {
          platform_stop (&platform_state);
        }

      draw_triangle (20, 20, 100, 100, 100, 300, rgba (255, 0, 0, 255), rp);
      draw_rectangle (500, 300, 200, 200, rgba (0, 255, 0, 255), rp);
      draw_circle (600, 500, 50, rgba (0, 0, 255, 255), rp);

      renderer_present (&platform_state, rp);
    }

  platform_stop (&platform_state);

  return 0;
}
