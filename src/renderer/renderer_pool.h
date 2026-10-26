#pragma once

#include "arena.h"
#include "thread.h"

typedef void (*RendererRowsFn) (void *context, s32 first_row, s32 end_row);

typedef struct
{
  PlatformThread *threads;
  s32 thread_count, row_count, completed_threads;
  u64 generation;
  bool stopping;
  RendererRowsFn function;
  void *context;
  PlatformMutex mutex;
  PlatformCondition work_available, work_complete;
} RendererPool;

typedef struct
{
  RendererPool *pool;
  s32 worker_index;
} RendererWorker;

// The caller serializes run_rows calls and renders the first row range itself.
// Worker ranges are disjoint; callback and context stay valid until return.
bool renderer_pool_init (RendererPool *pool, Arena *arena, s32 thread_count);
void renderer_pool_shutdown (RendererPool *pool);

void renderer_pool_run_rows (RendererPool *pool, s32 row_count,
                             RendererRowsFn function, void *context);
u64 renderer_pool_memory_required (s32 thread_count);
