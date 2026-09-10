#pragma once

#include <stdint.h>

#include "platform.h"
#include "macros.h"

//typedef struct
typedef struct {
  s32 w, h;
  u32 *image_buffer;
} RendererPlex;

RendererPlex *init_renderer (s32 w, s32 h);

// Utility
u32 rgba (u32 r, u32 g, u32 b, u32 a);
u8 blend_channel (u8 src, u8 dst, u8 alpha);
u32 blend_rgba_pixel (u32 src, u32 dst);

void draw_hline (s32 x0, s32 x1, s32 y, u32 color, RendererPlex *rp);

// Normal shapes
void draw_triangle (s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3,
                    u32 color, RendererPlex *rp);
void draw_rectangle (s32 x1, s32 y1, s32 x2, s32 y2, u32 color,
                     RendererPlex *rp);
void draw_circle (s32 cx, s32 cy, s32 r, u32 color, RendererPlex *rp);
void draw_rotated_rectangle (s32 cx, s32 cy, s32 w, s32 h, f32 theta,
                             u32 color, RendererPlex *rp);
void draw_rotated_oriented_rectangle (s32 dx1, s32 dy1, s32 dx2, s32 dy2,
                                      s32 width, u32 color,
                                      RendererPlex *rp);
void draw_arrow (s32 startx, s32 starty, s32 endx, s32 endy, s32 thickness,
                 u32 color, RendererPlex *rp);

// Transparent shapes
void draw_triangle_t (s32 x1, s32 y1, s32 x2, s32 y2, s32 x3, s32 y3,
                    u32 color, RendererPlex *rp);
void draw_rectangle_t (s32 x1, s32 y1, s32 x2, s32 y2, u32 color,
                     RendererPlex *rp);
void draw_circle_t (s32 cx, s32 cy, s32 r, u32 color, RendererPlex *rp);
void draw_rotated_rectangle_t (s32 cx, s32 cy, s32 w, s32 h, f32 theta,
                             u32 color, RendererPlex *rp);
void draw_rotated_oriented_rectangle_t (s32 dx1, s32 dy1, s32 dx2, s32 dy2,
                                      s32 width, u32 color,
                                      RendererPlex *rp);
void draw_arrow_t (s32 startx, s32 starty, s32 endx, s32 endy, s32 thickness,
                 u32 color, RendererPlex *rp);

void renderer_present (PlatformState *platform_state, RendererPlex *rp);