#pragma once

#include "renderer.h"

typedef void (*RendererTriangleRowFn) (u32 *pixels, s32 count, u32 color,
                                       s32 w0, s32 w1, s32 w2, s32 a0, s32 a1,
                                       s32 a2, bool transparent);

RendererTriangleRowFn renderer_get_triangle_row_renderer (s32 count);

void renderer_triangle_row_sse2 (u32 *pixels, s32 count, u32 color, s32 w0,
                                 s32 w1, s32 w2, s32 a0, s32 a1, s32 a2,
                                 bool transparent);
void renderer_triangle_row_avx2 (u32 *pixels, s32 count, u32 color, s32 w0,
                                 s32 w1, s32 w2, s32 a0, s32 a1, s32 a2,
                                 bool transparent);

void renderer_blend_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                               u32 color);
void renderer_blend_rectangle_sse2 (u32 *pixels, s32 stride, s32 width,
                                    s32 height, u32 color);
void renderer_blend_rectangle_avx2 (u32 *pixels, s32 stride, s32 width,
                                    s32 height, u32 color);

void renderer_fill_rectangle (u32 *pixels, s32 stride, s32 width, s32 height,
                              u32 color);
void renderer_fill_rectangle_sse2 (u32 *pixels, s32 stride, s32 width,
                                   s32 height, u32 color);
void renderer_fill_rectangle_avx2 (u32 *pixels, s32 stride, s32 width,
                                   s32 height, u32 color);
