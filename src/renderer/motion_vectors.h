#pragma once

#include <libavutil/motion_vector.h>

#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"

void renderer_motion_vectors_set_display_resolution (ScreenResolution resolution);
void renderer_draw_motion_vectors (const AVMotionVector *motion_vectors,
                                   s32 count, RendererPlex *rp,
                                   RendererPool *renderer_pool);
