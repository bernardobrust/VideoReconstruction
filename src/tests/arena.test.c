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
    arena_test_alignment_and_capacity ();
    arena_test_release ();
}
