#define ASTF_STRIP_PREFIX
#include "astf.h"

#include <stdio.h>
#include <string.h>

#include "dyn_arr.c"
#include "dyn_arr.h"
#include "dyn_arr.test.h"

typedef struct
{
  double a;
  int b;
  char c;
} St;

void
dyn_arr_test_init ()
{
  start_test_suite ("Dynamic array initialization");

  DynArr *xs = dyn_arr_init (2, sizeof (int));

  assert_not_null (xs);
  assert_not_null (xs->data);
  assert_equal (2, xs->cap);
  assert_equal (0, xs->len);
  assert_equal (4, xs->stride);

  DynArr *xs2 = dyn_arr_init (4, sizeof (St));

  assert_not_null (xs2);
  assert_not_null (xs2->data);
  assert_equal (4, xs2->cap);
  assert_equal (0, xs2->len);
  // It's not 13, know your alignments!
  assert_equal (16, xs2->stride);

  retrieve_results ();
}

void
dyn_arr_test_get ()
{
  start_test_suite ("Dynamic array get");

  DynArr *xs = dyn_arr_init (4, sizeof (int));
  xs->len = 4;
  int data[4] = { 4, 3, 2, 1 };
  memcpy (xs->data, data, 4 * 4);

  assert_equal (4, *(int *)(dyn_arr_get (xs, 0)));
  assert_equal (3, *(int *)(dyn_arr_get (xs, 1)));
  assert_equal (2, *(int *)(dyn_arr_get (xs, 2)));
  assert_equal (1, *(int *)(dyn_arr_get (xs, 3)));

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

  retrieve_results ();
}

void
dyn_arr_test_set ()
{
  start_test_suite ("Dynamic array set");

  DynArr *xs = dyn_arr_init (4, sizeof (int));
  xs->len = 4;

  // Coumpount literals are better here
  dyn_arr_set (xs, 0, &(int){ 4 });
  dyn_arr_set (xs, 1, &(int){ 3 });
  dyn_arr_set (xs, 2, &(int){ 2 });
  dyn_arr_set (xs, 3, &(int){ 1 });

  assert_equal (4, *(int *)(dyn_arr_get (xs, 0)));
  assert_equal (3, *(int *)(dyn_arr_get (xs, 1)));
  assert_equal (2, *(int *)(dyn_arr_get (xs, 2)));
  assert_equal (1, *(int *)(dyn_arr_get (xs, 3)));

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

  retrieve_results ();
}

void
dyn_arr_all_tests ()
{
  dyn_arr_test_init ();
  dyn_arr_test_get ();
  dyn_arr_test_set ();
}
