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

#include "operations.c"

s32
main (s32 argc, byte **argv)
{
  // Argument parsing for the video to inspect
  if (argc <= 1)
    {
      fprintf (stderr,
               "Please provide a path to the video file to inspect.\n");

      return EXIT_FAILURE;
    }

  byte *video_file = argv[1];
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
  if (decode_next_frame (vp, rp) != 0)
    return EXIT_FAILURE;

  // Initialize platform
  PlatformState platform_state = { 0 };
  platform_init (&platform_state, "Inspector", 0, 0, rp->w, rp->h,
                 (byte *)rp->image_buffer);

  // Stable framerate at video FPS, we'll have a lot of work latter (?) to fix
  // the fps of the UI
  AVRational video_fps = av_guess_frame_rate (vp->fmt, vp->stream, NULL);

  f64 frame_duration_ms = 1000.0 * (f64)video_fps.den / (f64)video_fps.num,
      next_frame = platform_get_time (), now, remaining;

  while (platform_update (&platform_state))
    {
      process_input (&platform_state);

      // Decode exactly one frame per scheduled traversal interval. Changing
      // speed only changes the interval, the decoder's current position stays
      // where it is.
      if (!paused)
        {
          now = platform_get_time ();
          remaining = next_frame - now;
          if (remaining > 0)
            platform_sleep (remaining);

          s32 ret = decode_next_frame (vp, rp);

          // Failed to decode
          if (ret < 0)
            return EXIT_FAILURE;

          // Video ended
          if (ret == 1)
            return EXIT_SUCCESS;

          renderer_present (&platform_state, rp);
          next_frame
              = platform_get_time () + frame_duration_ms / playback_speed;
        }
      else
        // FIxed in case we are not decoding, latter we'll keep the UI
        // framerate
        platform_sleep (10.0);
    }

  return EXIT_SUCCESS;
}
