#include "input.h"
#include "operations.h"

s32 frame_step_request;
bool paused = true;

// Pause playback without changing any pending frame-step requests
void
timeline_pause (void)
{
  paused = true;
}

bool
timeline_process_input (void)
{
  bool changed, step_backward, step_forward;

  // Return whether playback state changed so paused rendering can refresh
  changed = false;
  if (input_is_key_just_pressed (SPACE))
    {
      paused = !paused;
      changed = true;
    }

  step_backward = input_is_key_just_pressed (LEFT);
  step_forward = input_is_key_just_pressed (RIGHT);
  if (step_backward != step_forward)
    {
      timeline_pause ();
      frame_step_request += step_backward ? -1 : 1;
      changed = true;
    }

  return changed;
}

s32
timeline_take_frame_step_request (void)
{
  // Consume the accumulated steps as one request
  s32 request;

  request = frame_step_request;
  frame_step_request = 0;

  return request;
}
