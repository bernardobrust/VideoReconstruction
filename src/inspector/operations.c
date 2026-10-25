#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basic.h"
#include "dyn_arr.h"
#include "input.h"
#include "performance.h"
#include "platform.h"
#include "renderer.h"

// Playback controls (Fixed 1/8 step)
f64 playback_speed = 1.0, speed_step = 0.125;
bool paused = false, fullscreen = false, slow_down, speed_up;

typedef struct
{
    s32 x, y, w, h;
} ZoomRect;

typedef struct
{
    ZoomRect *items;
    s32 len, cap;
} ZoomStack;

enum
{
    MAX_ZOOM_LEVELS = 256
};

ZoomStack zoom_stack = { 0 };
u32 *clean_frame_buffer = NULL;
s32 *scale_x_map = NULL;
RendererPlex *current_rp = NULL;

bool is_dragging = false, needs_paused_redraw = true;
s32 drag_start_x = 0, drag_start_y = 0, drag_cur_x = 0, drag_cur_y = 0;

const u32 fill_color
= 1684327679; // precomputed from rgba (100, 180, 255, 100)

local void
add_array_size (u64 *total, u64 count, u64 element_size)
{
    assert (element_size > 0);
    assert (count <= (UINT64_MAX - *total) / element_size);
    *total += count * element_size;
}

u64
operations_memory_required (s32 w, s32 h)
{
    assert (w > 0 && h > 0);
    assert ((u64)w <= UINT64_MAX / (u64)h / sizeof (u32));
    
    u64 pixels = (u64)w * (u64)h, total = 128;
    add_array_size (&total, 1, sizeof (RendererPlex));
    add_array_size (&total, pixels * 2, sizeof (u32));
    add_array_size (&total, (u64)w, sizeof (s32));
    add_array_size (&total, MAX_ZOOM_LEVELS, sizeof (ZoomRect));
    
    return total;
}

local void
push_zoom (ZoomRect rect)
{
    if (zoom_stack.len >= zoom_stack.cap)
    {
        assert (zoom_stack.len > 1);
        
        memmove (zoom_stack.items + 1, zoom_stack.items + 2,
                 (zoom_stack.len - 2) * sizeof (*zoom_stack.items));
        --zoom_stack.len;
    }
    zoom_stack.items[zoom_stack.len++] = rect;
}

inline local ZoomRect
get_current_zoom (void)
{
    if (zoom_stack.len > 0)
        return zoom_stack.items[zoom_stack.len - 1];
    
    ZoomRect def = { 0, 0, current_rp ? current_rp->w : 0,
        current_rp ? current_rp->h : 0 };
    
    return def;
}

local void
scale_image_region (u32 *src, s32 src_w, s32 src_h, s32 zx, s32 zy, s32 zw,
                    s32 zh, u32 *dst, s32 dst_w, s32 dst_h)
{
    // 1:1 unzoomed fast path
    if (unlikely (zx == 0 && zy == 0 && zw == dst_w && zh == dst_h
                  && src_w == dst_w && src_h == dst_h))
    {
        memcpy (dst, src, dst_w * dst_h * sizeof (u32));
        return;
    }
    
    // We could clamp, but this seams faster (I didn't bechmark yet)
    if (zx < 0)
        zx = 0;
    if (zy < 0)
        zy = 0;
    if (zw < 1)
        zw = 1;
    if (zh < 1)
        zh = 1;
    if (zx + zw > src_w)
        zw = src_w - zx;
    if (zy + zh > src_h)
        zh = src_h - zy;
    
    // Precompute 1D horizontal map to remove division and multiplication in the
    // inner row loop
    for (s32 x = 0; x < dst_w; ++x)
    {
        s32 sx = zx + (s32)(((s64)x * zw) / dst_w);
        if (sx >= src_w)
            sx = src_w - 1;
        scale_x_map[x] = sx;
    }
    
    for (s32 y = 0; y < dst_h; ++y)
    {
        s32 sy = zy + (s32)(((s64)y * zh) / dst_h);
        if (sy >= src_h)
            sy = src_h - 1;
        
        u32 *src_row = src + sy * src_w, *dst_row = dst + y * dst_w;
        
        for (s32 x = 0; x < dst_w; ++x)
            dst_row[x] = src_row[scale_x_map[x]];
    }
}

inline local void
draw_selection_box (s32 x1, s32 y1, s32 x2, s32 y2, RendererPlex *rp)
{
    s32 min_x = MIN (x1, x2), max_x = MAX (x1, x2);
    s32 min_y = MIN (y1, y2), max_y = MAX (y1, y2);
    
    draw_rectangle_t (min_x, min_y, max_x, max_y, fill_color, rp);
}

void
operations_init (RendererPlex *rp, Arena *arena)
{
    assert (rp != NULL && arena != NULL);
    current_rp = rp;
    
    u64 buffer_size = (u64)rp->w * (u64)rp->h * sizeof (*clean_frame_buffer);
    clean_frame_buffer = arena_alloc (arena, buffer_size, _Alignof (u32));
    scale_x_map = arena_alloc (arena, (u64)rp->w * sizeof (*scale_x_map),
                               _Alignof (s32));
    zoom_stack.cap = MAX_ZOOM_LEVELS;
    zoom_stack.items
        = arena_alloc (arena, (u64)zoom_stack.cap * sizeof (*zoom_stack.items),
                       _Alignof (ZoomRect));
    zoom_stack.len = 0;
    assert (clean_frame_buffer != NULL && scale_x_map != NULL
            && zoom_stack.items != NULL);
    
    ZoomRect base_rect = { 0, 0, rp->w, rp->h };
    push_zoom (base_rect);
    
    is_dragging = false;
    needs_paused_redraw = true;
}

void
operations_on_frame_decoded (RendererPlex *rp)
{
    if (!clean_frame_buffer || !rp)
        return;
    
    memcpy (clean_frame_buffer, rp->image_buffer, rp->w * rp->h * sizeof (u32));
}

void
operations_apply_zoom (RendererPlex *rp)
{
    if (!clean_frame_buffer || !rp)
        return;
    
    operations_on_frame_decoded (rp);
    
    ZoomRect cur = get_current_zoom ();
    scale_image_region (clean_frame_buffer, rp->w, rp->h, cur.x, cur.y, cur.w,
                        cur.h, rp->image_buffer, rp->w, rp->h);
    
    if (is_dragging)
        draw_selection_box (drag_start_x, drag_start_y, drag_cur_x, drag_cur_y,
                            rp);
}

void
operations_render_paused (PlatformState *p, RendererPlex *rp)
{
    if (!clean_frame_buffer || !rp || !p
        || (!needs_paused_redraw && !is_dragging))
        return;
    
    ZoomRect cur = get_current_zoom ();
    scale_image_region (clean_frame_buffer, rp->w, rp->h, cur.x, cur.y, cur.w,
                        cur.h, rp->image_buffer, rp->w, rp->h);
    
    if (is_dragging)
        draw_selection_box (drag_start_x, drag_start_y, drag_cur_x, drag_cur_y,
                            rp);
    
    platform_present (p);
    needs_paused_redraw = false;
}

void
process_input (PlatformState *p)
{
    if (input_is_key_pressed (ESC))
        platform_stop (p);
    
    if (input_is_key_just_pressed (F))
    {
        fullscreen = !fullscreen;
        platform_set_fullscreen (p, fullscreen);
    }
    
    // Speed up and slow down
    speed_up = input_is_key_just_pressed (UP);
    if (speed_up)
    {
        playback_speed += playback_speed >= 1.0 ? 0.0 : speed_step;
#ifdef DEBUG
        printf ("Playback speed increased to: %f\n", playback_speed);
#endif
    }
    
    slow_down = input_is_key_just_pressed (DOWN);
    if (slow_down)
    {
        playback_speed -= playback_speed <= speed_step ? 0 : speed_step;
#ifndef NDEBUG
        printf ("Playback speed decreased to: %f\n", playback_speed);
#endif
    }
    
    // Toggle pause
    if (input_is_key_just_pressed (SPACE))
    {
        paused = !paused;
        needs_paused_redraw = true;
    }
    
    // Zoom in/out
    s32 mx = 0, my = 0;
    input_get_mouse_pos (&mx, &my);
    bool left_pressed = input_is_mouse_button_pressed (MOUSE_LEFT);
    bool left_just_released = input_is_mouse_button_just_released (MOUSE_LEFT);
    
    // Right Click: undo last zoom level
    if (input_is_mouse_button_just_pressed (MOUSE_RIGHT))
    {
        if (is_dragging)
        {
            is_dragging = false;
            needs_paused_redraw = true;
        }
        else if (zoom_stack.len > 1)
        {
            --zoom_stack.len;
#ifndef NDEBUG
            ZoomRect cur = get_current_zoom ();
            printf ("Zoom out (undo): [%d, %d, %dx%d] (zoom level: %d)\n", cur.x,
                    cur.y, cur.w, cur.h, (s32)zoom_stack.len - 1);
#endif
            needs_paused_redraw = true;
        }
    }
    
    // Left Click: start drawing transparent rectangle
    if (input_is_mouse_button_just_pressed (MOUSE_LEFT))
    {
        is_dragging = true;
        drag_start_x = mx;
        drag_start_y = my;
        drag_cur_x = mx;
        drag_cur_y = my;
        needs_paused_redraw = true;
    }
    
    // Mouse Movement while dragging
    if (is_dragging && left_pressed)
    {
        if (mx != drag_cur_x || my != drag_cur_y)
        {
            drag_cur_x = mx;
            drag_cur_y = my;
            needs_paused_redraw = true;
        }
    }
    
    // Release Left Click: zoom into covered area
    if (is_dragging && (!left_pressed || left_just_released))
    {
        is_dragging = false;
        needs_paused_redraw = true;
        
        if (current_rp)
        {
            s32 sx1 = MIN (drag_start_x, drag_cur_x),
            sx2 = MAX (drag_start_x, drag_cur_x),
            sy1 = MIN (drag_start_y, drag_cur_y),
            sy2 = MAX (drag_start_y, drag_cur_y);
            
            sx1 = clamp_s32 (sx1, 0, current_rp->w),
            sx2 = clamp_s32 (sx2, 0, current_rp->w),
            sy1 = clamp_s32 (sy1, 0, current_rp->h),
            sy2 = clamp_s32 (sy2, 0, current_rp->h);
            
            s32 sel_w = sx2 - sx1, sel_h = sy2 - sy1;
            
            // Only zoom if the area selected is at least 8x8 (smallest AV1
            // block)
            if (likely (sel_w >= 8 && sel_h >= 8))
            {
                ZoomRect cur = get_current_zoom ();
                
                s32 new_x1 = cur.x + (s32)(((s64)sx1 * cur.w) / current_rp->w),
                new_y1 = cur.y + (s32)(((s64)sy1 * cur.h) / current_rp->h),
                new_x2 = cur.x + (s32)(((s64)sx2 * cur.w) / current_rp->w),
                new_y2 = cur.y + (s32)(((s64)sy2 * cur.h) / current_rp->h);
                
                // Integer mapping can collapse a small screen selection to
                // zero source pixels after a few zoom levels. Keep the crop
                // valid so later zooms do not get stuck on a zero-sized rect.
                s32 new_w = MAX (1, new_x2 - new_x1),
                new_h = MAX (1, new_y2 - new_y1);
                
                // Clamp to video dimensions
                if (new_w > current_rp->w)
                    new_w = current_rp->w;
                if (new_h > current_rp->h)
                    new_h = current_rp->h;
                
                if (new_x1 < 0)
                    new_x1 = 0;
                if (new_y1 < 0)
                    new_y1 = 0;
                if (new_x1 + new_w > current_rp->w)
                    new_x1 = current_rp->w - new_w;
                if (new_y1 + new_h > current_rp->h)
                    new_y1 = current_rp->h - new_h;
                
                if (likely (new_x1 != cur.x || new_y1 != cur.y || new_w != cur.w
                            || new_h != cur.h))
                {
                    ZoomRect next_zoom = { new_x1, new_y1, new_w, new_h };
                    push_zoom (next_zoom);
#ifndef NDEBUG
                    printf ("Zoom in: [%d, %d, %dx%d] (zoom level: %d)\n",
                            new_x1, new_y1, new_w, new_h,
                            (s32)zoom_stack.len - 1);
#endif
                }
            }
        }
    }
}
