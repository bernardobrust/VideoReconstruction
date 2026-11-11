#include "frame_queue.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "basic.h"

// Calculation for required memory for the FrameQueue for arena allocation
u64
frame_queue_memory_required (s32 w, s32 h, s32 capacity)
{
  u64 metadata, frame_bytes;

  assert (w > 0 && h > 0 && capacity > 0);
  assert ((u64)w <= UINT64_MAX / (u64)h / sizeof (u32));

  metadata = 256 + (u64)capacity * sizeof (RendererPlex)
             + (u64)capacity * sizeof (s32) + (u64)capacity * sizeof (bool)
             + (u64)capacity * sizeof (bool) + (u64)capacity * sizeof (s32)
             + (u64)capacity * sizeof (bool)
             + (u64)capacity * sizeof (AVMotionVector *)
             + (u64)capacity * sizeof (s64) + (u64)capacity * sizeof (s32) * 2;
  frame_bytes = (u64)w * (u64)h * sizeof (u32);

  assert (metadata <= UINT64_MAX
          && frame_bytes <= (UINT64_MAX - metadata) / (u64)capacity);

  return metadata + (u64)capacity * frame_bytes;
}

// Give the budget in the config file, calculate how much capacity we have
s32
frame_queue_capacity_for_bytes (s32 w, s32 h, u64 byte_budget)
{
  u64 per_frame, overhead_per_frame, capacity_u64;
  s32 capacity;

  assert (w > 0 && h > 0 && byte_budget > 0);
  assert ((u64)w <= UINT64_MAX / (u64)h / sizeof (u32));

  per_frame = (u64)w * (u64)h * sizeof (u32);
  overhead_per_frame = sizeof (RendererPlex) + sizeof (s32) * 4
                       + sizeof (bool) * 2 + sizeof (AVMotionVector *);
  overhead_per_frame += sizeof (s64);
  overhead_per_frame += sizeof (bool);

  if (byte_budget <= 256 || per_frame > UINT64_MAX - overhead_per_frame)
    return 1;

  capacity_u64 = (byte_budget - 256) / (per_frame + overhead_per_frame);
  capacity = (s32)MIN (capacity_u64, INT32_MAX);

  return MAX (capacity, 1);
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
  queue->history_capacity = (capacity + 1) / 2;
  queue->future_capacity = MAX (1, capacity - queue->history_capacity);
  queue->history_cursor = -1;
  queue->generation = 1;
  queue->producer_finished = false;
  queue->terminal_result = 0;
  queue->producer_wakeup = false;
  queue->frames = arena_alloc (arena, (u64)capacity * sizeof (*queue->frames),
                               _Alignof (RendererPlex));
  queue->ready_indices = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->ready_indices), _Alignof (s32));
  queue->history_indices = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->history_indices), _Alignof (s32));
  queue->occupied = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->occupied), _Alignof (bool));
  queue->history_pinned = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->history_pinned), _Alignof (bool));
  queue->history_reading
      = arena_alloc (arena, (u64)capacity * sizeof (*queue->history_reading),
                     _Alignof (bool));
  queue->motion_vectors
      = arena_alloc (arena, (u64)capacity * sizeof (*queue->motion_vectors),
                     _Alignof (AVMotionVector *));
  queue->frame_numbers = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->frame_numbers), _Alignof (s64));
  queue->motion_vector_counts = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->motion_vector_counts),
      _Alignof (s32));
  queue->motion_vector_capacities = arena_alloc (
      arena, (u64)capacity * sizeof (*queue->motion_vector_capacities),
      _Alignof (s32));

  assert (queue->frames != NULL && queue->ready_indices != NULL
          && queue->history_indices != NULL && queue->occupied != NULL
          && queue->history_pinned != NULL && queue->history_reading != NULL
          && queue->motion_vectors != NULL && queue->frame_numbers != NULL
          && queue->motion_vector_counts != NULL
          && queue->motion_vector_capacities != NULL);

  memset (queue->occupied, 0, (u64)capacity * sizeof (*queue->occupied));
  memset (queue->history_pinned, 0,
          (u64)capacity * sizeof (*queue->history_pinned));
  memset (queue->history_reading, 0,
          (u64)capacity * sizeof (*queue->history_reading));
  memset (queue->motion_vectors, 0,
          (u64)capacity * sizeof (*queue->motion_vectors));
  memset (queue->motion_vector_counts, 0,
          (u64)capacity * sizeof (*queue->motion_vector_counts));
  memset (queue->motion_vector_capacities, 0,
          (u64)capacity * sizeof (*queue->motion_vector_capacities));

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
  s32 i;

  assert (queue != NULL);

  // Call only after the decoder and consumer threads have stopped
  for (i = 0; i < queue->capacity; ++i)
    free (queue->motion_vectors[i]);

  platform_condition_destroy (&queue->slot_available);
  platform_condition_destroy (&queue->frame_ready);
  platform_mutex_destroy (&queue->mutex);
}

// The queue is a circular data structure in practice
local bool
frame_queue_evict_oldest_history (FrameQueue *queue)
{
  s32 index;

  if (queue->history_count == 0)
    return false;

  index = queue->history_indices[queue->history_head];
  if (queue->history_pinned[index])
    return false;

  queue->occupied[index] = false;
  queue->history_head = (queue->history_head + 1) % queue->capacity;
  --queue->history_count;

  if (queue->history_cursor > 0)
    --queue->history_cursor;
  else if (queue->history_count == 0)
    queue->history_cursor = -1;

  return true;
}

// Threading function for write
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

      if (queue->producer_wakeup)
        {
          queue->producer_wakeup = false;
          platform_mutex_unlock (&queue->mutex);
          return false;
        }

      if (queue->ready_count >= queue->future_capacity)
        {
          platform_condition_wait (&queue->slot_available, &queue->mutex);
          continue;
        }

      for (i = 0; i < queue->capacity; ++i)
        {
          if (!queue->occupied[i])
            {
              // The occupied flag reserves this slot until the consumer
              // returns it. The producer may now write without holding the
              // queue mutex
              queue->occupied[i] = true;
              queue->history_pinned[i] = false;
              queue->history_reading[i] = false;
              frame->index = i;
              frame->renderer = &queue->frames[i];
              frame->motion_vectors = queue->motion_vectors[i];
              frame->motion_vector_count = 0;
              frame->was_taken = false;
              frame->from_history = false;
              frame->frame_number = -1;
              frame->generation = queue->generation;
              queue->motion_vector_counts[i] = 0;

              platform_mutex_unlock (&queue->mutex);
              return true;
            }
        }

      if (frame_queue_evict_oldest_history (queue))
        continue;

      platform_condition_wait (&queue->slot_available, &queue->mutex);
    }
}

void
frame_queue_set_frame_number (FrameQueue *queue, FrameQueueFrame *frame,
                              s64 frame_number)
{
  assert (queue != NULL && frame != NULL && frame_number >= 0);
  assert (frame->index >= 0 && frame->index < queue->capacity);

  queue->frame_numbers[frame->index] = frame_number;
  frame->frame_number = frame_number;
}

void
frame_queue_clear (FrameQueue *queue)
{
  s32 i, index;

  assert (queue != NULL);

  platform_mutex_lock (&queue->mutex);

  // Preserve slots currently read by the consumer, their old generation makes
  // their release discard them instead of adding stale frames to history
  for (i = 0; i < queue->ready_count; ++i)
    {
      index = queue->ready_indices[(queue->ready_head + i) % queue->capacity];
      queue->occupied[index] = false;
    }

  for (i = 0; i < queue->history_count; ++i)
    {
      index
          = queue
                ->history_indices[(queue->history_head + i) % queue->capacity];
      if (!queue->history_reading[index])
        {
          queue->occupied[index] = false;
          queue->history_pinned[index] = false;
        }
    }

  ++queue->generation;
  queue->ready_head = 0;
  queue->ready_count = 0;
  queue->history_head = 0;
  queue->history_count = 0;
  queue->history_cursor = -1;
  queue->producer_finished = false;
  queue->terminal_result = 0;
  queue->producer_wakeup = false;
  platform_condition_broadcast (&queue->slot_available);

  platform_mutex_unlock (&queue->mutex);
}

// Unlock + broadcast as avaliable
void
frame_queue_wake_producer (FrameQueue *queue)
{
  assert (queue != NULL);

  platform_mutex_lock (&queue->mutex);

  queue->producer_wakeup = true;
  platform_condition_broadcast (&queue->slot_available);

  platform_mutex_unlock (&queue->mutex);
}

void
frame_queue_copy_motion_vectors (FrameQueue *queue, FrameQueueFrame *frame,
                                 const AVMotionVector *motion_vectors,
                                 s32 count)
{
  AVMotionVector *resized;

  assert (queue != NULL && frame != NULL && frame->index >= 0
          && frame->index < queue->capacity
          && (motion_vectors != NULL || count == 0) && count >= 0);

  // Frame slot needs to be reserved
  assert (queue->occupied[frame->index]);

  if (count > queue->motion_vector_capacities[frame->index])
    {
      // Grow
      resized = realloc (queue->motion_vectors[frame->index],
                         (u64)count * sizeof (*resized));

      assert (resized != NULL);

      queue->motion_vectors[frame->index] = resized;
      queue->motion_vector_capacities[frame->index] = count;
    }

  // Now we copy
  if (count > 0)
    memcpy (queue->motion_vectors[frame->index], motion_vectors,
            (u64)count * sizeof (*motion_vectors));

  // Update metadata
  queue->motion_vector_counts[frame->index] = count;
  frame->motion_vectors = queue->motion_vectors[frame->index];
  frame->motion_vector_count = count;
}

void
frame_queue_publish (FrameQueue *queue, FrameQueueFrame frame)
{
  s32 tail;

  assert (queue != NULL && frame.index >= 0 && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  // Lock/unlock publishes the producer's completed pixel writes to the
  // consumer, which takes this same mutex before it can see the slot index
  assert (queue->occupied[frame.index]);
  assert (queue->ready_count < queue->capacity);
  tail = (queue->ready_head + queue->ready_count) % queue->capacity;

  queue->ready_indices[tail] = frame.index;
  ++queue->ready_count;
  platform_condition_signal (&queue->frame_ready);

  platform_mutex_unlock (&queue->mutex);
}

void
frame_queue_publish_history (FrameQueue *queue, FrameQueueFrame frame)
{
  s32 tail;

  assert (queue != NULL && frame.index >= 0 && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  // Keep frames decoded before a seek target available for backward steps
  assert (queue->occupied[frame.index]);
  assert (frame.generation == queue->generation);

  if (queue->history_count == queue->history_capacity)
    assert (frame_queue_evict_oldest_history (queue));

  tail = (queue->history_head + queue->history_count) % queue->capacity;

  queue->history_indices[tail] = frame.index;
  ++queue->history_count;
  queue->history_pinned[frame.index] = false;
  queue->history_reading[frame.index] = false;

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
  // frame_queue_release, preventing the decoder from overwriting a live frame
  frame->index = queue->ready_indices[queue->ready_head];
  frame->renderer = &queue->frames[frame->index];
  frame->motion_vectors = queue->motion_vectors[frame->index];
  frame->motion_vector_count = queue->motion_vector_counts[frame->index];
  frame->frame_number = queue->frame_numbers[frame->index];
  frame->generation = queue->generation;
  frame->was_taken = true;
  frame->from_history = false;
  queue->ready_head = (queue->ready_head + 1) % queue->capacity;
  --queue->ready_count;

  platform_condition_signal (&queue->slot_available);
  platform_mutex_unlock (&queue->mutex);

  return true;
}

void
frame_queue_release (FrameQueue *queue, FrameQueueFrame frame)
{
  s32 tail;

  assert (queue != NULL && frame.index >= 0 && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  // The decoder cannot reserve this slot again until this release is visible
  // under the queue mutex, so it cannot overwrite pixels still being read
  assert (queue->occupied[frame.index]);
  if (frame.was_taken)
    {
      if (frame.generation != queue->generation)
        {
          queue->occupied[frame.index] = false;

          platform_condition_signal (&queue->slot_available);
          platform_mutex_unlock (&queue->mutex);
          return;
        }

      if (queue->history_cursor >= 0)
        {
          s32 current_index;

          current_index = queue->history_indices[(queue->history_head
                                                  + queue->history_cursor)
                                                 % queue->capacity];
          queue->history_pinned[current_index] = false;
        }
      if (queue->history_count == queue->history_capacity)
        assert (frame_queue_evict_oldest_history (queue));

      tail = (queue->history_head + queue->history_count) % queue->capacity;
      queue->history_indices[tail] = frame.index;
      ++queue->history_count;
      queue->history_cursor = queue->history_count - 1;
      queue->history_pinned[frame.index] = true;
    }
  else
    queue->occupied[frame.index] = false;

  platform_condition_signal (&queue->slot_available);
  platform_mutex_unlock (&queue->mutex);
}

// Sliding window for the queue
bool
frame_queue_history_move (FrameQueue *queue, s32 direction,
                          FrameQueueFrame *frame)
{
  s32 next_cursor, index;

  assert (queue != NULL && frame != NULL
          && (direction == -1 || direction == 1));

  platform_mutex_lock (&queue->mutex);

  next_cursor = queue->history_cursor + direction;
  if (next_cursor < 0 || next_cursor >= queue->history_count)
    {
      platform_mutex_unlock (&queue->mutex);
      return false;
    }

  index = queue->history_indices[(queue->history_head + next_cursor)
                                 % queue->capacity];
  if (queue->history_cursor >= 0)
    {
      s32 previous_index;

      previous_index
          = queue
                ->history_indices[(queue->history_head + queue->history_cursor)
                                  % queue->capacity];
      queue->history_pinned[previous_index] = false;
      queue->history_reading[previous_index] = false;
    }

  queue->history_cursor = next_cursor;
  queue->history_pinned[index] = true;
  frame->index = index;
  frame->renderer = &queue->frames[index];
  frame->motion_vectors = queue->motion_vectors[index];
  frame->motion_vector_count = queue->motion_vector_counts[index];
  frame->frame_number = queue->frame_numbers[index];
  frame->generation = queue->generation;
  frame->was_taken = false;
  frame->from_history = true;
  queue->history_reading[index] = true;

  platform_mutex_unlock (&queue->mutex);
  return true;
}

void
frame_queue_history_release (FrameQueue *queue, FrameQueueFrame frame)
{
  assert (queue != NULL && frame.from_history && frame.index >= 0
          && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  if (frame.generation != queue->generation)
    {
      queue->occupied[frame.index] = false;
      queue->history_pinned[frame.index] = false;
      queue->history_reading[frame.index] = false;

      platform_condition_signal (&queue->slot_available);
      platform_mutex_unlock (&queue->mutex);
      return;
    }

  assert (queue->history_pinned[frame.index]);

  queue->history_reading[frame.index] = false;

  platform_condition_broadcast (&queue->slot_available);
  platform_mutex_unlock (&queue->mutex);
}

void
frame_queue_requeue_front (FrameQueue *queue, FrameQueueFrame frame)
{
  assert (queue != NULL && frame.was_taken && !frame.from_history
          && frame.index >= 0 && frame.index < queue->capacity);

  platform_mutex_lock (&queue->mutex);

  // Mismatch of generations: free slot
  if (frame.generation != queue->generation)
    {
      queue->occupied[frame.index] = false;
      platform_condition_signal (&queue->slot_available);
      platform_mutex_unlock (&queue->mutex);
      return;
    }

  assert (queue->occupied[frame.index]
          && queue->ready_count < queue->capacity);

  // Match: move head back
  queue->ready_head
      = (queue->ready_head + queue->capacity - 1) % queue->capacity;
  queue->ready_indices[queue->ready_head] = frame.index;
  ++queue->ready_count;

  platform_condition_signal (&queue->frame_ready);
  platform_mutex_unlock (&queue->mutex);
}

// Helper threading functions

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
