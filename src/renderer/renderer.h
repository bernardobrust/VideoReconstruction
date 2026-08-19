#pragma once

#include "platform.h"

typedef struct
{
  int w, h;
  unsigned *image_buffer;
} RendererPlex;

RendererPlex init_renderer (int w, int h);

unsigned rgba (unsigned r, unsigned g, unsigned b, unsigned a);

unsigned *init_image_buffer (int w, int h);

void draw_hline (int x0, int x1, int y, unsigned color, RendererPlex rp);

void draw_triangle (int x1, int y1, int x2, int y2, int x3, int y3,
                    unsigned color, RendererPlex rp);
void draw_rectangle (int x1, int y1, int x2, int y2, unsigned color,
                     RendererPlex rp);
void draw_circle (int cx, int cy, int r, unsigned color, RendererPlex rp);

void draw_rotated_rectangle (int cx, int cy, int w, int h, float theta,
                             unsigned color, RendererPlex rp);
void draw_rotated_oriented_rectangle (int dx1, int dy1, int dx2, int dy2,
                                      int width, unsigned color,
                                      RendererPlex rp);
void draw_arrow (int startx, int starty, int endx, int endy, int thickness,
                 unsigned color, RendererPlex rp);

void renderer_present (PlatformState *platform_state, RendererPlex rp);