#pragma once

#include "macros.h"

/*
 * Arena data structure
 * memory: pinter to the start of the preallocated block
 * cap: amount of bytes avaliable in the block
 * used: bytes already in use
 */
typedef struct
{
  ubyte *memory;
  u64 cap, used;
} Arena;

void arena_init (Arena *arena, u64 cap);
void arena_release (Arena *arena);
void *arena_alloc (Arena *arena, u64 size, u64 alignment);
