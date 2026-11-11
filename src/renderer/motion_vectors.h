#pragma once

#include <libavutil/motion_vector.h>

#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"

// Set display dimensions used to scale motion-vector drawing density
void
renderer_motion_vectors_set_display_resolution (ScreenResolution resolution);
// Draw codec motion vectors over a frame, optionally using the row worker pool
void renderer_draw_motion_vectors (const AVMotionVector *motion_vectors,
                                   s32 count, RendererPlex *renderer,
                                   RendererPool *renderer_pool);
