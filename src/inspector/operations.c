#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basic.h"
#include "dyn_arr.h"
#include "input.h"
#include "motion_vectors.h"
#include "operations.h"
#include "performance.h"
#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"
#include "split_screen.h"

f64 playback_speed = 1.0, speed_step = 0.125;
bool fullscreen = false, slow_down, speed_up;

typedef struct
{
  s32 x, y, w, h;
} ZoomRect;

typedef struct
{
  ZoomRect *items;
  s32 len, cap;
} ZoomStack;

enum
{
  MAX_ZOOM_LEVELS = 16
};

ZoomStack zoom_stack[2] = { 0 };
u32 *clean_frame_buffer = NULL;
u32 *overlay_frame_buffer = NULL;
s32 *scale_x_map = NULL;
RendererPlex *current_renderer = NULL;

bool is_dragging = false, is_panning = false, needs_paused_redraw = true;
bool split_selection_redraw_pending = false;
bool show_data_overlay[2] = { true, true };
AVMotionVector *current_motion_vectors = NULL;
s32 current_motion_vector_count = 0, current_motion_vector_capacity = 0;
s32 drag_start_x = 0, drag_start_y = 0, drag_cur_x = 0, drag_cur_y = 0;
s32 pan_start_x = 0, pan_start_y = 0;
ZoomRect pan_start_zoom[2];

local void zoom_process_input (void);
local void zoom_match_pane (s32 source, s32 target);
local void scale_image_rows (void *arg, s32 first_row, s32 end_row);
local void scale_image_region (u32 *src, s32 src_w, s32 src_h, s32 zx, s32 zy,
                               s32 zw, s32 zh, u32 *dst, s32 dst_w, s32 dst_h,
                               RendererPool *pool);
local void draw_selection_box (s32 x1, s32 y1, s32 x2, s32 y2,
                               RendererPlex *renderer);

const u32 fill_color
    = 1684327679; // Precomputed from rgba (100, 180, 255, 100)

local void
add_array_size (u64 *total, u64 count, u64 element_size)
{
  assert (element_size > 0);
  assert (count <= (UINT64_MAX - *total) / element_size);

  *total += count * element_size;
}

u64
operations_memory_required (s32 w, s32 h)
{
  assert (w > 0 && h > 0);
  assert ((u64)w <= UINT64_MAX / (u64)h / sizeof (u32));

  u64 pixels = (u64)w * (u64)h, total = 128;
  add_array_size (&total, 1, sizeof (RendererPlex));
  add_array_size (&total, pixels * 3, sizeof (u32));
  add_array_size (&total, (u64)w, sizeof (s32));
  add_array_size (&total, 2 * MAX_ZOOM_LEVELS, sizeof (ZoomRect));

  return total;
}

local void
push_zoom (ZoomRect rect, s32 pane)
{
  assert (pane >= 0 && pane < 2);

  if (zoom_stack[pane].len >= zoom_stack[pane].cap)
    {
      assert (zoom_stack[pane].len > 1);

      memmove (zoom_stack[pane].items + 1, zoom_stack[pane].items + 2,
               (zoom_stack[pane].len - 2) * sizeof (*zoom_stack[pane].items));
      --zoom_stack[pane].len;
    }

  zoom_stack[pane].items[zoom_stack[pane].len++] = rect;
}

local ZoomRect
get_current_zoom (s32 pane)
{
  assert (pane >= 0 && pane < 2);

  if (zoom_stack[pane].len > 0)
    return zoom_stack[pane].items[zoom_stack[pane].len - 1];

  ZoomRect def = { 0, 0, current_renderer ? current_renderer->w : 0,
                   current_renderer ? current_renderer->h : 0 };

  return def;
}

void
operations_init (RendererPlex *renderer, Arena *arena)
{
  assert (renderer != NULL && arena != NULL);

  current_renderer = renderer;

  u64 buffer_size
      = (u64)renderer->w * (u64)renderer->h * sizeof (*clean_frame_buffer);
  clean_frame_buffer = arena_alloc (arena, buffer_size, _Alignof (u32));
  overlay_frame_buffer = arena_alloc (arena, buffer_size, _Alignof (u32));
  scale_x_map = arena_alloc (arena, (u64)renderer->w * sizeof (*scale_x_map),
                             _Alignof (s32));
  zoom_stack[0].cap = MAX_ZOOM_LEVELS;
  zoom_stack[1].cap = MAX_ZOOM_LEVELS;
  zoom_stack[0].items = arena_alloc (
      arena, (u64)zoom_stack[0].cap * sizeof (*zoom_stack[0].items),
      _Alignof (ZoomRect));
  zoom_stack[1].items = arena_alloc (
      arena, (u64)zoom_stack[1].cap * sizeof (*zoom_stack[1].items),
      _Alignof (ZoomRect));
  zoom_stack[0].len = 0;
  zoom_stack[1].len = 0;

  assert (clean_frame_buffer != NULL && overlay_frame_buffer != NULL
          && scale_x_map != NULL && zoom_stack[0].items != NULL
          && zoom_stack[1].items != NULL);

  ZoomRect base_rect = { 0, 0, renderer->w, renderer->h };
  push_zoom (base_rect, 0);
  push_zoom (base_rect, 1);

  is_dragging = false;
  is_panning = false;
  needs_paused_redraw = true;
}

void
operations_set_fullscreen (bool enabled)
{
  fullscreen = enabled;
}

void
process_input (PlatformState *p)
{
  bool split_input_changed, timeline_changed;

  split_input_changed = split_screen_process_input (
      current_renderer ? current_renderer->w : 0);
  timeline_changed = timeline_process_input (!split_screen_is_enabled ());
  if (split_input_changed || timeline_changed)
    needs_paused_redraw = true;

  if (input_is_key_pressed (ESC))
    platform_stop (p);

  if (input_is_key_just_pressed (S))
    {
      split_screen_toggle ();
      needs_paused_redraw = true;
#ifndef NDEBUG
      printf ("Split screen %s\n",
              split_screen_is_enabled () ? "enabled" : "disabled");
#endif
    }

  if (input_is_key_just_pressed (M))
    {
      split_screen_toggle_mirror ();
#ifndef NDEBUG
      printf ("Mirror mode %s\n",
              split_screen_is_mirrored () ? "enabled" : "disabled");
#endif
    }

  if (input_is_key_just_pressed (A) && split_screen_is_enabled ())
    {
      s32 pane;

      pane = split_screen_get_selected_pane ();
      split_screen_adjust_panes ();
      zoom_match_pane (pane, 1 - pane);
      show_data_overlay[1 - pane] = show_data_overlay[pane];
    }

  if (input_is_key_just_pressed (F))
    {
      fullscreen = !fullscreen;
      platform_set_fullscreen (p, fullscreen);
#ifndef NDEBUG
      printf ("Fullscreen %s\n", fullscreen ? "enabled" : "disabled");
#endif
    }

  if (input_is_key_just_pressed (T))
    {
      s32 pane;

      pane
          = split_screen_is_enabled () ? split_screen_get_selected_pane () : 0;
      show_data_overlay[pane] = !show_data_overlay[pane];
      if (split_screen_is_mirrored ())
        show_data_overlay[1 - pane] = !show_data_overlay[1 - pane];
      needs_paused_redraw = true;
#ifndef NDEBUG
      printf ("Motion-vector overlay %s\n",
              show_data_overlay[pane] ? "enabled" : "disabled");
#endif
    }

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

  zoom_process_input ();
}

void
operations_on_frame_decoded (RendererPlex *renderer,
                             const AVMotionVector *motion_vectors,
                             s32 motion_vector_count)
{
  AVMotionVector *resized;

  if (!clean_frame_buffer || !renderer)
    return;

  memcpy (clean_frame_buffer, renderer->image_buffer,
          renderer->w * renderer->h * sizeof (u32));
  assert (motion_vector_count >= 0
          && (motion_vectors != NULL || motion_vector_count == 0));
  if (motion_vector_count > current_motion_vector_capacity)
    {
      resized = realloc (current_motion_vectors,
                         (u64)motion_vector_count * sizeof (*resized));
      assert (resized != NULL);
      current_motion_vectors = resized;
      current_motion_vector_capacity = motion_vector_count;
    }
  if (motion_vector_count > 0)
    memcpy (current_motion_vectors, motion_vectors,
            (u64)motion_vector_count * sizeof (*motion_vectors));
  current_motion_vector_count = motion_vector_count;
}

void
operations_get_render_state (OperationsRenderState *state, s32 pane)
{
  assert (state != NULL);

  ZoomRect zoom;

  zoom = get_current_zoom (pane);
  state->zoom_x = zoom.x;
  state->zoom_y = zoom.y;
  state->zoom_w = zoom.w;
  state->zoom_h = zoom.h;
  state->is_dragging = is_dragging;
  state->show_data_overlay = show_data_overlay[pane];
  state->drag_start_x = drag_start_x;
  state->drag_start_y = drag_start_y;
  state->drag_cur_x = drag_cur_x;
  state->drag_cur_y = drag_cur_y;
  state->is_dragging = state->is_dragging && !split_screen_is_enabled ();
}

void
operations_apply_zoom (RendererPlex *renderer, RendererPool *pool,
                       const OperationsRenderState *state)
{
  assert (state != NULL);

  if (!clean_frame_buffer || !renderer)
    return;

  memcpy (overlay_frame_buffer, clean_frame_buffer,
          (u64)renderer->w * renderer->h * sizeof (u32));
  if (state->show_data_overlay)
    {
      RendererPlex source = { renderer->w, renderer->h, overlay_frame_buffer };
      renderer_draw_motion_vectors (
          current_motion_vectors, current_motion_vector_count, &source, pool);
    }

  scale_image_region (overlay_frame_buffer, renderer->w, renderer->h,
                      state->zoom_x, state->zoom_y, state->zoom_w,
                      state->zoom_h, renderer->image_buffer, renderer->w,
                      renderer->h, pool);
  if (state->is_dragging)
    draw_selection_box (state->drag_start_x, state->drag_start_y,
                        state->drag_cur_x, state->drag_cur_y, renderer);
}

void
operations_render_frame (const RendererPlex *source,
                         const AVMotionVector *motion_vectors,
                         s32 motion_vector_count, RendererPlex *destination,
                         RendererPool *pool,
                         const OperationsRenderState *state)
{
  RendererPlex overlay_source;
  u32 *source_buffer;

  assert (source != NULL && destination != NULL && state != NULL
          && motion_vector_count >= 0
          && (motion_vectors != NULL || motion_vector_count == 0));

  source_buffer = source->image_buffer;
  if (state->show_data_overlay)
    {
      memcpy (overlay_frame_buffer, source_buffer,
              (u64)source->w * source->h * sizeof (u32));
      overlay_source
          = (RendererPlex){ source->w, source->h, overlay_frame_buffer };
      renderer_draw_motion_vectors (motion_vectors, motion_vector_count,
                                    &overlay_source, pool);
      source_buffer = overlay_frame_buffer;
    }

  scale_image_region (source_buffer, source->w, source->h, state->zoom_x,
                      state->zoom_y, state->zoom_w, state->zoom_h,
                      destination->image_buffer, destination->w,
                      destination->h, pool);
  if (state->is_dragging)
    draw_selection_box (state->drag_start_x, state->drag_start_y,
                        state->drag_cur_x, state->drag_cur_y, destination);
}

void
operations_draw_selection_box (RendererPlex *renderer, s32 pane)
{
  s32 selected_pane, selected_left, selected_width, target_left, target_width;
  s32 x1, x2;

  assert (renderer != NULL && pane >= 0 && pane < 2);

  if (!is_dragging
      || (pane != split_screen_get_selected_pane ()
          && !split_screen_is_mirrored ()))
    return;

  x1 = drag_start_x;
  x2 = drag_cur_x;
  selected_pane = split_screen_get_selected_pane ();
  if (pane != selected_pane)
    {
      selected_left = selected_pane == 0 ? 0 : renderer->w / 2;
      selected_width
          = selected_pane == 0 ? renderer->w / 2 : renderer->w - selected_left;
      target_left = pane == 0 ? 0 : renderer->w / 2;
      target_width = pane == 0 ? renderer->w / 2 : renderer->w - target_left;
      if (selected_width == 0)
        return;

      x1 = target_left
           + (s32)(((s64)clamp_s32 (drag_start_x - selected_left, 0,
                                    selected_width)
                    * target_width)
                   / selected_width);
      x2 = target_left
           + (s32)(((s64)clamp_s32 (drag_cur_x - selected_left, 0,
                                    selected_width)
                    * target_width)
                   / selected_width);
    }

  draw_selection_box (x1, drag_start_y, x2, drag_cur_y, renderer);
}

void
operations_render_paused (PlatformState *p, RendererPlex *renderer)
{
  ZoomRect cur;
  RendererPlex split_source;

  if (!clean_frame_buffer || !renderer || !p
      || (!needs_paused_redraw && !is_dragging))
    return;

  cur = get_current_zoom (
      split_screen_is_enabled () ? split_screen_get_selected_pane () : 0);
  memcpy (overlay_frame_buffer, clean_frame_buffer,
          (u64)renderer->w * renderer->h * sizeof (u32));
  if (show_data_overlay[split_screen_is_enabled ()
                            ? split_screen_get_selected_pane ()
                            : 0])
    {
      RendererPlex source = { renderer->w, renderer->h, overlay_frame_buffer };
      renderer_draw_motion_vectors (
          current_motion_vectors, current_motion_vector_count, &source, NULL);
    }
  scale_image_region (overlay_frame_buffer, renderer->w, renderer->h, cur.x,
                      cur.y, cur.w, cur.h, renderer->image_buffer, renderer->w,
                      renderer->h, NULL);
  if (split_screen_is_enabled ())
    {
      memcpy (overlay_frame_buffer, renderer->image_buffer,
              (u64)renderer->w * renderer->h * sizeof (u32));
      split_source
          = (RendererPlex){ renderer->w, renderer->h, overlay_frame_buffer };
      split_screen_compose_pane (renderer, &split_source, 0);
      split_screen_compose_pane (renderer, &split_source, 1);
    }

  if (is_dragging)
    draw_selection_box (drag_start_x, drag_start_y, drag_cur_x, drag_cur_y,
                        renderer);

  platform_present (p);
  needs_paused_redraw = false;
}

bool
operations_take_paused_redraw_request (void)
{
  bool requested;

  requested = needs_paused_redraw || is_dragging;
  needs_paused_redraw = false;

  return requested;
}

bool
operations_take_split_selection_redraw_request (void)
{
  bool requested;

  requested = split_selection_redraw_pending;
  split_selection_redraw_pending = false;

  return requested;
}

// Keep the inspector implementation in one translation unit
#include "split_screen.c"
#include "timeline.c"
#include "zoom.c"
