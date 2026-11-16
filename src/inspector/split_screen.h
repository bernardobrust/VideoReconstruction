#pragma once

#include "playback.h"

#include <stdbool.h>

bool split_screen_is_enabled (void);
void split_screen_toggle (void);
bool split_screen_process_input (void);
void split_screen_initialize_frames (s64 frame_number);
void split_screen_reset_frames (s64 frame_number);
void split_screen_request_all (void);
bool split_screen_advance_frames (void);
s64 split_screen_get_frame_number (s32 pane);
bool split_screen_get_pending_frame (s32 pane, s64 *frame_number,
                                     u64 *generation);
void split_screen_mark_rendered (s32 pane, s64 frame_number, u64 generation);
void split_screen_cancel_future_frame (s32 pane, s64 frame_number);
s32 split_screen_get_selected_pane (void);
void split_screen_compose_pane (RendererPlex *display,
                                const RendererPlex *source, s32 pane);
s32 split_screen_update_playback (InspectorPlayback *playback,
                                  f64 *next_frame);
