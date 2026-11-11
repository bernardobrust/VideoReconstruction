typedef struct
{
  u32 *src, *dst;
  s32 src_w, src_h, dst_w, dst_h, zx, zy, zw, zh;
  s32 *x_map;
  bool fast_copy;
} ScaleRowsContext;

// Scale an assigned row range for parallel processing by the renderer pool
local void
scale_image_rows (void *arg, s32 first_row, s32 end_row)
{
  ScaleRowsContext *context;
  u32 *src_row, *dst_row;
  s32 x, y, sy;

  context = arg;
  for (y = first_row; y < end_row; ++y)
    {
      src_row = context->src;
      dst_row = context->dst + (u64)y * context->dst_w;

      if (context->fast_copy)
        {
          memcpy (dst_row, src_row + (u64)y * context->src_w,
                  (u64)context->dst_w * sizeof (*dst_row));
          continue;
        }

      sy = context->zy + (s32)(((s64)y * context->zh) / context->dst_h);
      if (sy >= context->src_h)
        sy = context->src_h - 1;

      src_row += (u64)sy * context->src_w;

      for (x = 0; x < context->dst_w; ++x)
        dst_row[x] = src_row[context->x_map[x]];
    }
}

local void
scale_image_region (u32 *src, s32 src_w, s32 src_h, s32 zx, s32 zy, s32 zw,
                    s32 zh, u32 *dst, s32 dst_w, s32 dst_h, RendererPool *pool)
{
  bool fast_copy;
  s32 x, sx;
  ScaleRowsContext context;

  fast_copy = zx == 0 && zy == 0 && zw == dst_w && zh == dst_h
              && src_w == dst_w && src_h == dst_h;

  // Keep the crop within the source image before building the horizontal map
  if (zx < 0)
    zx = 0;
  if (zy < 0)
    zy = 0;
  if (zw < 1)
    zw = 1;
  if (zh < 1)
    zh = 1;
  if (zx + zw > src_w)
    zw = src_w - zx;
  if (zy + zh > src_h)
    zh = src_h - zy;

  if (likely (!fast_copy))
    {
      for (x = 0; x < dst_w; ++x)
        {
          sx = zx + (s32)(((s64)x * zw) / dst_w);
          if (sx >= src_w)
            sx = src_w - 1;
          scale_x_map[x] = sx;
        }
    }

  context
      = (ScaleRowsContext){ src, dst, src_w, src_h, dst_w,       dst_h,
                            zx,  zy,  zw,    zh,    scale_x_map, fast_copy };
  if (pool != NULL)
    renderer_pool_run_rows (pool, dst_h, scale_image_rows, &context);
  else
    scale_image_rows (&context, 0, dst_h);
}

local void
draw_selection_box (s32 x1, s32 y1, s32 x2, s32 y2, RendererPlex *renderer)
{
  s32 min_x = MIN (x1, x2), max_x = MAX (x1, x2);
  s32 min_y = MIN (y1, y2), max_y = MAX (y1, y2);

  draw_rectangle_t (min_x, min_y, max_x, max_y, fill_color, renderer);
}

void
process_input (PlatformState *p)
{
  bool timeline_changed;

  // Timeline changes invalidate the paused frame shown on screen
  timeline_changed = timeline_process_input ();
  if (timeline_changed)
    needs_paused_redraw = true;

  if (input_is_key_pressed (ESC))
    platform_stop (p);

  if (input_is_key_just_pressed (F))
    {
      fullscreen = !fullscreen;
      platform_set_fullscreen (p, fullscreen);
    }

  if (input_is_key_just_pressed (T))
    {
      show_data_overlay = !show_data_overlay;
      needs_paused_redraw = true;
    }

  // Adjust the multiplier applied to the video's frame cadence
  speed_up = input_is_key_just_pressed (UP);
  if (speed_up)
    {
      playback_speed += speed_step;
#ifndef NDEBUG
      printf ("Playback speed increased to: %f\n", playback_speed);
#endif
    }

  slow_down = input_is_key_just_pressed (DOWN);
  if (slow_down)
    {
      playback_speed = MAX (speed_step, playback_speed - speed_step);
#ifndef NDEBUG
      printf ("Playback speed decreased to: %f\n", playback_speed);
#endif
    }

  // Read pointer coordinates and button transitions once for this input update
  s32 mx = 0, my = 0;
  input_get_mouse_pos (&mx, &my);
  bool left_pressed = input_is_mouse_button_pressed (MOUSE_LEFT);
  bool left_just_released = input_is_mouse_button_just_released (MOUSE_LEFT);
  bool right_pressed = input_is_mouse_button_pressed (MOUSE_RIGHT);
  bool right_just_pressed = input_is_mouse_button_just_pressed (MOUSE_RIGHT);
  bool right_just_released = input_is_mouse_button_just_released (MOUSE_RIGHT);

  // Ctrl+Right Click undoes the last zoom level
  if (input_is_key_pressed (CTRL) && right_just_pressed)
    {
      if (is_dragging)
        {
          is_dragging = false;
          needs_paused_redraw = true;
        }
      else if (zoom_stack.len > 1)
        {
          --zoom_stack.len;
#ifndef NDEBUG
          ZoomRect cur = get_current_zoom ();
          printf ("Zoom out (undo): [%d, %d, %dx%d] (zoom level: %d)\n", cur.x,
                  cur.y, cur.w, cur.h, (s32)zoom_stack.len - 1);
#endif
          needs_paused_redraw = true;
        }
    }

  // Right Mouse Drag pans the current view
  if (!input_is_key_pressed (CTRL) && right_just_pressed)
    {
      if (is_dragging)
        {
          is_dragging = false;
          needs_paused_redraw = true;
        }

      is_panning = true;
      pan_start_x = mx;
      pan_start_y = my;
      pan_start_zoom = get_current_zoom ();
    }

  if (is_panning && (right_pressed || right_just_released))
    {
      if (current_renderer != NULL && (mx != pan_start_x || my != pan_start_y))
        {
          s64 source_delta_x, source_delta_y;
          s32 max_x, max_y;
          ZoomRect current_zoom, next_zoom;

          // Convert pointer movement to source pixels and move the crop
          // opposite it
          source_delta_x = (((s64)mx - pan_start_x) * pan_start_zoom.w)
                           / current_renderer->w;
          source_delta_y = (((s64)my - pan_start_y) * pan_start_zoom.h)
                           / current_renderer->h;
          max_x = current_renderer->w - pan_start_zoom.w;
          max_y = current_renderer->h - pan_start_zoom.h;
          next_zoom.x
              = clamp_s32 (pan_start_zoom.x - (s32)source_delta_x, 0, max_x);
          next_zoom.y
              = clamp_s32 (pan_start_zoom.y - (s32)source_delta_y, 0, max_y);
          next_zoom.w = pan_start_zoom.w;
          next_zoom.h = pan_start_zoom.h;

          current_zoom = get_current_zoom ();
          if (next_zoom.x != current_zoom.x || next_zoom.y != current_zoom.y)
            {
              zoom_stack.items[zoom_stack.len - 1] = next_zoom;
              needs_paused_redraw = true;
            }
        }
    }

  if (is_panning && right_just_released)
    is_panning = false;

  // Ctrl+Left Click starts a zoom selection
  if (input_is_key_pressed (CTRL)
      && input_is_mouse_button_just_pressed (MOUSE_LEFT))
    {
      is_dragging = true;
      drag_start_x = mx;
      drag_start_y = my;
      drag_cur_x = mx;
      drag_cur_y = my;
      needs_paused_redraw = true;
    }

  // Update the selection rectangle while dragging
  if (is_dragging && left_pressed)
    {
      if (mx != drag_cur_x || my != drag_cur_y)
        {
          drag_cur_x = mx;
          drag_cur_y = my;
          needs_paused_redraw = true;
        }
    }

  // Apply the selected rectangle as the next zoom level on release
  if (is_dragging && (!left_pressed || left_just_released))
    {
      is_dragging = false;
      needs_paused_redraw = true;

      if (current_renderer)
        {
          s32 sx1 = MIN (drag_start_x, drag_cur_x),
              sx2 = MAX (drag_start_x, drag_cur_x),
              sy1 = MIN (drag_start_y, drag_cur_y),
              sy2 = MAX (drag_start_y, drag_cur_y);

          sx1 = clamp_s32 (sx1, 0, current_renderer->w),
          sx2 = clamp_s32 (sx2, 0, current_renderer->w),
          sy1 = clamp_s32 (sy1, 0, current_renderer->h),
          sy2 = clamp_s32 (sy2, 0, current_renderer->h);

          s32 sel_w = sx2 - sx1, sel_h = sy2 - sy1;

          // Only zoom if the area selected is at least 8x8 (smallest AV1
          // block)
          if (likely (sel_w >= 8 && sel_h >= 8))
            {
              ZoomRect cur = get_current_zoom ();

              s32 new_x1
                  = cur.x + (s32)(((s64)sx1 * cur.w) / current_renderer->w),
                  new_y1
                  = cur.y + (s32)(((s64)sy1 * cur.h) / current_renderer->h),
                  new_x2
                  = cur.x + (s32)(((s64)sx2 * cur.w) / current_renderer->w),
                  new_y2
                  = cur.y + (s32)(((s64)sy2 * cur.h) / current_renderer->h);

              // Integer mapping can collapse a small screen selection to
              // zero source pixels after a few zoom levels. Keep the crop
              // valid so later zooms do not get stuck on a zero-sized rect.
              s32 new_w = MAX (1, new_x2 - new_x1),
                  new_h = MAX (1, new_y2 - new_y1);

              // Clamp to video dimensions
              if (new_w > current_renderer->w)
                new_w = current_renderer->w;
              if (new_h > current_renderer->h)
                new_h = current_renderer->h;

              if (new_x1 < 0)
                new_x1 = 0;
              if (new_y1 < 0)
                new_y1 = 0;
              if (new_x1 + new_w > current_renderer->w)
                new_x1 = current_renderer->w - new_w;
              if (new_y1 + new_h > current_renderer->h)
                new_y1 = current_renderer->h - new_h;

              if (likely (new_x1 != cur.x || new_y1 != cur.y || new_w != cur.w
                          || new_h != cur.h))
                {
                  ZoomRect next_zoom = { new_x1, new_y1, new_w, new_h };
                  push_zoom (next_zoom);
#ifndef NDEBUG
                  printf ("Zoom in: [%d, %d, %dx%d] (zoom level: %d)\n",
                          new_x1, new_y1, new_w, new_h,
                          (s32)zoom_stack.len - 1);
#endif
                }
            }
        }
    }
}
