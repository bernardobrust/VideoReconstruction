#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dyn_arr.h"

/*
** Note that the implementations are assert filled
** This causes slow debug and test builds for correctness sake
*/

DynArr *
dyn_arr_init (s32 initial_cap, s32 stride)
{
  assert (initial_cap >= 1);
  assert (stride >= 1);

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
  assert (xs != NULL);
  assert (xs->data != NULL);

  free (xs->data);
  free (xs);
}

void *
dyn_arr_get (DynArr *xs, s32 where)
{
  assert (xs != NULL);
  assert (xs->data != NULL);
  assert (where < xs->len);

  void *r = (u8*)xs->data + (where * xs->stride);
  assert (r != NULL);

  return r;
}

void
dyn_arr_set (DynArr *xs, s32 where, void *val)
{
  assert (xs != NULL);
  assert (xs->data != NULL);
  assert (where < xs->len);

  memcpy ((u8*)xs->data + (where * xs->stride), val, xs->stride);
}

void
dyn_arr_push (DynArr *xs, void *new_elem)
{
  assert (xs != NULL);
  assert (xs->data != NULL);

  // Resize needed (factor of 2 by default)
  if (xs->len >= xs->cap)
    {
      xs->data = realloc (xs->data, xs->len * xs->stride * 2);
      assert (xs->data != NULL);
      xs->cap *= 2;
    }

  dyn_arr_set (xs, xs->len++, new_elem);
}

void
dyn_arr_insert (DynArr *xs, void *new_elem, s32 where)
{
  assert (xs != NULL);
  assert (xs->data != NULL);
  assert (where <= xs->len);

  // Resize needed (factor of 2 by default)
  if (xs->len >= xs->cap)
    {
      xs->data = realloc (xs->data, xs->len * xs->stride * 2);
      assert (xs->data != NULL);
      xs->cap *= 2;
    }

  // Move memory from where forward by 1, opening up space for new_elem at
  // where
  memmove (((u8*)(xs->data) + ((where + 1) * xs->stride)),
           ((u8*)(xs->data) + (where * xs->stride)),
           (xs->len - where) * xs->stride);
  ++xs->len;
  dyn_arr_set (xs, where, new_elem);
}

void
dyn_arr_pop (DynArr *xs)
{
  assert (xs != NULL);
  assert (xs->data != NULL);

  // We could ignore the operation and return doing nothing, but if this assert
  // fails it's likely a programming error
  assert (xs->len > 0);

  // We don't shrink the array
  --xs->len;
}

void
dyn_arr_delete (DynArr *xs, s32 where)
{
  assert (xs != NULL);
  assert (xs->data != NULL);
  assert (where >= 0 && where < xs->len);

  // Move memory from where backward by 1, eating up the slot at where
  memmove (((u8*)(xs->data) + (where * xs->stride)),
           ((u8*)(xs->data) + ((where + 1) * xs->stride)),
           (xs->len - where - 1) * xs->stride);
  --xs->len;
}
