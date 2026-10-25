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
  s32 result = EXIT_FAILURE;

  // Argument parsing for the video to inspect
  if (argc <= 1)
    {
      fprintf (stderr,
               "Please provide a path to the video file to inspect.\n");

      return result;
    }

  byte *video_file = argv[1];
  s32 file_exists = platform_file_exists (video_file);
  if (file_exists == 0)
    {
#ifndef NDEBUG
      printf ("Video to inspect: %s.\n", video_file);
#endif
    }
  else if (file_exists == 1)
    {
      fprintf (stderr, "File does not exist. Is the path correct?\n");

      return result;
    }
  else
    {
      fprintf (stderr,
               "Could not access path. Do you have permission to open it?\n");

      return result;
    }

  Arena video_arena = { 0 }, renderer_arena = { 0 };
  PlatformState platform_state = { 0 };
  VideoPlex *vp = NULL;
  arena_init (&video_arena, sizeof (VideoPlex) + _Alignof (VideoPlex));

  // Initializing video
  vp = init_video (&video_arena, video_file);
  if (!vp)
    goto end;

  s32 w = vp->codec->width, h = vp->codec->height;
  u64 renderer_capacity = operations_memory_required (w, h);
  arena_init (&renderer_arena, renderer_capacity);

  // Renderer and inspector buffers share one arena and lifetime.
  RendererPlex *rp = init_renderer (&renderer_arena, w, h);
  operations_init (rp, &renderer_arena);

  platform_init (&platform_state, "Inspector", 0, 0, rp->w, rp->h,
                 (byte *)rp->image_buffer);
  if (platform_state.internal_state == NULL)
    goto end;
  decode_set_display_resolution (
      platform_get_screen_resolution (&platform_state));

  // decode_next_frame prints the error
  if (decode_next_frame (vp, rp) != 0)
    goto end;
  operations_on_frame_decoded (rp);

  // Stable framerate at video FPS, we'll have a lot of work latter (?) to fix
  // the fps of the UI
  AVRational video_fps = av_guess_frame_rate (vp->fmt, vp->stream, NULL);
  f64 frame_duration_ms = 1000.0 * (f64)video_fps.den / (f64)video_fps.num,
      next_frame = platform_get_time (), now, remaining;

  result = EXIT_SUCCESS;
  while (platform_update (&platform_state))
    {
      process_input (&platform_state);

      if (!paused)
        {
          now = platform_get_time ();
          remaining = next_frame - now;
          if (remaining > 0)
            platform_sleep (remaining);

          s32 ret = decode_next_frame (vp, rp);
          if (ret < 0)
            {
              result = EXIT_FAILURE;
              break;
            }
          if (ret == 1)
            break;

          operations_apply_zoom (rp);
          renderer_present (&platform_state, rp);
          next_frame
              = platform_get_time () + frame_duration_ms / playback_speed;
        }
      else
        {
          // Temp
          operations_render_paused (&platform_state, rp);
          platform_sleep (10.0);
        }
    }

end:
  return result;
}
