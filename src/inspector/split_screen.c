#include <assert.h>

#include "basic.h"
#include "decode.h"
#include "input.h"
#include "operations.h"
#include "split_screen.h"

enum
{
  SPLIT_SCREEN_PANE_COUNT = 2,
  SPLIT_RENDER_CANCELLED = -2
};

static bool split_screen_enabled = false;
static bool split_decoder_available = false;
static bool mirror_enabled = false;
static bool highlight_refresh_pending = false;
static bool has_rendered_pane[SPLIT_SCREEN_PANE_COUNT] = { false, false };
static s32 selected_pane = 0;
static s32 pending_pane_mask = 0;
static s64 pane_frame_numbers[SPLIT_SCREEN_PANE_COUNT] = { -1, -1 };
static s32 pending_step_delta[SPLIT_SCREEN_PANE_COUNT] = { 0, 0 };
static u64 request_generation = 0;
static u64 pane_generations[SPLIT_SCREEN_PANE_COUNT] = { 0, 0 };

local void
split_screen_request_pane (s32 pane)
{
  ++request_generation;
  pane_generations[pane] = request_generation;
  pending_pane_mask |= 1 << pane;
}

local bool
split_decoder_cancelled (InspectorPlayback *playback, s32 pane, u64 generation)
{
  bool cancelled;

  platform_mutex_lock (&playback->state_mutex);
  cancelled = playback->stopping || !split_screen_is_enabled ()
              || (playback->split_decoders[pane].request_pending
                  && playback->split_decoders[pane].requested_generation
                         != generation);
  platform_mutex_unlock (&playback->state_mutex);

  return cancelled;
}

local void
split_decoder_worker (void *arg)
{
  InspectorPlayback *playback;
  SplitDecoder *decoder;
  FrameQueueFrame frame;
  AVFrameSideData *side_data;
  AVMotionVector *motion_vectors;
  u64 motion_vector_count;
  s64 target_frame, decoded_frame;
  u64 generation;
  s32 pane, result;
  bool seek, frame_acquired;

  decoder = arg;
  playback = decoder->playback;
  pane = decoder->pane;

  for (;;)
    {
      platform_mutex_lock (&playback->state_mutex);
      while (!playback->stopping
             && (!split_screen_is_enabled () || !decoder->request_pending))
        platform_condition_wait (&playback->state_changed,
                                 &playback->state_mutex);

      if (playback->stopping)
        {
          platform_mutex_unlock (&playback->state_mutex);

          return;
        }

      target_frame = decoder->requested_frame_number;
      generation = decoder->requested_generation;
      decoder->request_pending = false;
      decoder->busy = true;
      decoder->result = 0;
      seek = decoder->next_frame_number < 0
             || target_frame < decoder->next_frame_number;
      platform_mutex_unlock (&playback->state_mutex);

      result = frame_queue_history_find (&playback->decoded_frames,
                                         target_frame, &frame);
      if (result == 0)
        frame_queue_history_release (&playback->decoded_frames, frame);
      else
        {
          result = frame_queue_acquire_cache_write (&playback->decoded_frames,
                                                    &frame)
                       ? 0
                       : -1;
          frame_acquired = result == 0;
          if (result == 0 && seek)
            result = decode_seek_to_frame (decoder->video, target_frame);

          while (result == 0)
            {
              if (split_decoder_cancelled (playback, pane, generation))
                {
                  result = 2;
                  break;
                }

              result = decode_next_frame (decoder->video, frame.renderer);
              if (result != 0)
                break;

              decoded_frame = video_frame_number_from_timestamp (
                  decoder->video,
                  decoder->video->frame->best_effort_timestamp);
              if (decoded_frame < 0)
                {
                  if (decoder->next_frame_number < 0)
                    {
                      result = -1;
                      break;
                    }
                  decoded_frame = decoder->next_frame_number;
                  video_record_frame_timestamp (decoder->video, decoded_frame);
                }
              decoder->next_frame_number = decoded_frame + 1;

              if (decoded_frame >= target_frame)
                {
                  if (decoded_frame == target_frame)
                    {
                      frame_queue_set_frame_number (&playback->decoded_frames,
                                                    &frame, decoded_frame);
                      side_data = av_frame_get_side_data (
                          decoder->video->frame, AV_FRAME_DATA_MOTION_VECTORS);
                      motion_vectors = side_data != NULL
                                           ? (AVMotionVector *)side_data->data
                                           : NULL;
                      motion_vector_count
                          = side_data != NULL
                                ? side_data->size / sizeof (*motion_vectors)
                                : 0;
                      assert (motion_vector_count <= INT32_MAX);
                      frame_queue_copy_motion_vectors (
                          &playback->decoded_frames, &frame, motion_vectors,
                          (s32)motion_vector_count);
                      frame_queue_publish_history (&playback->decoded_frames,
                                                   frame);
                      result = 0;
                    }
                  else
                    result = 1;
                  break;
                }
            }

          if (result != 0 && frame_acquired)
            frame_queue_release (&playback->decoded_frames, frame);
        }

      platform_mutex_lock (&playback->state_mutex);
      decoder->busy = false;
      if (result < 0 || result == 1)
        decoder->result = result;
      platform_condition_broadcast (&playback->state_changed);
      platform_mutex_unlock (&playback->state_mutex);
    }
}

bool
split_screen_decoders_init (InspectorPlayback *playback, Arena *arena,
                            s32 decoder_threads)
{
  s32 pane, threads_per_decoder;

  assert (playback != NULL && arena != NULL && decoder_threads > 0);

  threads_per_decoder = MAX (1, decoder_threads / 2);
  for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
    {
      playback->split_decoders[pane].video = init_video (
          arena, playback->video->video_file, threads_per_decoder);
      if (playback->split_decoders[pane].video == NULL)
        {
          while (--pane >= 0)
            shutdown_video (playback->split_decoders[pane].video);

          return false;
        }

      video_share_frame_timestamps (playback->split_decoders[pane].video,
                                    playback->video);
      playback->split_decoders[pane].playback = playback;
      playback->split_decoders[pane].pane = pane;
      playback->split_decoders[pane].next_frame_number = -1;
      playback->split_decoders[pane].requested_frame_number = -1;
    }

  for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
    platform_thread_start (&playback->split_decoders[pane].thread,
                           split_decoder_worker,
                           &playback->split_decoders[pane]);

  return true;
}

void
split_screen_decoders_shutdown (InspectorPlayback *playback)
{
  s32 pane;

  assert (playback != NULL);

  for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
    {
      platform_thread_join (&playback->split_decoders[pane].thread);
      shutdown_video (playback->split_decoders[pane].video);
    }
}

bool
split_screen_is_enabled (void)
{
  return split_screen_enabled;
}

void
split_screen_set_decoder_available (bool available)
{
  split_decoder_available = available;
}

void
split_screen_toggle (void)
{
  if (!split_decoder_available)
    return;

  split_screen_enabled = !split_screen_enabled;
  pending_pane_mask = 0;
  highlight_refresh_pending = true;

  if (split_screen_enabled)
    {
      selected_pane = 0;
      pane_frame_numbers[0] = -1;
      pane_frame_numbers[1] = -1;
      has_rendered_pane[0] = false;
      has_rendered_pane[1] = false;
      pending_step_delta[0] = 0;
      pending_step_delta[1] = 0;
      request_generation = 0;
      pane_generations[0] = 0;
      pane_generations[1] = 0;
    }
}

bool
split_screen_is_mirrored (void)
{
  return split_screen_enabled && mirror_enabled;
}

void
split_screen_toggle_mirror (void)
{
  if (!split_screen_enabled)
    return;

  mirror_enabled = !mirror_enabled;
  highlight_refresh_pending = true;
}

void
split_screen_adjust_panes (void)
{
  s32 other_pane;

  if (!split_screen_enabled)
    return;

  other_pane = 1 - selected_pane;
  pane_frame_numbers[other_pane] = pane_frame_numbers[selected_pane];
  pending_step_delta[other_pane] = pending_step_delta[selected_pane];
  if (pane_frame_numbers[other_pane] >= 0)
    split_screen_request_pane (other_pane);
}

bool
split_screen_process_input (s32 display_width)
{
  bool one_pressed, two_pressed, left_pressed, right_pressed,
      selection_changed;
  s32 direction, mouse_x, clicked_pane;

  one_pressed = input_is_key_just_pressed (ONE);
  two_pressed = input_is_key_just_pressed (TWO);
  if (!split_screen_enabled)
    return false;

  selection_changed = false;
  if (one_pressed)
    {
      selection_changed = selected_pane != 0;
      selected_pane = 0;
    }
  if (two_pressed)
    {
      selection_changed = selection_changed || selected_pane != 1;
      selected_pane = 1;
    }

  if (display_width > 0 && input_was_mouse_button_just_pressed (MOUSE_LEFT))
    {
      input_get_mouse_pos (&mouse_x, NULL);
      clicked_pane = mouse_x < display_width / 2 ? 0 : 1;
      if (clicked_pane != selected_pane)
        {
          selected_pane = clicked_pane;
          selection_changed = true;
        }
    }

  if (selection_changed)
    highlight_refresh_pending = true;

  left_pressed = input_is_key_just_pressed (LEFT);
  right_pressed = input_is_key_just_pressed (RIGHT);
  direction = (right_pressed ? 1 : 0) - (left_pressed ? 1 : 0);
  if (direction != 0)
    {
      timeline_pause ();

      if (pane_frame_numbers[selected_pane] >= 0)
        {
          pane_frame_numbers[selected_pane]
              = MAX (0, pane_frame_numbers[selected_pane] + direction);
          split_screen_request_pane (selected_pane);
          if (split_screen_is_mirrored ())
            {
              pane_frame_numbers[1 - selected_pane]
                  = MAX (0, pane_frame_numbers[1 - selected_pane] + direction);
              split_screen_request_pane (1 - selected_pane);
            }
        }
      else
        {
          pending_step_delta[selected_pane] += direction;
          if (split_screen_is_mirrored ())
            pending_step_delta[1 - selected_pane] += direction;
        }
    }

  return direction != 0;
}

void
split_screen_initialize_frames (s64 frame_number)
{
  if (!split_screen_enabled || frame_number < 0 || pane_frame_numbers[0] >= 0
      || pane_frame_numbers[1] >= 0)
    return;

  pane_frame_numbers[0] = MAX (0, frame_number + pending_step_delta[0]);
  pane_frame_numbers[1] = MAX (0, frame_number + pending_step_delta[1]);
  pending_step_delta[0] = 0;
  pending_step_delta[1] = 0;
  split_screen_request_pane (0);
  split_screen_request_pane (1);
}

void
split_screen_reset_frames (s64 frame_number)
{
  if (!split_screen_enabled || frame_number < 0)
    return;

  if (split_screen_is_mirrored ())
    {
      pane_frame_numbers[0] = frame_number;
      pane_frame_numbers[1] = frame_number;
      pending_step_delta[0] = 0;
      pending_step_delta[1] = 0;
      split_screen_request_pane (0);
      split_screen_request_pane (1);
    }
  else
    {
      pane_frame_numbers[selected_pane] = frame_number;
      pending_step_delta[selected_pane] = 0;
      split_screen_request_pane (selected_pane);
    }
}

void
split_screen_request_all (void)
{
  if (split_screen_enabled)
    {
      split_screen_request_pane (0);
      split_screen_request_pane (1);
    }
}

void
split_screen_request_action_panes (void)
{
  if (!split_screen_enabled)
    return;

  split_screen_request_pane (selected_pane);
  if (split_screen_is_mirrored ())
    split_screen_request_pane (1 - selected_pane);
}

bool
split_screen_take_highlight_refresh (void)
{
  bool pending;

  pending = highlight_refresh_pending;
  highlight_refresh_pending = false;

  return pending;
}

bool
split_screen_advance_frames (void)
{
  s32 first_pane, pane, pane_count;

  if (!split_screen_enabled || pane_frame_numbers[0] < 0
      || pane_frame_numbers[1] < 0)
    return false;

  first_pane = split_screen_is_mirrored () ? 0 : selected_pane;
  pane_count = split_screen_is_mirrored () ? SPLIT_SCREEN_PANE_COUNT : 1;
  for (pane = first_pane; pane < first_pane + pane_count; ++pane)
    {
      ++pane_frame_numbers[pane];
      split_screen_request_pane (pane);
    }

  return true;
}

s64
split_screen_get_frame_number (s32 pane)
{
  assert (pane >= 0 && pane < SPLIT_SCREEN_PANE_COUNT);

  return pane_frame_numbers[pane];
}

bool
split_screen_get_pending_frame (s32 pane, s64 *frame_number, u64 *generation)
{
  assert (pane >= 0 && pane < SPLIT_SCREEN_PANE_COUNT && frame_number != NULL
          && generation != NULL);

  if (!(pending_pane_mask & (1 << pane)))
    return false;

  *frame_number = pane_frame_numbers[pane];
  *generation = pane_generations[pane];

  return true;
}

void
split_screen_mark_rendered (s32 pane, s64 frame_number, u64 generation)
{
  assert (pane >= 0 && pane < SPLIT_SCREEN_PANE_COUNT);

  if (pane_frame_numbers[pane] == frame_number
      && pane_generations[pane] == generation)
    pending_pane_mask &= ~(1 << pane);
}

void
split_screen_cancel_future_frame (s32 pane, s64 frame_number)
{
  assert (pane >= 0 && pane < SPLIT_SCREEN_PANE_COUNT);

  if (pane_frame_numbers[pane] == frame_number)
    {
      if (frame_number > 0)
        --pane_frame_numbers[pane];
      pending_pane_mask &= ~(1 << pane);
    }
}

s32
split_screen_get_selected_pane (void)
{
  return selected_pane;
}

local void
split_screen_draw_pane_border (RendererPlex *display, s32 pane)
{
  s32 first_x, pane_w, border_width;
  u32 border_color;

  assert (display != NULL && pane >= 0 && pane < SPLIT_SCREEN_PANE_COUNT);

  first_x = pane == 0 ? 0 : display->w / 2;
  pane_w = pane == 0 ? display->w / 2 : display->w - first_x;
  if (pane_w == 0)
    return;

  border_width = MIN (3, MIN (pane_w, display->h));
  border_color = pane == selected_pane || split_screen_is_mirrored ()
                     ? rgba (255, 220, 0, 255)
                     : rgba (80, 80, 80, 255);
  if (border_width > 0)
    {
      draw_rectangle (first_x, 0, first_x + pane_w - 1, border_width - 1,
                      border_color, display);
      draw_rectangle (first_x, display->h - border_width, first_x + pane_w - 1,
                      display->h - 1, border_color, display);
      draw_rectangle (first_x, 0, first_x + border_width - 1, display->h - 1,
                      border_color, display);
      draw_rectangle (first_x + pane_w - border_width, 0, first_x + pane_w - 1,
                      display->h - 1, border_color, display);
    }
}

void
split_screen_draw_highlights (RendererPlex *display)
{
  s32 pane;

  assert (display != NULL);

  for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
    split_screen_draw_pane_border (display, pane);
}

void
split_screen_compose_pane (RendererPlex *display, const RendererPlex *source,
                           s32 pane)
{
  s32 x, y, first_x, pane_w, source_x;

  assert (display != NULL && source != NULL && pane >= 0
          && pane < SPLIT_SCREEN_PANE_COUNT);

  first_x = pane == 0 ? 0 : display->w / 2;
  pane_w = pane == 0 ? display->w / 2 : display->w - first_x;
  if (pane_w == 0)
    return;

  if (source->w == pane_w && source->h == display->h)
    {
      for (y = 0; y < display->h; ++y)
        memcpy (display->image_buffer + (u64)y * display->w + first_x,
                source->image_buffer + (u64)y * source->w,
                (u64)pane_w * sizeof (*display->image_buffer));
    }
  else
    {
      for (y = 0; y < display->h; ++y)
        for (x = 0; x < pane_w; ++x)
          {
            source_x = (s32)(((s64)x * source->w) / pane_w);
            if (source_x >= source->w)
              source_x = source->w - 1;

            display->image_buffer[(u64)y * display->w + first_x + x]
                = source->image_buffer[(u64)y * source->w + source_x];
          }
    }

  split_screen_draw_pane_border (display, pane);
  operations_draw_selection_box (display, pane);
}

void
split_screen_redraw_cached_frames (InspectorPlayback *playback)
{
  s32 pane;

  assert (playback != NULL);

  for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
    if (has_rendered_pane[pane])
      split_screen_compose_pane (playback->display,
                                 playback->split_render_targets[pane], pane);
}

local s32
split_screen_request_decode (InspectorPlayback *playback, s32 pane,
                             s64 frame_number, u64 generation)
{
  SplitDecoder *decoder;
  s32 result;

  result = 0;
  platform_mutex_lock (&playback->state_mutex);

  decoder = &playback->split_decoders[pane];
  if (decoder->result < 0)
    result = decoder->result;
  else if (split_screen_is_enabled () && frame_number >= 0
           && (!decoder->request_pending
               || decoder->requested_frame_number != frame_number
               || decoder->requested_generation != generation))
    {
      decoder->requested_frame_number = frame_number;
      decoder->requested_generation = generation;
      decoder->request_pending = true;
      decoder->result = 0;
      platform_condition_broadcast (&playback->state_changed);
    }

  platform_mutex_unlock (&playback->state_mutex);

  return result;
}

// Render a cached frame into one split pane
local s32
playback_split_render_frame (InspectorPlayback *playback, s32 pane,
                             s64 frame_number, u64 generation)
{
  FrameQueueFrame frame;
  OperationsRenderState render_state;
  s32 result;

  platform_mutex_lock (&playback->state_mutex);
  if (playback->stopping || !split_screen_is_enabled ())
    {
      platform_mutex_unlock (&playback->state_mutex);

      return SPLIT_RENDER_CANCELLED;
    }

  operations_get_render_state (&render_state, pane);
  playback->render_in_progress = true;
  platform_mutex_unlock (&playback->state_mutex);

  result = frame_queue_history_find (&playback->decoded_frames, frame_number,
                                     &frame);
  if (result != 0)
    {
      platform_mutex_lock (&playback->state_mutex);
      playback->render_in_progress = false;
      platform_condition_broadcast (&playback->state_changed);
      platform_mutex_unlock (&playback->state_mutex);

      return result;
    }

  operations_render_frame (frame.renderer, frame.motion_vectors,
                           frame.motion_vector_count,
                           playback->split_render_targets[pane],
                           &playback->renderer_pool, &render_state);

  platform_mutex_lock (&playback->state_mutex);
  if (!playback->stopping && split_screen_is_enabled ())
    {
      split_screen_compose_pane (playback->display,
                                 playback->split_render_targets[pane], pane);
      has_rendered_pane[pane] = true;
      split_screen_mark_rendered (pane, frame_number, generation);
      playback->frame_ready = true;
      playback->has_frame = true;
      if (pane == split_screen_get_selected_pane ())
        playback->current_frame_number = frame_number;
      playback->result = 0;
    }
  playback->render_in_progress = false;
  platform_condition_broadcast (&playback->state_changed);
  platform_mutex_unlock (&playback->state_mutex);

  frame_queue_history_release (&playback->decoded_frames, frame);

  return 0;
}

// Run queued split-pane renders, fetching or seeking uncached frames as needed
s32
split_screen_update_playback (InspectorPlayback *playback, f64 *next_frame)
{
  s64 frame_numbers[2];
  u64 generations[2];
  f64 now, speed;
  bool pending[2], stopping, seek_frame_ready, playing, has_pending;
  s32 pane, result;
  bool decode_requested;

  for (;;)
    {
      platform_mutex_lock (&playback->state_mutex);

      while (!playback->stopping && split_screen_is_enabled ()
             && playback->has_frame && paused && !playback->seek_in_progress
             && !playback->seek_frame_ready
             && !split_screen_get_pending_frame (0, &frame_numbers[0],
                                                 &generations[0])
             && !split_screen_get_pending_frame (1, &frame_numbers[1],
                                                 &generations[1]))
        platform_condition_wait (&playback->state_changed,
                                 &playback->state_mutex);

      stopping = playback->stopping;
      if (stopping || !split_screen_is_enabled () || !playback->has_frame)
        {
          platform_mutex_unlock (&playback->state_mutex);

          return stopping ? -1 : 0;
        }

      seek_frame_ready = playback->seek_frame_ready;
      playback->seek_frame_ready = false;
      playing = !paused;
      speed = playback_speed;
      pending[0] = split_screen_get_pending_frame (0, &frame_numbers[0],
                                                   &generations[0]);
      pending[1] = split_screen_get_pending_frame (1, &frame_numbers[1],
                                                   &generations[1]);
      has_pending = pending[0] || pending[1];

      if (playing && !seek_frame_ready && !has_pending)
        {
          now = platform_get_time ();
          if (now < *next_frame)
            {
              platform_mutex_unlock (&playback->state_mutex);
              platform_sleep (MIN (*next_frame - now, 2.0));

              continue;
            }

          split_screen_advance_frames ();
          pending[0] = split_screen_get_pending_frame (0, &frame_numbers[0],
                                                       &generations[0]);
          pending[1] = split_screen_get_pending_frame (1, &frame_numbers[1],
                                                       &generations[1]);
          *next_frame = now + playback->frame_duration_ms / speed;
        }

      platform_mutex_unlock (&playback->state_mutex);

      if (seek_frame_ready)
        continue;

      decode_requested = false;
      for (pane = 0; pane < 2; ++pane)
        {
          if (!pending[pane])
            continue;

          if (pane == 1 && decode_requested
              && frame_numbers[0] == frame_numbers[1])
            continue;

          result = playback_split_render_frame (
              playback, pane, frame_numbers[pane], generations[pane]);
          if (result == SPLIT_RENDER_CANCELLED)
            return 0;
          if (result != 0)
            {
              result = split_screen_request_decode (
                  playback, pane, frame_numbers[pane], generations[pane]);
              if (result < 0)
                return result;
              decode_requested = true;
            }
        }

      if (decode_requested)
        {
          platform_mutex_lock (&playback->state_mutex);
          for (;;)
            {
              bool wait_for_decoder = false;

              if (playback->stopping || !split_screen_is_enabled ())
                break;

              for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
                {
                  if (!split_screen_get_pending_frame (
                          pane, &frame_numbers[pane], &generations[pane]))
                    continue;

                  if (playback->split_decoders[pane].result < 0)
                    {
                      result = playback->split_decoders[pane].result;
                      platform_mutex_unlock (&playback->state_mutex);

                      return result;
                    }
                  if (playback->split_decoders[pane].result == 1)
                    {
                      split_screen_cancel_future_frame (pane,
                                                        frame_numbers[pane]);
                      if (playing)
                        timeline_pause ();
                      continue;
                    }

                  wait_for_decoder
                      = wait_for_decoder
                        || playback->split_decoders[pane].request_pending
                        || playback->split_decoders[pane].busy;
                }

              if (!wait_for_decoder)
                break;

              platform_condition_wait (&playback->state_changed,
                                       &playback->state_mutex);
            }

          platform_mutex_unlock (&playback->state_mutex);

          return 1;
        }

      return 1;
    }
}
