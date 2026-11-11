#pragma once

#include "arena.h"
#include "macros.h"
#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"
#include <libavutil/motion_vector.h>

extern f64 playback_speed;
extern bool paused;

typedef struct
{
  // Source-image crop used by the render worker
  s32 zoom_x, zoom_y, zoom_w, zoom_h;
  // Selection rectangle in display coordinates
  s32 drag_start_x, drag_start_y, drag_cur_x, drag_cur_y;
  bool is_dragging, show_data_overlay;
} OperationsRenderState;

// Copy operation state under the inspector state lock before worker rendering
// so input can continue updating the live state during a frame.

// Return the arena capacity needed for operation buffers at the given size
u64 operations_memory_required (s32 w, s32 h);

// Initialize operation buffers and the base zoom level
void operations_init (RendererPlex *renderer, Arena *arena);
void operations_set_fullscreen (bool enabled);

// Save a decoded frame and its motion vectors for rendering
void operations_on_frame_decoded (RendererPlex *renderer,
                                  const AVMotionVector *motion_vectors,
                                  s32 motion_vector_count);
// Copy mutable operation state while the caller holds the inspector state lock
void operations_get_render_state (OperationsRenderState *state);
// Render a frame using the captured crop and overlay settings
void operations_apply_zoom (RendererPlex *renderer, RendererPool *pool,
                            const OperationsRenderState *state);
// Redraw and present the current frame when paused state changes
void operations_render_paused (PlatformState *platform_state,
                               RendererPlex *renderer);

// Update zoom, overlay, playback, and timeline controls from current input
void process_input (PlatformState *platform_state);
// Update the timeline from input and report whether its state changed
bool timeline_process_input (void);
// Pause playback without clearing queued frame-step requests
void timeline_pause (void);
// Consume and clear accumulated frame-step requests
s32 timeline_take_frame_step_request (void);
