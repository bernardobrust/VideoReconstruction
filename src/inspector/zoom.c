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

local void
zoom_match_pane (s32 source, s32 target)
{
  assert (source >= 0 && source < 2 && target >= 0 && target < 2);

  zoom_stack[target].len = zoom_stack[source].len;
  memcpy (zoom_stack[target].items, zoom_stack[source].items,
          (u64)zoom_stack[source].len * sizeof (*zoom_stack[source].items));
}

local void
zoom_apply_selection (void)
{
  s32 sx1, sx2, sy1, sy2, new_w, new_h, new_x1, new_x2, new_y1, new_y2;
  s32 pane, first_pane, pane_count, pane_x, pane_w, action_pane;
  bool zoom_changed;
  ZoomRect cur, next_zoom;

  is_dragging = false;
  needs_paused_redraw = true;

  if (!current_renderer)
    return;

  pane = split_screen_is_enabled () ? split_screen_get_selected_pane () : 0;
  pane_x
      = split_screen_is_enabled () && pane == 1 ? current_renderer->w / 2 : 0;
  pane_w = split_screen_is_enabled ()
               ? (pane == 0 ? current_renderer->w / 2
                            : current_renderer->w - pane_x)
               : current_renderer->w;
  if (pane_w == 0)
    return;

  sx1 = MIN (drag_start_x, drag_cur_x) - pane_x;
  sx2 = MAX (drag_start_x, drag_cur_x) - pane_x;
  sy1 = MIN (drag_start_y, drag_cur_y);
  sy2 = MAX (drag_start_y, drag_cur_y);

  sx1 = clamp_s32 (sx1, 0, pane_w);
  sx2 = clamp_s32 (sx2, 0, pane_w);
  sy1 = clamp_s32 (sy1, 0, current_renderer->h);
  sy2 = clamp_s32 (sy2, 0, current_renderer->h);

  // Only zoom selections at least 8x8, the smallest AV1 block
  if (sx2 - sx1 < 8 || sy2 - sy1 < 8)
    return;

  first_pane = split_screen_is_mirrored () ? 0 : pane;
  pane_count = split_screen_is_mirrored () ? 2 : 1;
  zoom_changed = false;
  for (action_pane = first_pane; action_pane < first_pane + pane_count;
       ++action_pane)
    {
      cur = get_current_zoom (action_pane);
      new_x1 = cur.x + (s32)(((s64)sx1 * cur.w) / pane_w);
      new_y1 = cur.y + (s32)(((s64)sy1 * cur.h) / current_renderer->h);
      new_x2 = cur.x + (s32)(((s64)sx2 * cur.w) / pane_w);
      new_y2 = cur.y + (s32)(((s64)sy2 * cur.h) / current_renderer->h);

      // Integer mapping can collapse a small screen selection to zero source
      // pixels after a few zoom levels. Keep the crop valid so later zooms
      // work
      new_w = MAX (1, new_x2 - new_x1);
      new_h = MAX (1, new_y2 - new_y1);
      new_w = MIN (new_w, current_renderer->w);
      new_h = MIN (new_h, current_renderer->h);
      new_x1 = clamp_s32 (new_x1, 0, current_renderer->w - new_w);
      new_y1 = clamp_s32 (new_y1, 0, current_renderer->h - new_h);

      if (new_x1 != cur.x || new_y1 != cur.y || new_w != cur.w
          || new_h != cur.h)
        {
          next_zoom = (ZoomRect){ new_x1, new_y1, new_w, new_h };
          push_zoom (next_zoom, action_pane);
          zoom_changed = true;
        }
    }

#ifndef NDEBUG
  if (zoom_changed)
    {
      cur = get_current_zoom (pane);
      printf ("Zoom in: [%d, %d, %dx%d] (zoom level: %d)\n", cur.x, cur.y,
              cur.w, cur.h, (s32)zoom_stack[pane].len - 1);
    }
#endif
}
local void
zoom_process_input (void)
{
  s32 mx, my, pane, first_pane, pane_count, pane_w, action_pane;
  bool left_pressed, left_just_released, right_pressed, right_just_pressed,
      right_just_released;
#ifndef NDEBUG
  ZoomRect cur;
#endif

  input_get_mouse_pos (&mx, &my);
  pane = split_screen_is_enabled () ? split_screen_get_selected_pane () : 0;
  first_pane = split_screen_is_mirrored () ? 0 : pane;
  pane_count = split_screen_is_mirrored () ? 2 : 1;
  pane_w = split_screen_is_enabled ()
               ? (pane == 0 ? current_renderer->w / 2
                            : current_renderer->w - current_renderer->w / 2)
               : current_renderer->w;
  left_pressed = input_is_mouse_button_pressed (MOUSE_LEFT);
  left_just_released = input_is_mouse_button_just_released (MOUSE_LEFT);
  right_pressed = input_is_mouse_button_pressed (MOUSE_RIGHT);
  right_just_pressed = input_is_mouse_button_just_pressed (MOUSE_RIGHT);
  right_just_released = input_is_mouse_button_just_released (MOUSE_RIGHT);

  // Ctrl+Right Click undoes the last zoom level
  if (input_is_key_pressed (CTRL) && right_just_pressed)
    {
      if (is_dragging)
        {
          is_dragging = false;
          needs_paused_redraw = true;
        }
      else if (zoom_stack[pane].len > 1)
        {
          for (action_pane = first_pane; action_pane < first_pane + pane_count;
               ++action_pane)
            if (zoom_stack[action_pane].len > 1)
              --zoom_stack[action_pane].len;
#ifndef NDEBUG
          cur = get_current_zoom (pane);
          printf ("Zoom out (undo): [%d, %d, %dx%d] (zoom level: %d)\n", cur.x,
                  cur.y, cur.w, cur.h, (s32)zoom_stack[pane].len - 1);
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
      pan_start_zoom[0] = get_current_zoom (0);
      pan_start_zoom[1] = get_current_zoom (1);
    }

  if (is_panning && (right_pressed || right_just_released))
    {
      if (current_renderer != NULL && pane_w > 0
          && (mx != pan_start_x || my != pan_start_y))
        {
          s64 source_delta_x, source_delta_y;
          s32 max_x, max_y;
          ZoomRect current_zoom, next_zoom;

          // Convert pointer movement to source pixels and move the crop
          // opposite it
          for (action_pane = first_pane; action_pane < first_pane + pane_count;
               ++action_pane)
            {
              current_zoom = pan_start_zoom[action_pane];
              source_delta_x
                  = (((s64)mx - pan_start_x) * current_zoom.w) / pane_w;
              source_delta_y = (((s64)my - pan_start_y) * current_zoom.h)
                               / current_renderer->h;
              max_x = current_renderer->w - current_zoom.w;
              max_y = current_renderer->h - current_zoom.h;
              next_zoom.x
                  = clamp_s32 (current_zoom.x - (s32)source_delta_x, 0, max_x);
              next_zoom.y
                  = clamp_s32 (current_zoom.y - (s32)source_delta_y, 0, max_y);
              next_zoom.w = current_zoom.w;
              next_zoom.h = current_zoom.h;

              if (next_zoom.x != current_zoom.x
                  || next_zoom.y != current_zoom.y)
                {
                  zoom_stack[action_pane]
                      .items[zoom_stack[action_pane].len - 1] = next_zoom;
                  needs_paused_redraw = true;
                }
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
    zoom_apply_selection ();
}
