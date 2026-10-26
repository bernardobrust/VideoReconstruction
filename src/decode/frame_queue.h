#pragma once

#include "arena.h"
#include "renderer.h"
#include "thread.h"

typedef struct
{
  RendererPlex *frames;
  s32 *ready_indices;
  bool *occupied;
  s32 capacity, ready_head, ready_count, w, h, terminal_result;
  bool stopped, producer_finished;
  PlatformMutex mutex;
  PlatformCondition frame_ready, slot_available;
} FrameQueue;

typedef struct
{
  s32 index;
  RendererPlex *renderer;
} FrameQueueFrame;

u64 frame_queue_memory_required (s32 w, s32 h, s32 capacity);

void frame_queue_init (FrameQueue *queue, Arena *arena, s32 w, s32 h,
                       s32 capacity);
void frame_queue_destroy (FrameQueue *queue);

bool frame_queue_acquire_write (FrameQueue *queue, FrameQueueFrame *frame);
void frame_queue_publish (FrameQueue *queue, FrameQueueFrame frame);
bool frame_queue_take (FrameQueue *queue, FrameQueueFrame *frame);
void frame_queue_release (FrameQueue *queue, FrameQueueFrame frame);
void frame_queue_finish (FrameQueue *queue, s32 result);
void frame_queue_stop (FrameQueue *queue);
bool frame_queue_should_stop (FrameQueue *queue);
s32 frame_queue_result (FrameQueue *queue);
