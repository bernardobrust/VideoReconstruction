#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dyn_arr.h"

bool
dyn_arr64_try_init (DynArr64 *xs, u64 initial_cap, u64 stride)
{
  void *data;

  assert (xs != NULL);
  assert (stride > 0);

  if (initial_cap > SIZE_MAX / stride)
    return false;

  data = initial_cap == 0 ? NULL : malloc ((size_t)(stride * initial_cap));
  if (initial_cap > 0 && data == NULL)
    return false;

  *xs = (DynArr64){ .cap = initial_cap,
                    .len = 0,
                    .stride = stride,
                    .data = data,
                    .fixed_capacity = false };

  return true;
}

bool
dyn_arr64_try_reserve (DynArr64 *xs, u64 min_capacity)
{
  u64 new_cap;
  void *data;

  assert (xs != NULL);
  assert (xs->stride > 0);

  if (min_capacity <= xs->cap)
    return true;

  if (xs->fixed_capacity || min_capacity > SIZE_MAX / xs->stride)
    return false;

  new_cap = xs->cap == 0 ? 1 : xs->cap;
  while (new_cap < min_capacity)
    {
      if (new_cap > SIZE_MAX / 2)
        {
          new_cap = min_capacity;
          break;
        }

      new_cap *= 2;
    }

  if (new_cap > SIZE_MAX / xs->stride)
    new_cap = min_capacity;

  data = realloc (xs->data, (size_t)(new_cap * xs->stride));
  if (data == NULL)
    return false;

  xs->data = data;
  xs->cap = new_cap;

  return true;
}

bool
dyn_arr64_try_append (DynArr64 *xs, const void *new_elems, u64 count)
{
  assert (xs != NULL);
  assert (xs->stride > 0);
  assert (count == 0 || new_elems != NULL);

  if (count == 0)
    return true;

  if (count > UINT64_MAX - xs->len
      || !dyn_arr64_try_reserve (xs, xs->len + count))
    return false;

  memcpy ((u8 *)xs->data + (size_t)(xs->len * xs->stride), new_elems,
          (size_t)(count * xs->stride));
  xs->len += count;

  return true;
}

void
dyn_arr64_release (DynArr64 *xs)
{
  assert (xs != NULL);

  if (!xs->fixed_capacity)
    free (xs->data);

  *xs = (DynArr64){ 0 };
}

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
  xs->fixed_capacity = false;

  return xs;
}

void
dyn_arr_free (DynArr *xs)
{
  assert (xs != NULL);
  assert (xs->data != NULL || xs->fixed_capacity);

  if (!xs->fixed_capacity)
    {
      free (xs->data);
      free (xs);
    }
  else
    *xs = (DynArr){ 0 };
}

DynArr
dyn_arr_init_fixed (void *storage, s32 capacity, s32 stride)
{
  assert (storage != NULL && capacity > 0 && stride > 0);

  return (DynArr){ .cap = capacity,
                   .len = 0,
                   .stride = stride,
                   .data = storage,
                   .fixed_capacity = true };
}

void *
dyn_arr_get (DynArr *xs, s32 where)
{
  assert (xs != NULL);
  assert (xs->data != NULL);
  assert (where < xs->len);

  void *r = (u8 *)xs->data + (where * xs->stride);
  assert (r != NULL);

  return r;
}

void
dyn_arr_set (DynArr *xs, s32 where, void *val)
{
  assert (xs != NULL);
  assert (xs->data != NULL);
  assert (where < xs->len);

  memcpy ((u8 *)xs->data + (where * xs->stride), val, xs->stride);
}

void
dyn_arr_push (DynArr *xs, void *new_elem)
{
  bool pushed = dyn_arr_try_push (xs, new_elem);
  assert (pushed);
  (void)pushed;
}

bool
dyn_arr_try_push (DynArr *xs, void *new_elem)
{
  assert (xs != NULL);
  assert (xs->data != NULL);

  if (xs->len >= xs->cap)
    {
      if (xs->fixed_capacity)
        return false;

      void *data = realloc (xs->data, (size_t)xs->len * xs->stride * 2);
      assert (data != NULL);
      xs->data = data;
      xs->cap *= 2;
    }

  dyn_arr_set (xs, xs->len++, new_elem);
  return true;
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
      if (xs->fixed_capacity)
        {
          assert (!xs->fixed_capacity && "fixed-capacity array is full");
          return;
        }

      void *data = realloc (xs->data, (size_t)xs->len * xs->stride * 2);
      assert (data != NULL);
      xs->data = data;
      xs->cap *= 2;
    }

  // Move memory from where forward by 1, opening up space for new_elem at
  // where
  memmove (((u8 *)(xs->data) + ((where + 1) * xs->stride)),
           ((u8 *)(xs->data) + (where * xs->stride)),
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
  memmove (((u8 *)(xs->data) + (where * xs->stride)),
           ((u8 *)(xs->data) + ((where + 1) * xs->stride)),
           (xs->len - where - 1) * xs->stride);
  --xs->len;
}
