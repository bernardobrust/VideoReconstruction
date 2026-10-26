#define _POSIX_C_SOURCE 200112L

#include "thread.h"

#include <assert.h>
#include <unistd.h>

local void *
thread_start (void *data)
{
  PlatformThread *thread = data;

  thread->fn (thread->arg);

  return NULL;
}

bool
platform_thread_start (PlatformThread *thread, PlatformThreadFn fn, void *arg)
{
  assert (thread != NULL && fn != NULL);
  thread->fn = fn;
  thread->arg = arg;

  return pthread_create (&thread->handle, NULL, thread_start, thread) == 0;
}

void
platform_thread_join (PlatformThread *thread)
{
  int result;

  assert (thread != NULL);
  result = pthread_join (thread->handle, NULL);
  assert (result == 0);
  (void)result;
}

void
platform_mutex_init (PlatformMutex *mutex)
{
  int result;

  assert (mutex != NULL);
  result = pthread_mutex_init (&mutex->native, NULL);
  assert (result == 0);
  (void)result;
}

void
platform_mutex_destroy (PlatformMutex *mutex)
{
  int result;

  assert (mutex != NULL);
  result = pthread_mutex_destroy (&mutex->native);
  assert (result == 0);
  (void)result;
}

void
platform_mutex_lock (PlatformMutex *mutex)
{
  int result;

  assert (mutex != NULL);
  result = pthread_mutex_lock (&mutex->native);
  assert (result == 0);
  (void)result;
}

void
platform_mutex_unlock (PlatformMutex *mutex)
{
  int result;

  assert (mutex != NULL);
  result = pthread_mutex_unlock (&mutex->native);
  assert (result == 0);
  (void)result;
}

void
platform_condition_init (PlatformCondition *condition)
{
  int result;

  assert (condition != NULL);
  result = pthread_cond_init (&condition->native, NULL);
  assert (result == 0);
  (void)result;
}

void
platform_condition_destroy (PlatformCondition *condition)
{
  int result;

  assert (condition != NULL);
  result = pthread_cond_destroy (&condition->native);
  assert (result == 0);
  (void)result;
}

void
platform_condition_wait (PlatformCondition *condition, PlatformMutex *mutex)
{
  int result;

  assert (condition != NULL && mutex != NULL);
  result = pthread_cond_wait (&condition->native, &mutex->native);
  assert (result == 0);
  (void)result;
}

void
platform_condition_signal (PlatformCondition *condition)
{
  int result;

  assert (condition != NULL);
  result = pthread_cond_signal (&condition->native);
  assert (result == 0);
  (void)result;
}

void
platform_condition_broadcast (PlatformCondition *condition)
{
  int result;

  assert (condition != NULL);
  result = pthread_cond_broadcast (&condition->native);
  assert (result == 0);
  (void)result;
}

s32
platform_get_logical_core_count (void)
{
  long count;

  count = sysconf (_SC_NPROCESSORS_ONLN);

  return count > 0 && count <= INT32_MAX ? (s32)count : 1;
}
