#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "decode.h"
#include "input.h"
#include "platform.h"
#include "renderer.h"

int
main (int argc, char **argv)
{
  // Argument parsing for the video to inspect
  if (argc <= 1)
    {
      fprintf (stderr,
               "Please provide a path to the video file to inspect.\n");
      return EXIT_FAILURE;
    }

  char *video_file = argv[1];

  int file_exists = platform_file_exists (video_file);
  if (file_exists == 0)
    printf ("Video to inspect: %s.\n", video_file);
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

  VideoPlex *vp = init_video (video_file);
  // init_video prints the error
  if (vp == NULL)
    return EXIT_FAILURE;

  int w = vp->frame->width, h = vp->frame->height;

  unsigned *image = malloc ((size_t)w * h * sizeof (unsigned));
  if (!image)
    {
      fprintf (stderr, "Could not allocate image.\n");
      return EXIT_FAILURE;
    }

  // decode_next_frame prints the error
  if (decode_next_frame (vp, image) != 0)
    return EXIT_FAILURE;

  // Initialize the renderer and platform
  RendererPlex *rp = init_renderer (w, h);
  if (!rp)
    return EXIT_FAILURE;

  PlatformState platform_state = { 0 };
  platform_init (&platform_state, "Inspector", 0, 0, w, h,
                 (char *)rp->image_buffer);

  // Main app loop
  while (platform_update (&platform_state))
    {
      if (input_is_key_pressed (ESC))
        platform_stop (&platform_state);

      // move this to the renderer
      memcpy (rp->image_buffer, image,
              (size_t)w * h * sizeof (*rp->image_buffer));

      renderer_present (&platform_state, rp);
    }

  platform_stop (&platform_state);

  return EXIT_SUCCESS;
}
