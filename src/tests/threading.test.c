#define ASTF_STRIP_PREFIX
#include "astf.h"

#include "arena.h"
#include "frame_queue.h"
#include "macros.h"
#include "renderer_pool.h"
#include "thread.h"

#include <assert.h>
#include <string.h>

enum
{
  FRAME_TEST_COUNT = 128,
  FRAME_TEST_CAPACITY = 3,
  POOL_TEST_MAX_ROWS = 20,
  POOL_TEST_WORKERS = 4
};

typedef struct
{
  FrameQueue *queue;
  s32 count;
} FrameQueueTestContext;

local void
frame_queue_test_producer (void *arg)
{
  FrameQueueTestContext *context = arg;

  for (s32 i = 1; i <= context->count; ++i)
    {
      FrameQueueFrame frame;

      bool acquired = frame_queue_acquire_write (context->queue, &frame);
      assert (acquired);

      frame.renderer->image_buffer[0] = (u32)i;
      frame_queue_publish (context->queue, frame);
    }

  frame_queue_finish (context->queue, 1);
}

local void
frame_queue_test_consumer (void *arg)
{
  FrameQueueTestContext *context = arg;

  for (s32 expected = 1; expected <= context->count; ++expected)
    {
      FrameQueueFrame frame;

      bool available = frame_queue_take (context->queue, &frame);
      assert (available);
      assert (frame.renderer->image_buffer[0] == (u32)expected);

      frame_queue_release (context->queue, frame);
    }
}

typedef struct
{
  u32 *pixels;
  s32 width;
} RowsTestContext;

local void
thread_test_entry (void *arg)
{
  s32 *value = arg;

  *value = 42;
}

local void
fill_rows (void *arg, s32 first_row, s32 end_row)
{
  RowsTestContext *context = arg;
  for (s32 y = first_row; y < end_row; ++y)
    for (s32 x = 0; x < context->width; ++x)
      context->pixels[y * context->width + x] = (u32)(y + 1);
}

local void
renderer_pool_test (void)
{
  start_test_suite ("Renderer worker pool: partitioning and repeated runs");

  Arena arena = { 0 };
  arena_init (&arena, 64 * 1024);
  RendererPool pool, serial_pool;
  bool initialized = renderer_pool_init (&pool, &arena, POOL_TEST_WORKERS);
  bool serial_initialized = renderer_pool_init (&serial_pool, &arena, 0);
  assert (initialized);
  assert (serial_initialized);

  (void)initialized;
  (void)serial_initialized;

  enum
  {
    TEST_WIDTH = 7,
    TEST_ROW_CASES = 5
  };

  const s32 row_counts[TEST_ROW_CASES] = { 1, 2, 3, 19, POOL_TEST_MAX_ROWS };
  u32 pixels[POOL_TEST_MAX_ROWS * TEST_WIDTH] = { 0 };
  u32 serial_pixels[POOL_TEST_MAX_ROWS * TEST_WIDTH] = { 0 };
  RowsTestContext context = { pixels, TEST_WIDTH };
  RowsTestContext serial_context = { serial_pixels, TEST_WIDTH };

  for (s32 test_index = 0; test_index < TEST_ROW_CASES; ++test_index)
    {
      s32 row_count = row_counts[test_index];
      size_t used_bytes = (size_t)row_count * TEST_WIDTH * sizeof (u32);

      memset (pixels, 0, sizeof pixels);
      memset (serial_pixels, 0, sizeof serial_pixels);

      fill_rows (&serial_context, 0, row_count);
      renderer_pool_run_rows (&pool, row_count, fill_rows, &context);

      assert_equal (0, memcmp (serial_pixels, pixels, used_bytes));

      memset (pixels, 0, sizeof pixels);
      renderer_pool_run_rows (&serial_pool, row_count, fill_rows, &context);

      assert_equal (0, memcmp (serial_pixels, pixels, used_bytes));

      for (s32 y = 0; y < row_count; ++y)
        for (s32 x = 0; x < TEST_WIDTH; ++x)
          assert_equal ((u32)(y + 1), pixels[y * TEST_WIDTH + x]);
    }

  renderer_pool_shutdown (&serial_pool);
  renderer_pool_shutdown (&pool);
  arena_release (&arena);

  retrieve_results ();
}

local void
platform_thread_start_and_join_test (void)
{
  start_test_suite ("Platform thread: start, execute, and join");

  PlatformThread thread;
  s32 value = 0;
  bool started = platform_thread_start (&thread, thread_test_entry, &value);

  assert_true (started);
  platform_thread_join (&thread);
  assert_equal (42, value);

  retrieve_results ();
}

local void
platform_core_count_test (void)
{
  start_test_suite ("Platform threading: logical core count");

  assert_true (platform_get_logical_core_count () >= 1);

  retrieve_results ();
}

local void
frame_queue_test_order_and_capacity (void)
{
  start_test_suite ("Frame queue: bounded ordered producer and consumer");

  Arena arena = { 0 };
  arena_init (&arena, 64 * 1024);
  FrameQueue queue;
  frame_queue_init (&queue, &arena, 1, 1, FRAME_TEST_CAPACITY);

  FrameQueueTestContext context = { &queue, FRAME_TEST_COUNT };
  PlatformThread producer, consumer;
  bool producer_started
      = platform_thread_start (&producer, frame_queue_test_producer, &context);
  bool consumer_started
      = platform_thread_start (&consumer, frame_queue_test_consumer, &context);
  assert (producer_started && consumer_started);

  platform_thread_join (&producer);
  platform_thread_join (&consumer);

  assert_equal (FRAME_TEST_CAPACITY, queue.capacity);
  assert_equal (1, frame_queue_result (&queue));

  frame_queue_destroy (&queue);
  arena_release (&arena);

  retrieve_results ();
}

local void
frame_queue_test_finish_and_stop (void)
{
  start_test_suite ("Frame queue: finish, drain, and stop states");

  Arena arena = { 0 };
  arena_init (&arena, 64 * 1024);
  FrameQueue queue;
  FrameQueueFrame frame;
  frame_queue_init (&queue, &arena, 2, 2, 1);

  assert_false (frame_queue_should_stop (&queue));
  assert_true (frame_queue_acquire_write (&queue, &frame));

  frame.renderer->image_buffer[0] = 1234;
  frame_queue_publish (&queue, frame);
  frame_queue_finish (&queue, 1);

  assert_true (frame_queue_take (&queue, &frame));
  assert_equal (1234, frame.renderer->image_buffer[0]);

  frame_queue_release (&queue, frame);
  assert_false (frame_queue_take (&queue, &frame));
  assert_equal (1, frame_queue_result (&queue));
  assert_false (frame_queue_acquire_write (&queue, &frame));

  frame_queue_stop (&queue);
  assert_true (frame_queue_should_stop (&queue));
  assert_false (frame_queue_acquire_write (&queue, &frame));

  frame_queue_destroy (&queue);
  arena_release (&arena);

  retrieve_results ();
}

void
threading_all_tests (void)
{
  start_group ("Threading and Pipeline Tests");

  platform_core_count_test ();
  platform_thread_start_and_join_test ();
  frame_queue_test_order_and_capacity ();
  frame_queue_test_finish_and_stop ();
  renderer_pool_test ();

  end_group ();
}
