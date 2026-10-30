/*
** Dynamic array data structure
** cap: total amount of elements the array can fit
** len: elements in the array
** stride: size of the data type of the array
**
** Important note:
** Insert and delete are expensive, avoid them as much as possible
*/

#pragma once

#include <stdbool.h>

#include "macros.h"

typedef struct
{
  s32 cap, len, stride;
  void *data;
  bool fixed_capacity;
} DynArr;

typedef struct
{
  u64 cap, len, stride;
  void *data;
  bool fixed_capacity;
} DynArr64;

// Fallible 64 bit array operations for large collections (like the export
// buffer). These functions initialize and operate on a caller-owned DynArr64
bool dyn_arr64_try_init (DynArr64 *xs, u64 initial_cap, u64 stride);
bool dyn_arr64_try_reserve (DynArr64 *xs, u64 min_capacity);
bool dyn_arr64_try_append (DynArr64 *xs, const void *new_elems, u64 count);
void dyn_arr64_release (DynArr64 *xs);

DynArr *dyn_arr_init (s32 initial_cap, s32 stride);
void dyn_arr_free (DynArr *xs);
DynArr dyn_arr_init_fixed (void *storage, s32 capacity, s32 stride);
bool dyn_arr_try_push (DynArr *xs, void *new_elem);

void *dyn_arr_get (DynArr *xs, s32 where);
void dyn_arr_set (DynArr *xs, s32 where, void *val);

void dyn_arr_push (DynArr *xs, void *new_elem);
void dyn_arr_insert (DynArr *xs, void *new_elem, s32 where);
void dyn_arr_pop (DynArr *xs);
void dyn_arr_delete (DynArr *xs, s32 where);
