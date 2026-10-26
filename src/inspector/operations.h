#pragma once

#include "arena.h"
#include "macros.h"
#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"

extern f64 playback_speed;
extern bool paused;

typedef struct
{
  s32 zoom_x, zoom_y, zoom_w, zoom_h;
  s32 drag_start_x, drag_start_y, drag_cur_x, drag_cur_y;
  bool is_dragging;
} OperationsRenderState;

// Capture this under the inspector state lock. Worker rendering reads only the
// snapshot, so input can update the live operation state while a frame
// renders.

u64 operations_memory_required (s32 w, s32 h);

void operations_init (RendererPlex *rp, Arena *arena);

void operations_on_frame_decoded (RendererPlex *rp);
void operations_get_render_state (OperationsRenderState *state);
void operations_apply_zoom (RendererPlex *rp, RendererPool *pool,
                            const OperationsRenderState *state);
void operations_render_paused (PlatformState *platform_state,
                               RendererPlex *rp);
void process_input (PlatformState *platform_state);
