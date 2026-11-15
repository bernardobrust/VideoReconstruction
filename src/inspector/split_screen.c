// Static here is prefered over local due to problems with inlining in MSVC
static bool split_screen_enabled = false;

local void
split_screen_toggle (void)
{
  split_screen_enabled = !split_screen_enabled;
}

local bool
split_screen_is_enabled (void)
{
  return split_screen_enabled;
}

local void
split_screen_apply (RendererPlex *renderer, u32 *scratch)
{
  s32 x, y, half_w, panel_x, source_x;

  assert (renderer != NULL && scratch != NULL);

  half_w = renderer->w / 2;
  if (half_w == 0)
    return;

  memcpy (scratch, renderer->image_buffer,
          (u64)renderer->w * renderer->h * sizeof (u32));

  // Map each panel across the same full-frame source
  for (y = 0; y < renderer->h; ++y)
    for (x = 0; x < renderer->w; ++x)
      {
        panel_x = x < half_w ? x : x - half_w;
        source_x = (s32)(((s64)panel_x * renderer->w) / half_w);

        if (source_x >= renderer->w)
          source_x = renderer->w - 1;

        renderer->image_buffer[(u64)y * renderer->w + x]
            = scratch[(u64)y * renderer->w + source_x];
      }
}
