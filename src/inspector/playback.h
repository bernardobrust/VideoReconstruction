#pragma once

#include "decode.h"
#include "frame_queue.h"
#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"
#include "thread.h"

typedef struct
{
  VideoPlex *video;
  RendererPlex *display;
  RendererPlex *render_target;
  FrameQueue decoded_frames;
  RendererPool renderer_pool;
  PlatformThread decoder_thread, systems_thread;
  PlatformMutex state_mutex;
  PlatformCondition state_changed;
  f64 frame_duration_ms;
  s32 result;
  bool stopping, render_in_progress, has_frame, frame_ready, done;
} InspectorPlayback;

// state_mutex protects inspector operation globals and frame ownership flags.
// render_in_progress/frame_ready prevent the main thread from reading a buffer
// while the systems thread writes it. Only the main thread calls the platform
bool inspector_playback_init (InspectorPlayback *playback, Arena *arena,
                              VideoPlex *video, RendererPlex *display,
                              s32 cache_capacity, s32 renderer_workers,
                              f64 frame_duration_ms);
void inspector_playback_shutdown (InspectorPlayback *playback);

void inspector_playback_process_input (InspectorPlayback *playback,
                                       PlatformState *platform_state);
bool inspector_playback_update (InspectorPlayback *playback,
                                PlatformState *platform_state);
s32 inspector_playback_result (InspectorPlayback *playback);
void inspector_playback_stop (InspectorPlayback *playback);
