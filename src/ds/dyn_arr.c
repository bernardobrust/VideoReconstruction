#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dyn_arr.h"

DynArr *
dyn_arr_init (int initial_cap, int stride)
{
  assert (initial_cap > 1);
  assert (stride > 1);

  void *data = malloc (stride * initial_cap);
  assert (data != NULL);

  DynArr *xs = (DynArr *)malloc (sizeof (DynArr));
  assert (xs != NULL);

  xs->cap = initial_cap;
  xs->len = 0;
  xs->stride = stride;
  xs->data = data;

  return xs;
}

void
dyn_arr_free (DynArr *xs)
{
  free (xs->data);
  free (xs);
}

void *
dyn_arr_get (DynArr *xs, int where)
{
  // Slow, but gets removed in release builds
  assert (where < xs->len);

  void *r = (unsigned char *)xs->data + (where * xs->stride);
  assert (r != NULL);

  return r;
}

void
dyn_arr_set (DynArr *xs, int where, void *val)
{
  assert (where < xs->len);

  memcpy ((unsigned char *)xs->data + (where * xs->stride), val, xs->stride);
}
