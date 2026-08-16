#include "platform.h"

#include "dyn_arr.h"
#include "input.h"

DynArr event_queue;

void
platform_dispatch_events (PlatformState *platform_state)
{
  (void)platform_state;

  for (int i = 0; i < event_queue.len; ++i)
    {
      EventType ev = *(EventType *)dyn_arr_get (&event_queue, i);

      // Absolute Coding BTW
      switch (ev)
        {
        case KeyCtrlPress:
          input_set_key_pressed (CTRL);
          break;
        case KeyCtrlRelease:
          input_set_key_released (CTRL);
          break;
        case KeyShiftPress:
          input_set_key_pressed (SHIFT);
          break;
        case KeyShiftRelease:
          input_set_key_released (SHIFT);
          break;
        case KeyEscPress:
          input_set_key_pressed (ESC);
          break;
        case KeyEscRelease:
          input_set_key_released (ESC);
          break;
        case KeyOnePress:
          input_set_key_pressed (ONE);
          break;
        case KeyOneRelease:
          input_set_key_released (ONE);
          break;
        case KeyTwoPress:
          input_set_key_pressed (TWO);
          break;
        case KeyTwoRelease:
          input_set_key_released (TWO);
          break;
        case KeyThreePress:
          input_set_key_pressed (THREE);
          break;
        case KeyThreeRelease:
          input_set_key_released (THREE);
          break;
        case KeyPPress:
          input_set_key_pressed (P);
          break;
        case KeyPRelease:
          input_set_key_released (P);
          break;
        }
    }

  // We are sure we ran all of the events so there's no need to call pop every
  // iteration
  event_queue.len = 0;
}