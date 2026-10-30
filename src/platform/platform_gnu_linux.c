// Common GNU + Linux functions

#define _POSIX_C_SOURCE 200112L

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "platform.h"

inline f64
platform_get_time (void)
{
  struct timeval time;

  gettimeofday (&time, NULL);
  return (f64)time.tv_sec * 1000.0 + (f64)time.tv_usec / 1000.0;
}

void
platform_sleep (f64 ms)
{
  if (ms <= 0.0)
    return;

  struct timespec time
      = { .tv_sec = (time_t)(ms / 1000.0),
          .tv_nsec
          = (long)((ms - (f64)(time_t)(ms / 1000.0) * 1000.0) * 1000000.0) };

  while (nanosleep (&time, &time) == -1 && errno == EINTR)
    {
    }
}

// Returns: 0 => OK
// 1 => wrong path
// 2 => inaccessible
inline s32
platform_file_exists (byte *filepath)
{
  if (access (filepath, F_OK) != 0)
    {
      if (errno == ENOENT)
        return 1;
      else
        return 2;
    }

  return 0;
}

bool
platform_mapped_file_create (PlatformMappedFile *file, const char *path,
                             u64 size)
{
  int fd;
  void *data;

  assert (file != NULL);
  assert (path != NULL);

  if (size > (u64)SIZE_MAX || size > (u64)INT64_MAX)
    return false;

  memset (file, 0, sizeof (*file));
  file->native_file = -1;

  fd = open (path, O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd < 0 || ftruncate (fd, (off_t)size) != 0)
    {
      if (fd >= 0)
        close (fd);

      return false;
    }

  if (size == 0)
    {
      file->size = 0;
      file->native_file = fd;
      return true;
    }

  data = mmap (NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED)
    {
      close (fd);

      return false;
    }

  file->data = data;
  file->size = size;
  file->native_file = fd;

  return true;
}

void
platform_mapped_file_close (PlatformMappedFile *file)
{
  assert (file != NULL);

  if (file->data != NULL && file->size > 0)
    munmap (file->data, (size_t)file->size);

  if (file->native_file >= 0)
    close ((int)file->native_file);

  memset (file, 0, sizeof (*file));
  file->native_file = -1;
}
