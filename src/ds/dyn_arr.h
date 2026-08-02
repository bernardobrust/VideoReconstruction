/*
** Dynamic array data structure
** cap: total amount of elements the array can fit
** len: elements in the array
** stride: size of the data type of the array
**
** Important notes:
** insert and delete are expensive, avoid them as much as possible
*/

#pragma once

typedef struct
{
  int cap, len, stride;
  void *data;
} DynArr;

DynArr *dyn_arr_init (int initial_cap, int stride);
void dyn_arr_free (DynArr *xs);

void *dyn_arr_get (DynArr *xs, int where);
void dyn_arr_set (DynArr *xs, int where, void *val);

void dyn_arr_push (DynArr *xs, void *new_elem);
void dyn_arr_insert (DynArr *xs, void *new_elem, int where);
void dyn_arr_pop (DynArr *xs);
void dyn_arr_delete (DynArr *xs, int where);
