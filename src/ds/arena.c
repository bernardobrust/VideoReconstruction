#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "arena.h"

void
arena_init (Arena *arena, u64 cap)
{
  assert (arena != NULL && cap > 0);
  assert (cap <= SIZE_MAX);
  assert (arena->memory == NULL && arena->cap == 0 && arena->used == 0);

  ubyte *memory = (ubyte *)malloc ((u64)cap);
  assert (memory != NULL);

  arena->memory = memory;
  arena->cap = cap;
  arena->used = 0;
}

void
arena_release (Arena *arena)
{
  assert (arena != NULL);

  free (arena->memory);
  arena->memory = NULL;
  arena->cap = 0;
  arena->used = 0;
}

/*
 * Allocates memory from a preallocated contiguous block (arena)
 *
 * Arena allocations are performed by advancing a pointer into the arena's
 * memory. No individual allocations or deallocations occur, memory is
 * reclaimed all at once by resetting arena->used to zero or destroying the
 * arena. This is usefull to avoid syscalls like malloc()
 *
 * Each allocation is aligned to the requested power-of-two boundary.
 * Padding is added when necessary to ensure the returned address satisfies
 * the alignment requirement. The arena's used counter is then advanced past
 * the allocated region, including any alignment padding
 *
 * The caller must ensure sufficient capacity is available. Allocations are
 * never resized or moved, so previously returned pointers remain valid as
 * long as the arena's underlying memory remains allocated
 *
 * This is why we have functions to calculate the memory required for systems
 */
void *
arena_alloc (Arena *arena, u64 size, u64 alignment)
{
  assert (arena != NULL && arena->memory != NULL && size > 0 && alignment > 0
          && (alignment & (alignment - 1)) == 0);
  assert (arena->used <= arena->cap);

  uintptr_t base = (uintptr_t)arena->memory;
  assert (base <= UINTPTR_MAX - arena->used);

  uintptr_t current = base + (uintptr_t)arena->used;
  u64 misalignment = (u64)(current & (uintptr_t)(alignment - 1)),
      padding = (alignment - misalignment) & (alignment - 1);
  assert (padding <= arena->cap - arena->used);

  u64 offset = arena->used + padding;
  assert (size <= arena->cap - offset);

  ubyte *result = arena->memory + offset;
  arena->used = offset + size;

  return result;
}
