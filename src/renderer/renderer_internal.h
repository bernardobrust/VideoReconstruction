#pragma once

#include "renderer.h"

typedef void (*RendererTriangleRowFn) (u32 *pixels, s32 count, u32 color,
                                       s32 edge_value_0, s32 edge_value_1,
                                       s32 edge_value_2, s32 edge_step_x_0,
                                       s32 edge_step_x_1, s32 edge_step_x_2,
                                       bool transparent);

// Select the best supported backend once, then return the row rasterizer
void renderer_backend_init (void);
RendererTriangleRowFn renderer_get_triangle_row_renderer (s32 count);

// Rasterize one triangle row with scalar or SIMD coverage tests
void renderer_triangle_row_sse2 (u32 *pixels, s32 count, u32 color,
                                 s32 edge_value_0, s32 edge_value_1,
                                 s32 edge_value_2, s32 edge_step_x_0,
                                 s32 edge_step_x_1, s32 edge_step_x_2,
                                 bool transparent);
void renderer_triangle_row_avx2 (u32 *pixels, s32 count, u32 color,
                                 s32 edge_value_0, s32 edge_value_1,
                                 s32 edge_value_2, s32 edge_step_x_0,
                                 s32 edge_step_x_1, s32 edge_step_x_2,
                                 bool transparent);

// Blend or fill a clipped rectangle using the selected backend
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
