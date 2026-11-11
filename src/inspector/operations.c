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

ZoomStack zoom_stack = { 0 };
u32 *clean_frame_buffer = NULL;
u32 *overlay_frame_buffer = NULL;
s32 *scale_x_map = NULL;
RendererPlex *current_renderer = NULL;

bool is_dragging = false, is_panning = false, needs_paused_redraw = true;
bool show_data_overlay = true;
AVMotionVector *current_motion_vectors = NULL;
s32 current_motion_vector_count = 0, current_motion_vector_capacity = 0;
s32 drag_start_x = 0, drag_start_y = 0, drag_cur_x = 0, drag_cur_y = 0;
s32 pan_start_x = 0, pan_start_y = 0;
ZoomRect pan_start_zoom;

// The included zoom implementation uses these helpers defined later
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
  add_array_size (&total, MAX_ZOOM_LEVELS, sizeof (ZoomRect));

  return total;
}

local void
push_zoom (ZoomRect rect)
{
  if (zoom_stack.len >= zoom_stack.cap)
    {
      assert (zoom_stack.len > 1);

      memmove (zoom_stack.items + 1, zoom_stack.items + 2,
               (zoom_stack.len - 2) * sizeof (*zoom_stack.items));
      --zoom_stack.len;
    }

  zoom_stack.items[zoom_stack.len++] = rect;
}

local ZoomRect
get_current_zoom (void)
{
  if (zoom_stack.len > 0)
    return zoom_stack.items[zoom_stack.len - 1];

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
  zoom_stack.cap = MAX_ZOOM_LEVELS;
  zoom_stack.items
      = arena_alloc (arena, (u64)zoom_stack.cap * sizeof (*zoom_stack.items),
                     _Alignof (ZoomRect));
  zoom_stack.len = 0;

  assert (clean_frame_buffer != NULL && overlay_frame_buffer != NULL
          && scale_x_map != NULL && zoom_stack.items != NULL);

  ZoomRect base_rect = { 0, 0, renderer->w, renderer->h };
  push_zoom (base_rect);

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
operations_get_render_state (OperationsRenderState *state)
{
  assert (state != NULL);

  ZoomRect zoom;

  zoom = get_current_zoom ();
  state->zoom_x = zoom.x;
  state->zoom_y = zoom.y;
  state->zoom_w = zoom.w;
  state->zoom_h = zoom.h;
  state->is_dragging = is_dragging;
  state->show_data_overlay = show_data_overlay;
  state->drag_start_x = drag_start_x;
  state->drag_start_y = drag_start_y;
  state->drag_cur_x = drag_cur_x;
  state->drag_cur_y = drag_cur_y;
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
operations_render_paused (PlatformState *p, RendererPlex *renderer)
{
  if (!clean_frame_buffer || !renderer || !p
      || (!needs_paused_redraw && !is_dragging))
    return;

  ZoomRect cur = get_current_zoom ();
  memcpy (overlay_frame_buffer, clean_frame_buffer,
          (u64)renderer->w * renderer->h * sizeof (u32));
  if (show_data_overlay)
    {
      RendererPlex source = { renderer->w, renderer->h, overlay_frame_buffer };
      renderer_draw_motion_vectors (
          current_motion_vectors, current_motion_vector_count, &source, NULL);
    }
  scale_image_region (overlay_frame_buffer, renderer->w, renderer->h, cur.x,
                      cur.y, cur.w, cur.h, renderer->image_buffer, renderer->w,
                      renderer->h, NULL);

  if (is_dragging)
    draw_selection_box (drag_start_x, drag_start_y, drag_cur_x, drag_cur_y,
                        renderer);

  platform_present (p);
  needs_paused_redraw = false;
}

// Keep the inspector implementation in one translation unit
#include "timeline.c"
#include "zoom.c"
