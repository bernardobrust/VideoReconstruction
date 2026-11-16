#include <assert.h>

#include "basic.h"
#include "input.h"
#include "operations.h"
#include "split_screen.h"

enum
{
  SPLIT_SCREEN_PANE_COUNT = 2,
  SPLIT_RENDER_CANCELLED = -2
};

static bool split_screen_enabled = false;
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

bool
split_screen_is_enabled (void)
{
  return split_screen_enabled;
}

void
split_screen_toggle (void)
{
  split_screen_enabled = !split_screen_enabled;
  pending_pane_mask = 0;

  if (split_screen_enabled)
    {
      selected_pane = 0;
      pane_frame_numbers[0] = -1;
      pane_frame_numbers[1] = -1;
      pending_step_delta[0] = 0;
      pending_step_delta[1] = 0;
      request_generation = 0;
      pane_generations[0] = 0;
      pane_generations[1] = 0;
    }
}

bool
split_screen_process_input (void)
{
  bool one_pressed, two_pressed, left_pressed, right_pressed, changed;
  s32 direction;

  one_pressed = input_is_key_just_pressed (ONE);
  two_pressed = input_is_key_just_pressed (TWO);
  if (!split_screen_enabled)
    return false;

  if (one_pressed)
    selected_pane = 0;
  if (two_pressed)
    selected_pane = 1;

  left_pressed = input_is_key_just_pressed (LEFT);
  right_pressed = input_is_key_just_pressed (RIGHT);
  direction = (right_pressed ? 1 : 0) - (left_pressed ? 1 : 0);
  changed = direction != 0;
  if (changed)
    {
      timeline_pause ();

      if (pane_frame_numbers[selected_pane] >= 0)
        {
          pane_frame_numbers[selected_pane]
              = MAX (0, pane_frame_numbers[selected_pane] + direction);
          split_screen_request_pane (selected_pane);
        }
      else
        pending_step_delta[selected_pane] += direction;
    }

  return changed;
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

  pane_frame_numbers[0] = frame_number;
  pane_frame_numbers[1] = frame_number;
  pending_step_delta[0] = 0;
  pending_step_delta[1] = 0;
  split_screen_request_pane (0);
  split_screen_request_pane (1);
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

bool
split_screen_advance_frames (void)
{
  s32 pane;

  if (!split_screen_enabled || pane_frame_numbers[0] < 0
      || pane_frame_numbers[1] < 0)
    return false;

  for (pane = 0; pane < SPLIT_SCREEN_PANE_COUNT; ++pane)
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

local void
playback_request_split_seek (InspectorPlayback *playback, s64 frame_number)
{
  platform_mutex_lock (&playback->state_mutex);

  if (split_screen_is_enabled () && !playback->seek_in_progress
      && frame_number >= 0)
    {
      playback->seek_frame_request = frame_number;
      playback->seek_in_progress = true;
      playback->seek_frame_ready = false;
      platform_condition_broadcast (&playback->state_changed);
      frame_queue_wake_producer (&playback->decoded_frames);
    }

  platform_mutex_unlock (&playback->state_mutex);
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

  operations_get_render_state (&render_state);
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

  memcpy (playback->render_target->image_buffer, frame.renderer->image_buffer,
          (u64)playback->display->w * playback->display->h * sizeof (u32));
  operations_on_frame_decoded (playback->render_target, frame.motion_vectors,
                               frame.motion_vector_count);
  operations_apply_zoom (playback->render_target, &playback->renderer_pool,
                         &render_state);

  platform_mutex_lock (&playback->state_mutex);
  if (!playback->stopping && split_screen_is_enabled ())
    {
      split_screen_compose_pane (playback->display, playback->render_target,
                                 pane);
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
  FrameQueueFrame frame;
  s64 frame_numbers[2];
  u64 generations[2];
  f64 now, speed;
  bool pending[2], stopping, seek_frame_ready, playing, has_pending;
  s32 pane, result, older_pane, future_pane;

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
        {
          if (frame_queue_take (&playback->decoded_frames, &frame))
            frame_queue_release (&playback->decoded_frames, frame);
          else if (frame_queue_result (&playback->decoded_frames) < 0)
            return -1;

          continue;
        }

      older_pane = -1;
      future_pane = -1;
      for (pane = 0; pane < 2; ++pane)
        {
          if (!pending[pane])
            continue;

          result = playback_split_render_frame (
              playback, pane, frame_numbers[pane], generations[pane]);
          if (result == SPLIT_RENDER_CANCELLED)
            return 0;
          if (result < 0)
            older_pane = pane;
          else if (result > 0)
            future_pane = pane;
        }

      if (older_pane >= 0)
        {
          playback_request_split_seek (playback, frame_numbers[older_pane]);

          return 1;
        }

      if (future_pane >= 0)
        {
          if (frame_queue_take (&playback->decoded_frames, &frame))
            {
              frame_queue_release (&playback->decoded_frames, frame);

              return 1;
            }

          result = frame_queue_result (&playback->decoded_frames);
          if (result == 1)
            {
              platform_mutex_lock (&playback->state_mutex);

              split_screen_cancel_future_frame (future_pane,
                                                frame_numbers[future_pane]);
              if (playing)
                timeline_pause ();
              platform_condition_broadcast (&playback->state_changed);

              platform_mutex_unlock (&playback->state_mutex);

              return 1;
            }
          if (result < 0)
            return -1;
        }

      return 1;
    }
}
