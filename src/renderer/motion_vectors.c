#include "motion_vectors.h"

#include "basic.h"
#include <assert.h>
#include <libavutil/motion_vector.h>
#include <math.h>
#include <stdio.h>

// We may want to calculate this based on the display's dimensions
const f32 MAX_MOTION_MAGNITUDE = 90.0f;
f32 MAX_DISPLAY_LENGTH = 15.0f;
void
renderer_motion_vectors_set_display_resolution (ScreenResolution resolution)
{
  if (resolution.width > 0 && resolution.height > 0)
    MAX_DISPLAY_LENGTH
        = (f32)(resolution.width < resolution.height ? resolution.width
                                                     : resolution.height)
          / 48.0f;
}

typedef struct
{
  const AVMotionVector *mvs;
  RendererPlex *frame_renderer;
  s32 count;
} MotionVectorRenderContext;

// Each worker renders a disjoint set of rows, while iterating motion vectors
// in the same order. This preserves the sequential result where overlays meet
local void
process_motion_vector (const AVMotionVector *mv, RendererPlex *renderer,
                       s32 row_offset)
{
  const s32 thickness = 2, arrow_margin = 4;
  s32 cx, cy, startx, starty, raw_endx, raw_endy, arrow_min_y, arrow_max_y,
      endx, endy;
  f32 dx, dy, raw_len, normalized, motion_factor, display_len, scale;
  u32 color, block_color, arrow_color;
  bool block_intersects, arrow_intersects;

  cx = (s32)(mv->w / 2);
  cy = (s32)(mv->h / 2);

  if (mv->source > 0)
    {
      startx = (s32)mv->dst_x + cx;
      starty = (s32)mv->dst_y + cy;
      raw_endx = (s32)mv->src_x + cx;
      raw_endy = (s32)mv->src_y + cy;
    }
  else
    {
      startx = (s32)mv->src_x + cx;
      starty = (s32)mv->src_y + cy;
      raw_endx = (s32)mv->dst_x + cx;
      raw_endy = (s32)mv->dst_y + cy;
    }

  // The arrow head and shaft extend only a few pixels beyond the vector line.
  // This conservative bound avoids repeating geometry work on other row
  // partitions while still allowing an arrow to cross a partition boundary
  arrow_min_y = MIN (starty, raw_endy) - arrow_margin;
  arrow_max_y = MAX (starty, raw_endy) + arrow_margin;
  block_intersects = (s32)mv->dst_y < row_offset + renderer->h
                     && (s32)mv->dst_y + (s32)mv->h > row_offset;
  arrow_intersects
      = arrow_max_y >= row_offset && arrow_min_y < row_offset + renderer->h;
  if (!block_intersects && !arrow_intersects)
    return;

  dx = (f32)(raw_endx - startx);
  dy = (f32)(raw_endy - starty);
  starty -= row_offset;
  raw_len = hypot_f32 (dx, dy);
  normalized = clamp_f32 (raw_len / MAX_MOTION_MAGNITUDE, 0.0f, 1.0f);
  motion_factor = sqrtf (normalized);
  color = interpolate_color_br (motion_factor);
  block_color = (color & 0x00FFFFFF) | (48u << 24);
  arrow_color = (color & 0x00FFFFFF) | (104u << 24);

  if (block_intersects)
    draw_rectangle_t ((s32)mv->dst_x, (s32)mv->dst_y - row_offset,
                      (s32)mv->dst_x + (s32)mv->w - 1,
                      (s32)mv->dst_y + (s32)mv->h - 1 - row_offset,
                      block_color, renderer);

  if (raw_len == 0.0f)
    return;

  display_len = MIN (raw_len, MAX_DISPLAY_LENGTH);
  scale = display_len / raw_len;
  endx = startx + (s32)roundf (dx * scale);
  endy = starty + (s32)roundf (dy * scale);
  draw_arrow_t (startx, starty, endx, endy, thickness, arrow_color, renderer);
}

local void
process_motion_vector_rows (void *arg, s32 first_row, s32 end_row)
{
  MotionVectorRenderContext *context;
  RendererPlex row_renderer;
  s32 i;

  context = arg;
  assert (context != NULL && first_row >= 0
          && end_row <= context->frame_renderer->h);

  if (first_row >= end_row)
    return;

  row_renderer.w = context->frame_renderer->w;
  row_renderer.h = end_row - first_row;
  row_renderer.image_buffer
      = context->frame_renderer->image_buffer + first_row * row_renderer.w;

  for (i = 0; i < context->count; ++i)
    process_motion_vector (&context->mvs[i], &row_renderer, first_row);
}

void
renderer_draw_motion_vectors (const AVMotionVector *motion_vectors, s32 count,
                              RendererPlex *renderer,
                              RendererPool *renderer_pool)
{
  MotionVectorRenderContext context;

  assert (motion_vectors != NULL || count == 0);
  assert (renderer != NULL && count >= 0);
  context.mvs = motion_vectors;
  context.frame_renderer = renderer;
  context.count = count;

  if (count > 0)
    {
      if (renderer_pool != NULL)
        renderer_pool_run_rows (renderer_pool, renderer->h,
                                process_motion_vector_rows, &context);
      else
        process_motion_vector_rows (&context, 0, renderer->h);
    }
}
