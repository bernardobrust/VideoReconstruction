#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "decode.h"
#include "input.h"
#include "macros.h"
#include "platform.h"
#include "renderer.h"

s32
main (s32 argc, char **argv)
{
  // Argument parsing for the video to inspect
  if (argc <= 1)
    {
      fprintf (stderr,
               "Please provide a path to the video file to inspect.\n");
      return EXIT_FAILURE;
    }

  char *video_file = argv[1];

  s32 file_exists = platform_file_exists (video_file);
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

  // Initializing video
  VideoPlex *vp = init_video (video_file);
  // init_video prints the error
  if (!vp)
    return EXIT_FAILURE;

  s32 w = vp->codec->width, h = vp->codec->height;

  // Initialize the renderer
  RendererPlex *rp = init_renderer (w, h);
  if (!rp)
    return EXIT_FAILURE;

  // decode_next_frame prints the error
  if (decode_next_frame (vp, rp->image_buffer) != 0)
    return EXIT_FAILURE;

  // Initialize platform
  PlatformState platform_state = { 0 };
  platform_init (&platform_state, "Inspector", 0, 0, rp->w, rp->h,
                 (char *)rp->image_buffer);

  // Stable framerate at video FPS, we'll have a lot of work latter (?) to fix
  // the fps of the UI
  AVRational fps = av_guess_frame_rate (vp->fmt, vp->stream, NULL);
  f64 frame_time_ms = 1000.0 * fps.den / fps.num,
      next_frame = platform_get_time (), now, remaining;

  while (platform_update (&platform_state))
    {
      if (input_is_key_pressed (ESC))
        platform_stop (&platform_state);

      s32 ret = decode_next_frame (vp, rp->image_buffer);
      if (ret < 0)
        return EXIT_FAILURE;
      if (ret == 1)
        return EXIT_SUCCESS;

      // Trying out transparent shapes
      draw_triangle_t (0, 0, 500, 500, 700, 200, rgba (255, 0, 0, 100), rp);
      draw_circle_t (200, 200, 100, rgba (255, 255, 0, 128), rp);
      draw_rectangle_t (500, 500, 600, 600, rgba (0, 0, 255, 128), rp);

      draw_arrow_t (300, 300, 400, 400, 8, rgba (0, 255, 0, 128), rp);
      draw_arrow_t (100, 1200, 500, 400, 12, rgba (120, 120, 200, 180), rp);

      renderer_present (&platform_state, rp);

      next_frame += frame_time_ms;
      now = platform_get_time ();
      remaining = next_frame - now;

      // If this is negative we are actually delayed
      if (remaining > 0)
        platform_sleep (remaining);
    }

  return EXIT_SUCCESS;
}
