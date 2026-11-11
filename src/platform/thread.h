#pragma once

#include "macros.h"

#include <stdbool.h>

#ifdef _WIN32 // Windows

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

typedef void (*PlatformThreadFn) (void *arg);

// Native thread handle and entry-point data
typedef struct
{
  HANDLE handle;
  PlatformThreadFn fn;
  void *arg;
} PlatformThread;

typedef struct
{
  CRITICAL_SECTION native;
} PlatformMutex;

typedef struct
{
  CONDITION_VARIABLE native;
} PlatformCondition;

#else // GNU + LINUX

#include <pthread.h>

typedef void (*PlatformThreadFn) (void *arg);

// Native thread handle and entry-point data
typedef struct
{
  pthread_t handle;
  PlatformThreadFn fn;
  void *arg;
} PlatformThread;

typedef struct
{
  pthread_mutex_t native;
} PlatformMutex;

typedef struct
{
  pthread_cond_t native;
} PlatformCondition;

#endif

// Start a worker with the supplied callback and argument
void platform_thread_start (PlatformThread *thread, PlatformThreadFn fn,
                            void *arg);
void platform_thread_join (PlatformThread *thread);

// Initialize, destroy, lock, and unlock a platform mutex
void platform_mutex_init (PlatformMutex *mutex);
void platform_mutex_destroy (PlatformMutex *mutex);
void platform_mutex_lock (PlatformMutex *mutex);
void platform_mutex_unlock (PlatformMutex *mutex);

// Condition waits release the mutex until signaled, then reacquire it
void platform_condition_init (PlatformCondition *condition);
void platform_condition_destroy (PlatformCondition *condition);
void platform_condition_wait (PlatformCondition *condition,
                              PlatformMutex *mutex);
void platform_condition_signal (PlatformCondition *condition);
void platform_condition_broadcast (PlatformCondition *condition);

// Reports available logical processor capacity, not current CPU utilization.
s32 platform_get_logical_core_count (void);
