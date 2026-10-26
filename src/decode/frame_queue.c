#include "frame_queue.h"

#include <assert.h>
#include <string.h>

u64
frame_queue_memory_required (s32 w, s32 h, s32 capacity)
{
  u64 metadata, frame_bytes;

  assert (w > 0 && h > 0 && capacity > 0);
  assert ((u64)w <= UINT64_MAX / (u64)h / sizeof (u32));

  metadata = 256 + (u64)capacity * sizeof (RendererPlex)
             + (u64)capacity * sizeof (s32) + (u64)capacity * sizeof (bool);
  frame_bytes = (u64)w * (u64)h * sizeof (u32);

  assert (metadata <= UINT64_MAX
          && frame_bytes <= (UINT64_MAX - metadata) / (u64)capacity);

  return metadata + (u64)capacity * frame_bytes;
}

void
frame_queue_init (FrameQueue *queue, Arena *arena, s32 w, s32 h, s32 capacity)
{
  assert (queue != NULL && arena != NULL && w > 0 && h > 0 && capacity > 0);

  s32 i;

  memset (queue, 0, sizeof (*queue));

  queue->w = w;
  queue->h = h;
  queue->capacity = capacity;
  queue->frames = arena_alloc (arena, (u64)capacity * sizeof (*queue->frames),
                               _Alignof (RendererPlex));
  queue->ready_indices = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->ready_indices), _Alignof (s32));
  queue->occupied = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->occupied), _Alignof (bool));

  assert (queue->frames != NULL && queue->ready_indices != NULL
          && queue->occupied != NULL);

  memset (queue->occupied, 0, (u64)capacity * sizeof (*queue->occupied));

  for (i = 0; i < capacity; ++i)
    {
      queue->frames[i].w = w;
      queue->frames[i].h = h;
      queue->frames[i].image_buffer
          = arena_alloc (arena, (u64)w * h * sizeof (u32), _Alignof (u32));

      assert (queue->frames[i].image_buffer != NULL);
    }

  platform_mutex_init (&queue->mutex);
  platform_condition_init (&queue->frame_ready);
  platform_condition_init (&queue->slot_available);
}

void
frame_queue_destroy (FrameQueue *queue)
{
  assert (queue != NULL);

  // Call only after the decoder and consumer threads have stopped.
  platform_condition_destroy (&queue->slot_available);
  platform_condition_destroy (&queue->frame_ready);
  platform_mutex_destroy (&queue->mutex);
}

bool
frame_queue_acquire_write (FrameQueue *queue, FrameQueueFrame *frame)
{
  s32 i;

  assert (queue != NULL && frame != NULL);
  platform_mutex_lock (&queue->mutex);
  for (;;)
    {
      if (queue->stopped || queue->producer_finished)
        {
          platform_mutex_unlock (&queue->mutex);

          return false;
        }
      for (i = 0; i < queue->capacity; ++i)
        {
          if (!queue->occupied[i])
            {
              // The occupied flag reserves this slot until the consumer
              // returns it. The producer may now write without holding the
              // queue mutex.
              queue->occupied[i] = true;
              frame->index = i;
              frame->renderer = &queue->frames[i];
              platform_mutex_unlock (&queue->mutex);

              return true;
            }
        }

      platform_condition_wait (&queue->slot_available, &queue->mutex);
    }
}

void
frame_queue_publish (FrameQueue *queue, FrameQueueFrame frame)
{
  s32 tail;

  assert (queue != NULL && frame.index >= 0 && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  // Lock/unlock publishes the producer's completed pixel writes to the
  // consumer, which takes this same mutex before it can see the slot index.
  assert (queue->occupied[frame.index]);
  assert (queue->ready_count < queue->capacity);
  tail = (queue->ready_head + queue->ready_count) % queue->capacity;

  queue->ready_indices[tail] = frame.index;
  ++queue->ready_count;
  platform_condition_signal (&queue->frame_ready);

  platform_mutex_unlock (&queue->mutex);
}

bool
frame_queue_take (FrameQueue *queue, FrameQueueFrame *frame)
{
  assert (queue != NULL && frame != NULL);

  platform_mutex_lock (&queue->mutex);

  while (queue->ready_count == 0 && !queue->producer_finished
         && !queue->stopped)
    platform_condition_wait (&queue->frame_ready, &queue->mutex);

  if (queue->ready_count == 0)
    {
      platform_mutex_unlock (&queue->mutex);

      return false;
    }

  // Ownership transfers to the consumer, but the slot remains occupied until
  // frame_queue_release, preventing the decoder from overwriting a live frame.
  frame->index = queue->ready_indices[queue->ready_head];
  frame->renderer = &queue->frames[frame->index];
  queue->ready_head = (queue->ready_head + 1) % queue->capacity;
  --queue->ready_count;

  platform_mutex_unlock (&queue->mutex);
  return true;
}

void
frame_queue_release (FrameQueue *queue, FrameQueueFrame frame)
{
  assert (queue != NULL && frame.index >= 0 && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  // The decoder cannot reserve this slot again until this release is visible
  // under the queue mutex, so it cannot overwrite pixels still being read.
  assert (queue->occupied[frame.index]);
  queue->occupied[frame.index] = false;
  platform_condition_signal (&queue->slot_available);

  platform_mutex_unlock (&queue->mutex);
}

void
frame_queue_finish (FrameQueue *queue, s32 result)
{
  assert (queue != NULL && (result == 1 || result < 0));

  platform_mutex_lock (&queue->mutex);

  queue->terminal_result = result;
  queue->producer_finished = true;
  platform_condition_broadcast (&queue->frame_ready);
  platform_condition_broadcast (&queue->slot_available);

  platform_mutex_unlock (&queue->mutex);
}

void
frame_queue_stop (FrameQueue *queue)
{
  assert (queue != NULL);

  platform_mutex_lock (&queue->mutex);

  queue->stopped = true;
  platform_condition_broadcast (&queue->frame_ready);
  platform_condition_broadcast (&queue->slot_available);

  platform_mutex_unlock (&queue->mutex);
}

bool
frame_queue_should_stop (FrameQueue *queue)
{
  assert (queue != NULL);

  bool stopped;
  platform_mutex_lock (&queue->mutex);
  stopped = queue->stopped;
  platform_mutex_unlock (&queue->mutex);

  return stopped;
}

s32
frame_queue_result (FrameQueue *queue)
{
  assert (queue != NULL);

  s32 result;
  platform_mutex_lock (&queue->mutex);
  result = queue->terminal_result;
  platform_mutex_unlock (&queue->mutex);

  return result;
}
