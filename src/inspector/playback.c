#include "playback.h"

#include <assert.h>
#include <string.h>

#include "basic.h"
#include "operations.h"
#include <libavutil/frame.h>
#include <libavutil/motion_vector.h>

local void
decoder_publish_frame (InspectorPlayback *playback, FrameQueueFrame *frame,
                       s64 frame_number, bool to_history)
{
  AVFrameSideData *side_data;
  AVMotionVector *motion_vectors;
  u64 motion_vector_count_u64;
  s32 motion_vector_count;

  video_record_frame_timestamp (playback->video, frame_number);
  frame_queue_set_frame_number (&playback->decoded_frames, frame,
                                frame_number);
  side_data = av_frame_get_side_data (playback->video->frame,
                                      AV_FRAME_DATA_MOTION_VECTORS);
  motion_vectors
      = side_data != NULL ? (AVMotionVector *)side_data->data : NULL;
  motion_vector_count_u64
      = side_data != NULL ? side_data->size / sizeof (*motion_vectors) : 0;
  assert (motion_vector_count_u64 <= INT32_MAX);
  motion_vector_count = (s32)motion_vector_count_u64;
  frame_queue_copy_motion_vectors (&playback->decoded_frames, frame,
                                   motion_vectors, motion_vector_count);
  if (to_history)
    frame_queue_publish_history (&playback->decoded_frames, *frame);
  else
    frame_queue_publish (&playback->decoded_frames, *frame);
}

local void
decoder_seek_finished (InspectorPlayback *playback, bool frame_ready)
{
  platform_mutex_lock (&playback->state_mutex);
  playback->seek_in_progress = false;
  playback->seek_frame_ready = frame_ready;
  platform_condition_broadcast (&playback->state_changed);
  platform_mutex_unlock (&playback->state_mutex);
}

local void
decoder_wait_for_seek (InspectorPlayback *playback)
{
  platform_mutex_lock (&playback->state_mutex);
  while (!playback->stopping && playback->seek_frame_request < 0)
    platform_condition_wait (&playback->state_changed, &playback->state_mutex);
  platform_mutex_unlock (&playback->state_mutex);
}

local void
decoder_thread (void *arg)
{
  InspectorPlayback *playback;
  FrameQueueFrame frame = { 0 };
  s64 next_frame_number, seek_frame_number, decoded_frame_number;
  s32 result;

  playback = arg;
  next_frame_number = 0;
  for (;;)
    {
      platform_mutex_lock (&playback->state_mutex);

      seek_frame_number = playback->seek_frame_request;
      if (seek_frame_number >= 0)
        playback->seek_frame_request = -1;

      platform_mutex_unlock (&playback->state_mutex);

      if (seek_frame_number >= 0)
        {
          result = decode_seek_to_frame (playback->video, seek_frame_number);
          if (result < 0)
            {
              decoder_seek_finished (playback, false);
              continue;
            }

          // Reset cached frames only after FFmpeg accepted the seek
          frame_queue_clear (&playback->decoded_frames);

          // Cache every decoded frame before the target as backward history
          for (;;)
            {
              if (!frame_queue_acquire_write (&playback->decoded_frames,
                                              &frame))
                {
                  if (frame_queue_should_stop (&playback->decoded_frames))
                    return;
                  continue;
                }
              result = decode_next_frame (playback->video, frame.renderer);
              if (result != 0)
                {
                  frame_queue_release (&playback->decoded_frames, frame);
                  frame_queue_finish (&playback->decoded_frames, result);
                  decoder_seek_finished (playback, false);
                  if (result == 1)
                    {
                      decoder_wait_for_seek (playback);
                      break;
                    }
                  return;
                }

              decoded_frame_number = video_frame_number_from_timestamp (
                  playback->video,
                  playback->video->frame->best_effort_timestamp);
              if (decoded_frame_number < 0)
                {
                  frame_queue_release (&playback->decoded_frames, frame);
                  frame_queue_finish (&playback->decoded_frames, -1);
                  decoder_seek_finished (playback, false);
                  return;
                }

              decoder_publish_frame (playback, &frame, decoded_frame_number,
                                     decoded_frame_number < seek_frame_number);
              if (decoded_frame_number >= seek_frame_number)
                {
                  next_frame_number = decoded_frame_number + 1;
                  decoder_seek_finished (playback, true);
                  break;
                }
            }
          continue;
        }

      if (!frame_queue_acquire_write (&playback->decoded_frames, &frame))
        {
          if (frame_queue_should_stop (&playback->decoded_frames))
            return;
          if (frame_queue_result (&playback->decoded_frames) == 1)
            decoder_wait_for_seek (playback);
          continue;
        }

      result = decode_next_frame (playback->video, frame.renderer);
      if (result == 0)
        {
          decoder_publish_frame (playback, &frame, next_frame_number++, false);
          continue;
        }

      frame_queue_release (&playback->decoded_frames, frame);
      frame_queue_finish (&playback->decoded_frames, result);
      if (result == 1)
        {
          decoder_wait_for_seek (playback);
          continue;
        }

      return;
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
playback_request_backward_seek (InspectorPlayback *playback)
{
  platform_mutex_lock (&playback->state_mutex);
  if (!playback->seek_in_progress && playback->current_frame_number > 0)
    {
      playback->seek_frame_request = playback->current_frame_number - 1;
      playback->seek_in_progress = true;
      platform_condition_broadcast (&playback->state_changed);
      frame_queue_wake_producer (&playback->decoded_frames);
    }
  platform_mutex_unlock (&playback->state_mutex);
}

local void
playback_destroy_synchronization (InspectorPlayback *playback)
{
  frame_queue_destroy (&playback->decoded_frames);
  platform_condition_destroy (&playback->state_changed);
  platform_mutex_destroy (&playback->state_mutex);
}

// Coordinate decoding, frame selection, rendering, and publication
local void
systems_thread (void *arg)
{
  InspectorPlayback *playback;
  FrameQueueFrame frame = { 0 };
  OperationsRenderState render_state;
  f64 next_frame, speed, now;
  bool stopping, stepping, from_history, has_frame, first_frame,
      seek_frame_ready;
  s32 step_direction;

  playback = arg;
  next_frame = platform_get_time ();
  speed = playback_speed;

  for (;;)
    {
      platform_mutex_lock (&playback->state_mutex);

      while (!playback->stopping
             && (playback->seek_in_progress
                 || (paused && playback->has_frame
                     && playback->frame_step_request == 0
                     && !playback->seek_frame_ready)))
        {
          // The main thread signals after input changes. The condition wait
          // releases state_mutex so input handling can resume playback
          platform_condition_wait (&playback->state_changed,
                                   &playback->state_mutex);
        }
      stopping = playback->stopping;
      speed = playback_speed;
      first_frame = !playback->has_frame;
      seek_frame_ready = playback->seek_frame_ready;
      playback->seek_frame_ready = false;
      stepping
          = seek_frame_ready || (paused && playback->frame_step_request != 0);
      step_direction = 0;
      if (stepping && !seek_frame_ready)
        {
          step_direction = playback->frame_step_request < 0 ? -1 : 1;
          playback->frame_step_request -= step_direction;
        }

      platform_mutex_unlock (&playback->state_mutex);

      if (stopping)
        break;

      from_history = false;
      if (seek_frame_ready)
        {
          // The target is ready to take, decoded frames before it are history
          has_frame = false;
        }
      else if (stepping)
        {
          has_frame = frame_queue_history_move (&playback->decoded_frames,
                                                step_direction, &frame);
          from_history = has_frame;
          if (!has_frame && step_direction < 0)
            {
              playback_request_backward_seek (playback);
              continue;
            }
        }
      else
        {
          has_frame = frame_queue_history_move (&playback->decoded_frames, 1,
                                                &frame);
          from_history = has_frame;
        }

      if (!has_frame && !frame_queue_take (&playback->decoded_frames, &frame))
        {
          if (stepping)
            continue;

          if (frame_queue_result (&playback->decoded_frames) == 1)
            {
              platform_mutex_lock (&playback->state_mutex);

              timeline_pause ();
              while (!playback->stopping && playback->frame_step_request == 0)
                platform_condition_wait (&playback->state_changed,
                                         &playback->state_mutex);
              stopping = playback->stopping;

              platform_mutex_unlock (&playback->state_mutex);

              if (stopping)
                break;

              continue;
            }

          break;
        }

      if (playback_stopping (playback))
        {
          if (from_history)
            frame_queue_history_release (&playback->decoded_frames, frame);
          else
            frame_queue_release (&playback->decoded_frames, frame);

          break;
        }

      // Keep decoded frames queued ahead, but publish them at the video's
      // cadence. Speed changes adjust this interval without dropping frames
      for (; !stepping && !first_frame;)
        {
          platform_mutex_lock (&playback->state_mutex);

          while (paused && !playback->stopping
                 && playback->frame_step_request == 0)
            platform_condition_wait (&playback->state_changed,
                                     &playback->state_mutex);
          stopping = playback->stopping;
          if (paused && playback->frame_step_request != 0)
            {
              step_direction = playback->frame_step_request < 0 ? -1 : 1;
              playback->frame_step_request -= step_direction;
              stepping = true;
            }
          speed = playback_speed;

          platform_mutex_unlock (&playback->state_mutex);

          if (stepping)
            {
              if (step_direction < 0)
                {
                  if (from_history)
                    {
                      if (!frame_queue_history_move (&playback->decoded_frames,
                                                     -1, &frame))
                        {
                          frame_queue_history_release (
                              &playback->decoded_frames, frame);
                          playback_request_backward_seek (playback);
                          goto next_iteration;
                        }
                    }
                  else
                    {
                      frame_queue_requeue_front (&playback->decoded_frames,
                                                 frame);
                      if (!frame_queue_history_move (&playback->decoded_frames,
                                                     -1, &frame))
                        {
                          playback_request_backward_seek (playback);
                          goto next_iteration;
                        }
                    }
                  from_history = true;
                }

              break;
            }

          if (stopping)
            break;

          now = platform_get_time ();
          if (now >= next_frame)
            break;

          platform_sleep (MIN (next_frame - now, 2.0));
        }

      if (stopping)
        {
          if (from_history)
            frame_queue_history_release (&playback->decoded_frames, frame);
          else
            frame_queue_release (&playback->decoded_frames, frame);

          break;
        }

      platform_mutex_lock (&playback->state_mutex);
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
          if (from_history)
            frame_queue_history_release (&playback->decoded_frames, frame);
          else
            frame_queue_release (&playback->decoded_frames, frame);

          break;
        }

      // The queue slot stays owned by this consumer while its pixels are read.
      // Rendering targets a private buffer, so the UI may still present the
      // previously published image during this work
      memcpy (playback->render_target->image_buffer,
              frame.renderer->image_buffer,
              (u64)playback->display->w * playback->display->h * sizeof (u32));

      operations_on_frame_decoded (playback->render_target,
                                   frame.motion_vectors,
                                   frame.motion_vector_count);
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
      playback->current_frame_number = frame.frame_number;
      playback->result = 0;

      platform_mutex_unlock (&playback->state_mutex);

      if (from_history)
        frame_queue_history_release (&playback->decoded_frames, frame);
      else
        frame_queue_release (&playback->decoded_frames, frame);
      next_frame = platform_get_time () + playback->frame_duration_ms / speed;

    next_iteration:;
    }

  platform_mutex_lock (&playback->state_mutex);

  playback->render_in_progress = false;
  playback->result = frame_queue_result (&playback->decoded_frames);
  playback->done = true;
  platform_condition_broadcast (&playback->state_changed);

  platform_mutex_unlock (&playback->state_mutex);
}

void
inspector_playback_init (InspectorPlayback *playback, Arena *arena,
                         VideoPlex *video, RendererPlex *display,
                         s32 cache_capacity, s32 renderer_workers,
                         f64 frame_duration_ms)
{
  assert (playback != NULL && arena != NULL && video != NULL && display != NULL
          && cache_capacity > 0 && renderer_workers >= 0
          && frame_duration_ms > 0.0);

  memset (playback, 0, sizeof (*playback));

  playback->video = video;
  playback->display = display;
  playback->frame_duration_ms = frame_duration_ms;
  playback->result = 0;
  playback->seek_frame_request = -1;
  playback->current_frame_number = -1;
  playback->render_target = init_renderer (arena, display->w, display->h);

  platform_mutex_init (&playback->state_mutex);
  platform_condition_init (&playback->state_changed);
  frame_queue_init (&playback->decoded_frames, arena, display->w, display->h,
                    cache_capacity);

  renderer_pool_init (&playback->renderer_pool, arena, renderer_workers);

  platform_thread_start (&playback->decoder_thread, decoder_thread, playback);
  platform_thread_start (&playback->systems_thread, systems_thread, playback);
}

void
inspector_playback_process_input (InspectorPlayback *playback,
                                  PlatformState *platform_state)
{
  assert (playback != NULL && platform_state != NULL);

  platform_mutex_lock (&playback->state_mutex);

  process_input (platform_state);
  playback->frame_step_request += timeline_take_frame_step_request ();
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
