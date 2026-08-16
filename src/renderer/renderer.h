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

void renderer_present (PlatformState *platform_state, RendererPlex rp);