#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"

void
arena_init (Arena *arena, u64 capacity)
{
  assert (arena != NULL && capacity > 0);
  assert (capacity <= SIZE_MAX);
  assert (arena->memory == NULL && arena->capacity == 0 && arena->used == 0);

  ubyte *memory = (ubyte *)malloc ((u64)capacity);
  assert (memory != NULL);

  arena->memory = memory;
  arena->capacity = capacity;
  arena->used = 0;
}

void
arena_release (Arena *arena)
{
  assert (arena != NULL);

  free (arena->memory);
  arena->memory = NULL;
  arena->capacity = 0;
  arena->used = 0;
}

void *
arena_alloc (Arena *arena, u64 size, u64 alignment)
{
  assert (arena != NULL && arena->memory != NULL && size > 0 && alignment > 0
          && (alignment & (alignment - 1)) == 0);
  assert (arena->used <= arena->capacity);

  uintptr_t base = (uintptr_t)arena->memory;
  assert (base <= UINTPTR_MAX - arena->used);

  uintptr_t current = base + (uintptr_t)arena->used;
  u64 misalignment = (u64)(current & (uintptr_t)(alignment - 1)),
      padding = (alignment - misalignment) & (alignment - 1);
  assert (padding <= arena->capacity - arena->used);

  u64 offset = arena->used + padding;
  assert (size <= arena->capacity - offset);

  ubyte *result = arena->memory + offset;
  arena->used = offset + size;

  return result;
}
