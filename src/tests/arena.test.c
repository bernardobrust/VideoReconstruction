#define ASTF_STRIP_PREFIX
#include "astf.h"

#include "arena.c"
#include "arena.test.h"

#include <stdint.h>

static void
arena_test_alignment_and_capacity (void)
{
  start_test_suite ("Arena alignment and capacity");

  Arena arena = { 0 };
  arena_init (&arena, 128);
  void *aligned = arena_alloc (&arena, 8, 64);

  assert_not_null (aligned);
  assert_true ((u64)(uintptr_t)aligned % 64 == 0);

  u64 remaining = arena.capacity - arena.used;
  void *last = arena_alloc (&arena, remaining, 1);

  assert_not_null (last);
  assert_true (arena.capacity == arena.used);

  arena_release (&arena);
  assert_null (arena.memory);

  retrieve_results ();
}

static void
arena_test_sequential_allocations (void)
{
  start_test_suite ("Arena sequential allocations and padding");

  Arena arena = { 0 };
  arena_init (&arena, 256);

  u8 *first = arena_alloc (&arena, 3, 1);
  u32 *second = arena_alloc (&arena, sizeof (*second), _Alignof (u32));
  u64 *third = arena_alloc (&arena, sizeof (*third), _Alignof (u64));

  assert_not_null (first);
  assert_not_null (second);
  assert_not_null (third);
  assert_true ((uintptr_t)second % _Alignof (u32) == 0);
  assert_true ((uintptr_t)third % _Alignof (u64) == 0);
  assert_true ((ubyte *)second >= first + 3);
  assert_true ((ubyte *)third >= (ubyte *)second + sizeof (*second));
  assert_true (arena.used >= 3 + sizeof (*second) + sizeof (*third));
  assert_true (arena.used < arena.capacity);

  arena_release (&arena);

  retrieve_results ();
}

static void
arena_test_release (void)
{
  start_test_suite ("Arena release");

  Arena arena = { 0 };
  arena_init (&arena, 32);
  arena_alloc (&arena, 4, _Alignof (u32));
  arena_release (&arena);

  assert_null (arena.memory);
  assert_true (arena.capacity == 0);

  retrieve_results ();
}

void
arena_all_tests (void)
{
  start_group ("Arena Tests");

  arena_test_alignment_and_capacity ();
  arena_test_sequential_allocations ();
  arena_test_release ();

  end_group ();
}
