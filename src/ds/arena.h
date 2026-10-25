#pragma once

#include "macros.h"

typedef struct
{
  ubyte *memory;
  u64 capacity, used;
} Arena;

void arena_init (Arena *arena, u64 capacity);
void arena_release (Arena *arena);
void *arena_alloc (Arena *arena, u64 size, u64 alignment);
