// Fixed 1/8 step
f64 playback_speed = 1.0, speed_step = 0.125;
bool paused = false, slow_down, speed_up;

inline void
process_input (PlatformState *p)
{
  if (input_is_key_pressed (ESC))
    platform_stop (p);

  // Speed up and slow down
  speed_up = input_is_key_just_pressed (UP);
  if (speed_up)
    {
      // We can't (and shouldn't) decode faster than the video's time
      // references
      playback_speed += playback_speed >= 1.0 ? 0.0 : speed_step;

      printf ("Playback speed increased to: %f\n", playback_speed);
    }

  slow_down = input_is_key_just_pressed (DOWN);
  if (slow_down)
    {
      // We do not want super-low speeds, this actually limits to 1/4 of
      // the normal speed
      playback_speed -= playback_speed <= speed_step ? 0 : speed_step;

      printf ("Playback speed decreased to: %f\n", playback_speed);
    }

  // NOTE: this pause is for the video, not the program, so UI, inputs,
  // pre-decoding (theaded) should still keep going
  if (input_is_key_just_pressed (SPACE))
    // Toggle pause
    paused = !paused;
}