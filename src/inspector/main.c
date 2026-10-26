#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "basic.h"
#include "decode.h"
#include "frame_queue.h"
#include "input.h"
#include "macros.h"
#include "operations.h"
#include "platform.h"
#include "playback.h"
#include "renderer.h"
#include "renderer_pool.h"

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
  InspectorPlayback playback = { 0 };
  VideoPlex *vp = NULL;
  arena_init (&video_arena, sizeof (VideoPlex) + _Alignof (VideoPlex));

  // We'll require a lot of profiling to adjust these values
  s32 logical_cores = platform_get_logical_core_count (),
      worker_budget = MAX (0, logical_cores - 3),
      decoder_threads = MAX (1, worker_budget * 3 / 4);

  // Initializing video
  vp = init_video (&video_arena, video_file, decoder_threads);
  if (!vp)
    goto end;

  s32 w = vp->codec->width, h = vp->codec->height;

  enum
  {
    // We'll later get this from a config file
    DECODED_FRAME_CACHE_CAPACITY = 12
  };

  // Account for the main, decoder-coordinator, and systems threads first.
  // The remaining budget is split between dav1d and renderer helper threads,
  // the systems thread also renders one row range while the pool is active.
  s32 renderer_workers = MAX (0, worker_budget - decoder_threads);
  u64 renderer_capacity
      = operations_memory_required (w, h)
        + frame_queue_memory_required (w, h, DECODED_FRAME_CACHE_CAPACITY)
        + (u64)w * h * sizeof (u32)
        + renderer_pool_memory_required (renderer_workers);
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

  // Keep the video's temporal coherence by default, playback speed scales it.
  AVRational video_fps = av_guess_frame_rate (vp->fmt, vp->stream, NULL);
  f64 frame_duration_ms
      = video_fps.num > 0 && video_fps.den > 0
            ? 1000.0 * (f64)video_fps.den / (f64)video_fps.num
            : 1000.0 / 30.0;

  if (!inspector_playback_init (&playback, &renderer_arena, vp, rp,
                                DECODED_FRAME_CACHE_CAPACITY, renderer_workers,
                                frame_duration_ms))
    goto end;

  result = EXIT_SUCCESS;
  while (platform_update (&platform_state))
    {
      inspector_playback_process_input (&playback, &platform_state);

      if (!platform_state.running)
        break;
      if (!inspector_playback_update (&playback, &platform_state))
        break;

      platform_sleep (1.0);
    }

  if (inspector_playback_result (&playback) < 0)
    result = EXIT_FAILURE;

end:
  return result;
}
