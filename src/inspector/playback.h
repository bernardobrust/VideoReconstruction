#pragma once

#include "decode.h"
#include "frame_queue.h"
#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"
#include "thread.h"

typedef struct
{
  struct InspectorPlayback *playback;
  VideoPlex *video;
  PlatformThread thread;
  s64 next_frame_number, requested_frame_number;
  u64 requested_generation;
  s32 pane, result;
  bool request_pending, busy;
} SplitDecoder;

typedef struct InspectorPlayback
{
  // Video source and buffers used for published and in-progress frames
  VideoPlex *video;
  RendererPlex *display;
  RendererPlex *render_target;
  RendererPlex *split_render_targets[2];
  FrameQueue decoded_frames;
  RendererPool renderer_pool;
  SplitDecoder split_decoders[2];
  Arena *arena;

  // Worker threads and synchronization shared with the UI thread
  PlatformThread decoder_thread, systems_thread;
  PlatformMutex state_mutex;
  PlatformCondition state_changed;

  // Playback timing, seek/step requests, and current frame status
  f64 frame_duration_ms;
  s32 result, frame_step_request, split_decoder_threads;
  s64 seek_frame_request, current_frame_number;
  bool seek_in_progress, seek_frame_ready;
  bool stopping, render_in_progress, has_frame, frame_ready, done;
  bool split_decoders_initialized, split_decoders_init_attempted;
} InspectorPlayback;

// state_mutex protects shared playback state and inspector operation globals.
// render_in_progress/frame_ready coordinate buffer publication with the UI.
// Only the main thread calls platform presentation functions.
// Start the decoder and systems threads and initialize playback resources.
void inspector_playback_init (InspectorPlayback *playback, Arena *arena,
                              VideoPlex *video, RendererPlex *display,
                              s32 cache_capacity, s32 renderer_workers,
                              f64 frame_duration_ms);
// Stop both threads and release playback synchronization resources.
void inspector_playback_shutdown (InspectorPlayback *playback);

// Apply current input to playback state and signal waiting worker threads.
void inspector_playback_process_input (InspectorPlayback *playback,
                                       PlatformState *platform_state);
// Present a newly rendered frame or refresh the paused view when needed.
bool inspector_playback_update (InspectorPlayback *playback,
                                PlatformState *platform_state);
// Return the decoder or systems-thread result.
s32 inspector_playback_result (InspectorPlayback *playback);
// Request shutdown and wake any waiting worker threads.
void inspector_playback_stop (InspectorPlayback *playback);
