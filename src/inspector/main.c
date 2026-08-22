#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input.h"
#include "platform.h"
#include "renderer.h"

int
main (int argc, char **argv)
{
  // Argument parsing for the video to inspect
  if (argc <= 1)
    {
      fprintf (stderr, "Please provide a path to the video file to inspect.\n");
      return EXIT_FAILURE;
    }

  char *video_file = argv[1];

  int file_exists = platform_file_exists (video_file);
  if (file_exists == 0)
    printf ("Video to inspect: %s\n", video_file);
  else if (file_exists == 1)
    {
      fprintf (stderr, "File does not exist. Is the path correct?\n");
      return EXIT_FAILURE;
    }
  else
    {
      fprintf (stderr,
               "Could not access path. Do you have permission to open it?\n");
      return EXIT_FAILURE;
    }

  // Initialize the renderer and platform
  RendererPlex rp = init_renderer (680, 560);

  PlatformState platform_state = { 0 };
  platform_init (&platform_state, "Inspector", 0, 0, rp.w, rp.h,
                 (char *)rp.image_buffer);

  // Main app loop
  float theta = 0.0f;
  while (platform_update (&platform_state))
    {
      if (input_is_key_pressed (ESC))
        platform_stop (&platform_state);

      // Frame-dependent for now
      theta += 0.001f;

      draw_triangle (20, 20, 100, 100, 100, 300, rgba (255, 0, 0, 255), rp);
      draw_rectangle (500, 300, 200, 200, rgba (0, 255, 0, 255), rp);
      draw_circle (600, 500, 50, rgba (0, 0, 255, 255), rp);
      draw_rotated_rectangle (200, 200, 80, 50, theta, rgba (155, 100, 0, 255),
                              rp);
      draw_rotated_oriented_rectangle (300, 300, 400, 400, 80,
                                       rgba (155, 100, 0, 255), rp);
      draw_arrow (400, 400, 600, 200, 20, rgba (255, 0, 0, 255), rp);

      renderer_present (&platform_state, rp);
    }

  platform_stop (&platform_state);

  return 0;
}
