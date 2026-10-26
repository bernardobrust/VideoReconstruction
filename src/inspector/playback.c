#include "playback.h"

#include <assert.h>
#include <string.h>

#include "basic.h"
#include "operations.h"

local void
decoder_thread (void *arg)
{
  InspectorPlayback *playback;
  FrameQueueFrame frame;
  s32 result;

  playback = arg;
  for (;;)
    {
      if (!frame_queue_acquire_write (&playback->decoded_frames, &frame))
        return;

      result = decode_next_frame (playback->video, frame.renderer);
      if (result == 0)
        frame_queue_publish (&playback->decoded_frames, frame);
      else
        {
          frame_queue_release (&playback->decoded_frames, frame);
          frame_queue_finish (&playback->decoded_frames, result);

          return;
        }
    }
}

local bool
playback_stopping (InspectorPlayback *playback)
{
  bool stopping;

  platform_mutex_lock (&playback->state_mutex);
  stopping = playback->stopping;
  platform_mutex_unlock (&playback->state_mutex);

  return stopping;
}

local void
playback_destroy_synchronization (InspectorPlayback *playback)
{
  frame_queue_destroy (&playback->decoded_frames);
  platform_condition_destroy (&playback->state_changed);
  platform_mutex_destroy (&playback->state_mutex);
}

local void
systems_thread (void *arg)
{
  InspectorPlayback *playback;
  FrameQueueFrame frame;
  OperationsRenderState render_state;
  f64 next_frame, speed, now;
  bool stopping;

  playback = arg;
  next_frame = platform_get_time ();

  for (;;)
    {
      platform_mutex_lock (&playback->state_mutex);
      while (!playback->stopping && paused)
        {
          // The main thread signals after input changes. The condition wait
          // releases state_mutex so input handling can resume playback
          platform_condition_wait (&playback->state_changed,
                                   &playback->state_mutex);
        }
      stopping = playback->stopping;

      platform_mutex_unlock (&playback->state_mutex);
      if (stopping)
        break;

      if (!frame_queue_take (&playback->decoded_frames, &frame))
        break;

      if (playback_stopping (playback))
        {
          frame_queue_release (&playback->decoded_frames, frame);

          break;
        }

      // Keep decoded frames queued ahead, but publish them at the video's
      // cadence. Speed changes adjust this interval without dropping frames
      for (;;)
        {
          platform_mutex_lock (&playback->state_mutex);

          while (paused && !playback->stopping)
            platform_condition_wait (&playback->state_changed,
                                     &playback->state_mutex);
          stopping = playback->stopping;
          speed = playback_speed;

          platform_mutex_unlock (&playback->state_mutex);

          if (stopping)
            break;

          now = platform_get_time ();
          if (now >= next_frame)
            break;

          platform_sleep (MIN (next_frame - now, 2.0));
        }

      if (stopping)
        {
          frame_queue_release (&playback->decoded_frames, frame);

          break;
        }

      platform_mutex_lock (&playback->state_mutex);

      while (paused && !playback->stopping)
        platform_condition_wait (&playback->state_changed,
                                 &playback->state_mutex);

      stopping = playback->stopping;
      if (!stopping)
        {
          // Capture mutable inspector state while locked, then render from the
          // immutable snapshot so input handling does not wait for the pool
          operations_get_render_state (&render_state);
          playback->render_in_progress = true;
        }

      platform_mutex_unlock (&playback->state_mutex);

      if (stopping)
        {
          frame_queue_release (&playback->decoded_frames, frame);

          break;
        }

      // The queue slot stays owned by this consumer while its pixels are read.
      // Rendering targets a private buffer, so the UI may still present the
      // previously published image during this work
      memcpy (playback->render_target->image_buffer,
              frame.renderer->image_buffer,
              (u64)playback->display->w * playback->display->h * sizeof (u32));

      operations_apply_zoom (playback->render_target, &playback->renderer_pool,
                             &render_state);

      platform_mutex_lock (&playback->state_mutex);

      // Publish only a fully rendered frame. The UI presents from display
      // while holding this same mutex, so it never reads pixels during
      // publication
      memcpy (playback->display->image_buffer,
              playback->render_target->image_buffer,
              (u64)playback->display->w * playback->display->h * sizeof (u32));
      playback->render_in_progress = false;
      playback->frame_ready = true;
      playback->has_frame = true;
      playback->result = 0;

      platform_mutex_unlock (&playback->state_mutex);

      frame_queue_release (&playback->decoded_frames, frame);
      next_frame = platform_get_time () + playback->frame_duration_ms / speed;
    }

  platform_mutex_lock (&playback->state_mutex);

  playback->render_in_progress = false;
  playback->result = frame_queue_result (&playback->decoded_frames);
  playback->done = true;
  platform_condition_broadcast (&playback->state_changed);

  platform_mutex_unlock (&playback->state_mutex);
}

bool
inspector_playback_init (InspectorPlayback *playback, Arena *arena,
                         VideoPlex *video, RendererPlex *display,
                         s32 cache_capacity, s32 renderer_workers,
                         f64 frame_duration_ms)
{
  bool decoder_started, systems_started;

  assert (playback != NULL && arena != NULL && video != NULL && display != NULL
          && cache_capacity > 0 && renderer_workers >= 0
          && frame_duration_ms > 0.0);

  memset (playback, 0, sizeof (*playback));

  playback->video = video;
  playback->display = display;
  playback->frame_duration_ms = frame_duration_ms;
  playback->result = 0;
  playback->render_target = init_renderer (arena, display->w, display->h);

  platform_mutex_init (&playback->state_mutex);
  platform_condition_init (&playback->state_changed);
  frame_queue_init (&playback->decoded_frames, arena, display->w, display->h,
                    cache_capacity);

  if (!renderer_pool_init (&playback->renderer_pool, arena, renderer_workers))
    {
      playback_destroy_synchronization (playback);

      return false;
    }

  decoder_started = platform_thread_start (&playback->decoder_thread,
                                           decoder_thread, playback);
  if (!decoder_started)
    {
      renderer_pool_shutdown (&playback->renderer_pool);
      playback_destroy_synchronization (playback);

      return false;
    }
  systems_started = platform_thread_start (&playback->systems_thread,
                                           systems_thread, playback);
  if (!systems_started)
    {
      frame_queue_stop (&playback->decoded_frames);
      platform_thread_join (&playback->decoder_thread);
      renderer_pool_shutdown (&playback->renderer_pool);
      playback_destroy_synchronization (playback);

      return false;
    }

  return true;
}

void
inspector_playback_process_input (InspectorPlayback *playback,
                                  PlatformState *platform_state)
{
  assert (playback != NULL && platform_state != NULL);

  platform_mutex_lock (&playback->state_mutex);

  process_input (platform_state);
  platform_condition_broadcast (&playback->state_changed);

  platform_mutex_unlock (&playback->state_mutex);
}

bool
inspector_playback_update (InspectorPlayback *playback,
                           PlatformState *platform_state)
{
  assert (playback != NULL && platform_state != NULL);

  bool running;

  platform_mutex_lock (&playback->state_mutex);

  if (playback->frame_ready)
    {
      // platform_present reads the shared display buffer synchronously.
      // Holding state_mutex prevents publication from replacing its pixels
      renderer_present (platform_state, playback->display);
      playback->frame_ready = false;
    }

  if (playback->has_frame && !playback->frame_ready
      && !playback->render_in_progress)
    {
      // Reuse the last decoded image to refresh zoom/selection UI even when
      // the decoder has not produced another frame yet
      operations_render_paused (platform_state, playback->display);
    }

  running = !playback->done;

  platform_mutex_unlock (&playback->state_mutex);

  return running;
}

s32
inspector_playback_result (InspectorPlayback *playback)
{
  assert (playback != NULL);

  s32 result;
  platform_mutex_lock (&playback->state_mutex);
  result = playback->done ? playback->result : 0;
  platform_mutex_unlock (&playback->state_mutex);

  return result;
}

void
inspector_playback_stop (InspectorPlayback *playback)
{
  assert (playback != NULL);

  platform_mutex_lock (&playback->state_mutex);

  playback->stopping = true;
  platform_condition_broadcast (&playback->state_changed);

  platform_mutex_unlock (&playback->state_mutex);

  frame_queue_stop (&playback->decoded_frames);
}

void
inspector_playback_shutdown (InspectorPlayback *playback)
{
  assert (playback != NULL);

  inspector_playback_stop (playback);
  platform_thread_join (&playback->systems_thread);
  platform_thread_join (&playback->decoder_thread);
  renderer_pool_shutdown (&playback->renderer_pool);
  playback_destroy_synchronization (playback);
}
