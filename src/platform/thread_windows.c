#include "thread.h"

#include <assert.h>

local DWORD WINAPI
thread_start (LPVOID data)
{
  PlatformThread *thread = data;

  thread->fn (thread->arg);

  return 0;
}

bool
platform_thread_start (PlatformThread *thread, PlatformThreadFn fn, void *arg)
{
  assert (thread != NULL && fn != NULL);
  thread->fn = fn;
  thread->arg = arg;

  thread->handle = CreateThread (NULL, 0, thread_start, thread, 0, NULL);

  return thread->handle != NULL;
}

void
platform_thread_join (PlatformThread *thread)
{
  DWORD result;

  assert (thread != NULL && thread->handle != NULL);
  result = WaitForSingleObject (thread->handle, INFINITE);
  assert (result == WAIT_OBJECT_0);
  (void)result;

  CloseHandle (thread->handle);
  thread->handle = NULL;
}

void
platform_mutex_init (PlatformMutex *mutex)
{
  assert (mutex != NULL);

  InitializeCriticalSection (&mutex->native);
}

void
platform_mutex_destroy (PlatformMutex *mutex)
{
  assert (mutex != NULL);
  DeleteCriticalSection (&mutex->native);
}

void
platform_mutex_lock (PlatformMutex *mutex)
{
  assert (mutex != NULL);
  EnterCriticalSection (&mutex->native);
}

void
platform_mutex_unlock (PlatformMutex *mutex)
{
  assert (mutex != NULL);
  LeaveCriticalSection (&mutex->native);
}

void
platform_condition_init (PlatformCondition *condition)
{
  assert (condition != NULL);

  InitializeConditionVariable (&condition->native);
}

void
platform_condition_destroy (PlatformCondition *condition)
{
  assert (condition != NULL);
  (void)condition;
}

void
platform_condition_wait (PlatformCondition *condition, PlatformMutex *mutex)
{
  BOOL result;

  assert (condition != NULL && mutex != NULL);
  result = SleepConditionVariableCS (&condition->native, &mutex->native,
                                     INFINITE);
  assert (result != 0);
  (void)result;
}

void
platform_condition_signal (PlatformCondition *condition)
{
  assert (condition != NULL);
  WakeConditionVariable (&condition->native);
}

void
platform_condition_broadcast (PlatformCondition *condition)
{
  assert (condition != NULL);
  WakeAllConditionVariable (&condition->native);
}

s32
platform_get_logical_core_count (void)
{
  SYSTEM_INFO info;

  GetSystemInfo (&info);

  return info.dwNumberOfProcessors > 0 ? (s32)info.dwNumberOfProcessors : 1;
}
