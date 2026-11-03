#include <stdio.h>
#include <stdlib.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#define FLAG_IMPLEMENTATION
#include "flag.h"

#include "basic.h"
#include "decode.h"
#include "export.h"
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

  // CLI parsing
  byte **inspect_video = flag_str ("inspect", "", "Video to inspect");
  Flag_List *export = flag_list ("export", "Where to export the data to");

  if (!flag_parse (argc, argv))
    {
      flag_print_error (stderr);
      return EXIT_FAILURE;
    }

  argc = flag_rest_argc ();
  argv = flag_rest_argv ();

  // No arguments
  if ((*inspect_video)[0] == '\0' && export->count == 0)
    {
      fprintf (stderr,
               "Use either '-export (video file) (data destination file)' or "
               "'-inspect (video file)'"
               "\n\t'-export': exports the video data to a "
               "file\n\t'-inspect' opens the video inspector on that file\n");
      return EXIT_FAILURE;
    }
  // Only export
  else if ((*inspect_video)[0] == '\0')
    {
      if (export->count != 2)
        {
          fprintf (stderr,
                   "Usage: -export (video file) (data destination file)\n");
          return EXIT_FAILURE;
        }

      return export_data_to (export->items[0], export->items[1])
                 ? EXIT_SUCCESS
                 : EXIT_FAILURE;
    }
  // Video only
  else if (export->count == 0)
    goto video_only;
  // We'll latter want to find a way to both export and inspect the video at
  // once
  else
    {
      fprintf (stderr, "Unimplemented path\n");
      return EXIT_FAILURE;
    }

video_only:
  byte *video_file = *inspect_video;
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

  // Reserve the main, decoder-coordinator, and systems threads. The remaining
  // helper budget supports up to three dav1d workers and two renderer workers.
  s32 logical_cores = platform_get_logical_core_count (),
      worker_budget = MAX (0, logical_cores - 3),
      decoder_threads = MIN (3, (worker_budget * 3 + 2) / 5),
      renderer_workers
      = MAX (0, MIN (2, worker_budget - decoder_threads));
  if (decoder_threads == 0)
    decoder_threads = 1;

  // Initializing video
  vp = init_video (&video_arena, video_file, decoder_threads);
  if (!vp)
    goto end;

  s32 w = vp->codec->width, h = vp->codec->height;

  enum
  {
    // We'll later get this from a config file
    DECODED_FRAME_CACHE_MEGABYTES = 1024
  };

  s32 decoded_frame_cache_capacity = frame_queue_capacity_for_megabytes (
      w, h, DECODED_FRAME_CACHE_MEGABYTES);

  u64 renderer_capacity
      = operations_memory_required (w, h)
        + frame_queue_memory_required (w, h, decoded_frame_cache_capacity)
        + (u64)w * h * sizeof (u32)
        + renderer_pool_memory_required (renderer_workers);
  arena_init (&renderer_arena, renderer_capacity);

  // Renderer and inspector buffers share one arena and lifetime
  RendererPlex *rp = init_renderer (&renderer_arena, w, h);
  operations_init (rp, &renderer_arena);

  platform_init (&platform_state, "Inspector", 0, 0, rp->w, rp->h,
                 (byte *)rp->image_buffer);
  if (platform_state.internal_state == NULL)
    goto end;

  decode_set_display_resolution (
      platform_get_screen_resolution (&platform_state));

  // Keep the video's temporal coherence by default, playback speed scales it
  AVRational video_fps = av_guess_frame_rate (vp->fmt, vp->stream, NULL);
  f64 frame_duration_ms
      = video_fps.num > 0 && video_fps.den > 0
            ? 1000.0 * (f64)video_fps.den / (f64)video_fps.num
            : 1000.0 / 30.0;

  if (!inspector_playback_init (&playback, &renderer_arena, vp, rp,
                                decoded_frame_cache_capacity, renderer_workers,
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
