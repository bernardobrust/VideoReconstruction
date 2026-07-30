// Common GNU + Linux functions

#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "platform.h"

double
platform_get_time (void)
{
  struct timeval time;

  gettimeofday (&time, NULL);
  return (double)time.tv_sec * 1000.0 + (double)time.tv_usec / 1000.0;
}

void
platform_sleep (double ms)
{
  if (ms <= 0.0)
    return;

  struct timespec time
      = { .tv_sec = (time_t)(ms / 1000.0),
          .tv_nsec = (long)((ms - (double)(time_t)(ms / 1000.0) * 1000.0)
                            * 1000000.0) };

  while (nanosleep (&time, &time) == -1 && errno == EINTR)
    {
    }
}
