#pragma once

#include "arena.h"
#include "renderer.h"
#include "thread.h"
#include <libavutil/motion_vector.h>

typedef struct
{
  RendererPlex *frames;
  s32 *ready_indices;
  s32 *history_indices;
  bool *occupied, *history_pinned;
  bool *history_reading;
  AVMotionVector **motion_vectors;
  s32 *motion_vector_counts, *motion_vector_capacities;
  s64 *frame_numbers;
  s32 capacity, ready_head, ready_count, history_head, history_count,
      history_cursor, history_capacity, future_capacity, w, h, terminal_result;
  u64 generation;
  bool stopped, producer_finished, producer_wakeup;
  PlatformMutex mutex;
  PlatformCondition frame_ready, slot_available;
} FrameQueue;

typedef struct
{
  s32 index;
  RendererPlex *renderer;
  AVMotionVector *motion_vectors;
  s32 motion_vector_count;
  s64 frame_number;
  u64 generation;
  bool was_taken, from_history;
} FrameQueueFrame;

u64 frame_queue_memory_required (s32 w, s32 h, s32 capacity);
s32 frame_queue_capacity_for_bytes (s32 w, s32 h, u64 byte_budget);

void frame_queue_init (FrameQueue *queue, Arena *arena, s32 w, s32 h,
                       s32 capacity);
void frame_queue_destroy (FrameQueue *queue);

bool frame_queue_acquire_write (FrameQueue *queue, FrameQueueFrame *frame);
void frame_queue_copy_motion_vectors (FrameQueue *queue,
                                      FrameQueueFrame *frame,
                                      const AVMotionVector *motion_vectors,
                                      s32 count);
void frame_queue_publish (FrameQueue *queue, FrameQueueFrame frame);
void frame_queue_publish_history (FrameQueue *queue, FrameQueueFrame frame);
void frame_queue_set_frame_number (FrameQueue *queue, FrameQueueFrame *frame,
                                   s64 frame_number);
void frame_queue_clear (FrameQueue *queue);
void frame_queue_wake_producer (FrameQueue *queue);
bool frame_queue_take (FrameQueue *queue, FrameQueueFrame *frame);
bool frame_queue_history_move (FrameQueue *queue, s32 direction,
                               FrameQueueFrame *frame);
void frame_queue_history_release (FrameQueue *queue, FrameQueueFrame frame);
void frame_queue_requeue_front (FrameQueue *queue, FrameQueueFrame frame);
void frame_queue_release (FrameQueue *queue, FrameQueueFrame frame);
void frame_queue_finish (FrameQueue *queue, s32 result);
void frame_queue_stop (FrameQueue *queue);
bool frame_queue_should_stop (FrameQueue *queue);
s32 frame_queue_result (FrameQueue *queue);
