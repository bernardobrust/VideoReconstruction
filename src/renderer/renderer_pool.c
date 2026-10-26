#include "renderer_pool.h"

#include <assert.h>
#include <string.h>

u64
renderer_pool_memory_required (s32 thread_count)
{
  assert (thread_count >= 0);

  u64 per_worker;

  if (thread_count == 0)
    return 0;

  per_worker = sizeof (PlatformThread) + sizeof (RendererWorker);
  assert ((u64)thread_count <= (UINT64_MAX - 256) / per_worker);

  return 256 + (u64)thread_count * per_worker;
}

local void
renderer_worker (void *arg)
{
  RendererWorker *worker;
  RendererPool *pool;
  RendererRowsFn function;
  void *context;
  s32 row_count, lane_count, first_row, end_row;
  u64 seen_generation = 0;

  worker = arg;
  pool = worker->pool;

  for (;;)
    {
      platform_mutex_lock (&pool->mutex);

      while (pool->generation == seen_generation && !pool->stopping)
        platform_condition_wait (&pool->work_available, &pool->mutex);

      if (pool->stopping)
        {
          platform_mutex_unlock (&pool->mutex);

          return;
        }

      seen_generation = pool->generation;
      function = pool->function;
      context = pool->context;
      row_count = pool->row_count;
      lane_count = pool->thread_count + 1;
      first_row
          = (s32)(((s64)row_count * (worker->worker_index + 1)) / lane_count);
      end_row
          = (s32)(((s64)row_count * (worker->worker_index + 2)) / lane_count);

      platform_mutex_unlock (&pool->mutex);

      // Each worker receives a disjoint row interval. Once the completion
      // counter reaches the pool size, the caller may safely reuse the
      // buffers.
      function (context, first_row, end_row);

      platform_mutex_lock (&pool->mutex);

      ++pool->completed_threads;
      if (pool->completed_threads == pool->thread_count)
        platform_condition_signal (&pool->work_complete);

      platform_mutex_unlock (&pool->mutex);
    }
}

bool
renderer_pool_init (RendererPool *pool, Arena *arena, s32 thread_count)
{
  assert (pool != NULL && arena != NULL && thread_count >= 0);

  RendererWorker *workers;
  bool started;
  s32 started_count, i, j;

  memset (pool, 0, sizeof (*pool));
  pool->thread_count = thread_count;
  if (thread_count == 0)
    return true;

  pool->threads
      = arena_alloc (arena, (u64)thread_count * sizeof (*pool->threads),
                     _Alignof (PlatformThread));
  workers = arena_alloc (arena, (u64)thread_count * sizeof (*workers),
                         _Alignof (RendererWorker));
  assert (pool->threads != NULL && workers != NULL);

  platform_mutex_init (&pool->mutex);
  platform_condition_init (&pool->work_available);
  platform_condition_init (&pool->work_complete);

  started_count = 0;
  for (i = 0; i < thread_count; ++i)
    {
      workers[i].pool = pool;
      workers[i].worker_index = i;
      started = platform_thread_start (&pool->threads[i], renderer_worker,
                                       &workers[i]);
      if (!started)
        {
          platform_mutex_lock (&pool->mutex);
          pool->stopping = true;
          platform_condition_broadcast (&pool->work_available);
          platform_mutex_unlock (&pool->mutex);

          for (j = 0; j < started_count; ++j)
            platform_thread_join (&pool->threads[j]);

          platform_condition_destroy (&pool->work_complete);
          platform_condition_destroy (&pool->work_available);
          platform_mutex_destroy (&pool->mutex);

          return false;
        }

      ++started_count;
    }

  return true;
}

void
renderer_pool_run_rows (RendererPool *pool, s32 row_count,
                        RendererRowsFn function, void *context)
{
  s32 lane_count, caller_end;

  assert (pool != NULL && row_count > 0 && function != NULL);
  if (pool->thread_count == 0)
    {
      function (context, 0, row_count);
      return;
    }

  platform_mutex_lock (&pool->mutex);

  assert (pool->completed_threads == 0);

  pool->row_count = row_count;
  pool->function = function;
  pool->context = context;
  ++pool->generation;
  lane_count = pool->thread_count + 1;
  caller_end = (s32)((s64)row_count / lane_count);
  platform_condition_broadcast (&pool->work_available);

  platform_mutex_unlock (&pool->mutex);

  // The systems thread renders the first row range itself. This avoids
  // dedicating a thread that only waits for the renderer workers to finish.
  function (context, 0, caller_end);

  platform_mutex_lock (&pool->mutex);

  while (pool->completed_threads < pool->thread_count)
    platform_condition_wait (&pool->work_complete, &pool->mutex);
  pool->completed_threads = 0;

  platform_mutex_unlock (&pool->mutex);
}

void
renderer_pool_shutdown (RendererPool *pool)
{
  assert (pool != NULL);

  s32 i;

  if (pool->thread_count == 0)
    return;

  platform_mutex_lock (&pool->mutex);

  pool->stopping = true;
  platform_condition_broadcast (&pool->work_available);

  platform_mutex_unlock (&pool->mutex);

  for (i = 0; i < pool->thread_count; ++i)
    platform_thread_join (&pool->threads[i]);

  platform_condition_destroy (&pool->work_complete);
  platform_condition_destroy (&pool->work_available);
  platform_mutex_destroy (&pool->mutex);
}
