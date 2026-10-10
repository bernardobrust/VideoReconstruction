#include "platform.h"

#include "dyn_arr.h"
#include "input.h"

DynArr event_queue;

void
platform_dispatch_events (void)
{
  for (s32 i = 0; i < event_queue.len; ++i)
    {
      PlatformEvent ev = *(PlatformEvent *)dyn_arr_get (&event_queue, i);

      // Absolute Coding BTW
      switch (ev.type)
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
        case KeyLeftPress:
          input_set_key_pressed (LEFT);
          break;
        case KeyLeftRelease:
          input_set_key_released (LEFT);
          break;
        case KeyRightPress:
          input_set_key_pressed (RIGHT);
          break;
        case KeyRightRelease:
          input_set_key_released (RIGHT);
          break;
        case KeyUpPress:
          input_set_key_pressed (UP);
          break;
        case KeyUpRelease:
          input_set_key_released (UP);
          break;
        case KeyDownPress:
          input_set_key_pressed (DOWN);
          break;
        case KeyDownRelease:
          input_set_key_released (DOWN);
          break;
        case KeySpacePress:
          input_set_key_pressed (SPACE);
          break;
        case KeySpaceRelease:
          input_set_key_released (SPACE);
          break;
        case MouseMove:
          input_set_mouse_pos (ev.x, ev.y);
          break;
        case MouseLeftPress:
          input_set_mouse_pos (ev.x, ev.y);
          input_set_mouse_button_pressed (MOUSE_LEFT);
          break;
        case MouseLeftRelease:
          input_set_mouse_pos (ev.x, ev.y);
          input_set_mouse_button_released (MOUSE_LEFT);
          break;
        case MouseRightPress:
          input_set_mouse_pos (ev.x, ev.y);
          input_set_mouse_button_pressed (MOUSE_RIGHT);
          break;
        case MouseRightRelease:
          input_set_mouse_pos (ev.x, ev.y);
          input_set_mouse_button_released (MOUSE_RIGHT);
          break;
        }
    }

  // We are sure we ran all of the events so there's no need to call pop every
  // iteration
  event_queue.len = 0;
}
