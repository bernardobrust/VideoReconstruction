#define ASTF_STRIP_PREFIX
#include "astf.h"

#include <string.h>

// This is actually really good for testing
#include "dyn_arr.c"

#include "dyn_arr.test.h"
#include "macros.h"

typedef struct
{
  f64 a;
  s32 b;
  byte c;
} St;

void
dyn_arr_test_init (void)
{
  start_test_suite ("Dynamic array initialization");

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (2, sizeof (s32));

  assert_not_null (xs);
  assert_not_null (xs->data);
  assert_equal (2, xs->cap);
  assert_equal (0, xs->len);
  assert_equal (4, xs->stride);
  // -----------------------------------------------------------

  // Compound --------------------------------------------------
  DynArr *xs2 = dyn_arr_init (4, sizeof (St));

  assert_not_null (xs2);
  assert_not_null (xs2->data);
  assert_equal (4, xs2->cap);
  assert_equal (0, xs2->len);
  // It's not 13, know your alignments!
  assert_equal (16, xs2->stride);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  dyn_arr_free (xs2);
  retrieve_results ();
}

void
dyn_arr_test_get (void)
{
  start_test_suite ("Dynamic array get");

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (4, sizeof (s32));
  xs->len = 4;
  s32 data[4] = { 4, 3, 2, 1 };
  memcpy (xs->data, data, 4 * 4);

  assert_equal (4, *(s32 *)(dyn_arr_get (xs, 0)));
  assert_equal (3, *(s32 *)(dyn_arr_get (xs, 1)));
  assert_equal (2, *(s32 *)(dyn_arr_get (xs, 2)));
  assert_equal (1, *(s32 *)(dyn_arr_get (xs, 3)));
  // -----------------------------------------------------------

  // Compound --------------------------------------------------
  DynArr *xs2 = dyn_arr_init (4, sizeof (St));
  xs2->len = 4;
  St data2[4] = {
    { 3.14, 8, 'a' }, { 2.16, 8, 'b' }, { 3.14, 16, 'd' }, { 1.23, 0, 'a' }
  };
  memcpy (xs2->data, data2, 4 * sizeof (St));

  assert_equal (8, ((St *)(dyn_arr_get (xs2, 0)))->b);
  assert_equal ('b', ((St *)(dyn_arr_get (xs2, 1)))->c);
  assert_approx (3.14, ((St *)(dyn_arr_get (xs2, 2)))->a, 1e-2);
  assert_equal (0, ((St *)(dyn_arr_get (xs2, 3)))->b);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  dyn_arr_free (xs2);
  retrieve_results ();
}

void
dyn_arr_test_set (void)
{
  start_test_suite ("Dynamic array set");

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (4, sizeof (s32));
  xs->len = 4;

  // Compound literals are better here
  dyn_arr_set (xs, 0, &(s32){ 4 });
  dyn_arr_set (xs, 1, &(s32){ 3 });
  dyn_arr_set (xs, 2, &(s32){ 2 });
  dyn_arr_set (xs, 3, &(s32){ 1 });

  assert_equal (4, *(s32 *)(dyn_arr_get (xs, 0)));
  assert_equal (3, *(s32 *)(dyn_arr_get (xs, 1)));
  assert_equal (2, *(s32 *)(dyn_arr_get (xs, 2)));
  assert_equal (1, *(s32 *)(dyn_arr_get (xs, 3)));
  // -----------------------------------------------------------

  // Compound --------------------------------------------------
  DynArr *xs2 = dyn_arr_init (4, sizeof (St));
  xs2->len = 4;
  St data[4] = {
    { 3.14, 8, 'a' }, { 2.16, 8, 'b' }, { 3.14, 16, 'd' }, { 1.23, 0, 'a' }
  };

  dyn_arr_set (xs2, 0, &data[0]);
  dyn_arr_set (xs2, 1, &data[1]);
  dyn_arr_set (xs2, 2, &data[2]);
  dyn_arr_set (xs2, 3, &data[3]);

  assert_equal (8, ((St *)(dyn_arr_get (xs2, 0)))->b);
  assert_equal ('b', ((St *)(dyn_arr_get (xs2, 1)))->c);
  assert_approx (3.14, ((St *)(dyn_arr_get (xs2, 2)))->a, 1e-2);
  assert_equal ('a', ((St *)(dyn_arr_get (xs2, 3)))->c);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  dyn_arr_free (xs2);
  retrieve_results ();
}

void
dyn_arr_test_push (void)
{
  start_test_suite ("Dynamic array push");

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (1, sizeof (s32));

  dyn_arr_push (xs, &(s32){ 4 });
  assert_equal (4, *(s32 *)(dyn_arr_get (xs, 0)));

  dyn_arr_push (xs, &(s32){ 5 });

  assert_equal (2, xs->len);
  assert_equal (2, xs->cap);
  assert_equal (5, *(s32 *)(dyn_arr_get (xs, 1)));

  dyn_arr_push (xs, &(s32){ 10 });

  assert_equal (3, xs->len);
  assert_equal (4, xs->cap);
  assert_equal (10, *(s32 *)(dyn_arr_get (xs, 2)));

  dyn_arr_push (xs, &(s32){ 4 });

  assert_equal (4, xs->len);
  assert_equal (4, xs->cap);

  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 4 });

  assert_equal (6, xs->len);
  assert_equal (8, xs->cap);
  assert_equal (4, *(s32 *)(dyn_arr_get (xs, 5)));
  // -----------------------------------------------------------

  // Compound --------------------------------------------------
  DynArr *xs2 = dyn_arr_init (4, sizeof (St));
  St data[4] = {
    { 3.14, 8, 'a' }, { 2.16, 8, 'b' }, { 3.14, 16, 'd' }, { 1.23, 0, 'a' }
  };

  dyn_arr_push (xs2, &data[0]);
  dyn_arr_push (xs2, &data[1]);
  dyn_arr_push (xs2, &data[2]);
  dyn_arr_push (xs2, &data[3]);

  assert_equal (8, ((St *)(dyn_arr_get (xs2, 0)))->b);
  assert_equal ('b', ((St *)(dyn_arr_get (xs2, 1)))->c);
  assert_approx (3.14, ((St *)(dyn_arr_get (xs2, 2)))->a, 1e-2);
  assert_equal ('a', ((St *)(dyn_arr_get (xs2, 3)))->c);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  dyn_arr_free (xs2);
  retrieve_results ();
}

void
dyn_arr_test_insert (void)
{
  start_test_suite ("Dynamic array insert");

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (4, sizeof (s32));

  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 4 });

  dyn_arr_insert (xs, &(s32){ 5 }, 1);
  assert_equal (5, *(s32 *)(dyn_arr_get (xs, 1)));

  dyn_arr_insert (xs, &(s32){ 10 }, 2);
  assert_equal (10, *(s32 *)(dyn_arr_get (xs, 2)));

  dyn_arr_insert (xs, &(s32){ 7 }, 0);
  assert_equal (7, *(s32 *)(dyn_arr_get (xs, 0)));

  dyn_arr_insert (xs, &(s32){ 1 }, 6);
  assert_equal (1, *(s32 *)(dyn_arr_get (xs, 6)));

  dyn_arr_insert (xs, &(s32){ 2 }, 0);
  assert_equal (2, *(s32 *)(dyn_arr_get (xs, 0)));
  // -----------------------------------------------------------

  // Compound --------------------------------------------------
  DynArr *xs2 = dyn_arr_init (4, sizeof (St));
  St data[4] = {
    { 3.14, 8, 'a' }, { 2.16, 8, 'b' }, { 3.14, 16, 'd' }, { 1.23, 0, 'a' }
  };

  dyn_arr_insert (xs2, &data[0], 0);
  dyn_arr_insert (xs2, &data[1], 1);
  dyn_arr_insert (xs2, &data[2], 1);
  dyn_arr_insert (xs2, &data[3], 1);

  assert_equal (8, ((St *)(dyn_arr_get (xs2, 0)))->b);
  assert_equal ('b', ((St *)(dyn_arr_get (xs2, 3)))->c);
  assert_approx (3.14, ((St *)(dyn_arr_get (xs2, 2)))->a, 1e-2);
  assert_equal ('a', ((St *)(dyn_arr_get (xs2, 1)))->c);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  dyn_arr_free (xs2);
  retrieve_results ();
}

void
dyn_arr_test_pop (void)
{
  start_test_suite ("Dynamic array pop");

  // We'll only do simple for this one as it doesn't even care about the
  // stride

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (4, sizeof (s32));

  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 4 });

  dyn_arr_pop (xs);
  assert_equal (3, xs->len);
  assert_equal (4, xs->cap);

  dyn_arr_pop (xs);
  assert_equal (2, xs->len);
  assert_equal (4, xs->cap);

  dyn_arr_pop (xs);
  assert_equal (1, xs->len);
  assert_equal (4, xs->cap);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  retrieve_results ();
}

void
dyn_arr_test_delete (void)
{

  start_test_suite ("Dynamic array delete");

  // Simple ----------------------------------------------------
  DynArr *xs = dyn_arr_init (4, sizeof (s32));

  dyn_arr_push (xs, &(s32){ 4 });
  dyn_arr_push (xs, &(s32){ 5 });
  dyn_arr_push (xs, &(s32){ 7 });
  dyn_arr_push (xs, &(s32){ 10 });
  dyn_arr_push (xs, &(s32){ -1 });
  dyn_arr_push (xs, &(s32){ 3 });

  dyn_arr_delete (xs, 1);
  assert_equal (7, *(s32 *)(dyn_arr_get (xs, 1)));

  dyn_arr_delete (xs, 2);
  assert_equal (-1, *(s32 *)(dyn_arr_get (xs, 2)));

  dyn_arr_delete (xs, 0);
  assert_equal (7, *(s32 *)(dyn_arr_get (xs, 0)));

  dyn_arr_delete (xs, 2);
  dyn_arr_delete (xs, 1);
  assert_equal (7, *(s32 *)(dyn_arr_get (xs, 0)));
  // -----------------------------------------------------------

  // Compound --------------------------------------------------
  DynArr *xs2 = dyn_arr_init (4, sizeof (St));
  St data[4] = {
    { 3.14, 8, 'a' }, { 2.16, 8, 'b' }, { 3.14, 16, 'd' }, { 1.23, 0, 'a' }
  };

  dyn_arr_push (xs2, &data[0]);
  dyn_arr_push (xs2, &data[1]);
  dyn_arr_push (xs2, &data[2]);
  dyn_arr_push (xs2, &data[3]);

  dyn_arr_delete (xs2, 0);
  dyn_arr_delete (xs2, 0);

  assert_approx (3.14, ((St *)(dyn_arr_get (xs2, 0)))->a, 1e-2);
  assert_approx (1.23, ((St *)(dyn_arr_get (xs2, 1)))->a, 1e-2);
  // -----------------------------------------------------------

  dyn_arr_free (xs);
  dyn_arr_free (xs2);
  retrieve_results ();
}

void
dyn_arr_all_tests (void)
{
  dyn_arr_test_init ();
  dyn_arr_test_get ();
  dyn_arr_test_set ();
  dyn_arr_test_push ();
  dyn_arr_test_insert ();
  dyn_arr_test_pop ();
  dyn_arr_test_delete ();
}
