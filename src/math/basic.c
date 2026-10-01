#include <math.h>

#include "basic.h"

s32
clamp_s32 (s32 v, s32 min, s32 max)
{
  if (v < min)
    return min;

  if (v > max)
    return max;

  return v;
}

f32
clamp_f32 (f32 v, f32 min, f32 max)
{
  if (v < min)
    return min;

  if (v > max)
    return max;

  return v;
}

f32
hypot_f32 (f32 a, f32 b)
{
  f32 abs_a = fabsf (a), abs_b = fabsf (b), max_val = MAX (abs_a, abs_b),
      min_val = MIN (abs_a, abs_b);

  return max_val + 0.3375f * min_val; // Max relative error aprox. 5.5%
}

/*
Returns det(AB, AP), where
det |Px - Ax,  Py - Ay|
    |Bx - Ax,  By - Ay|

Positive if P is on one side of AB
Negative if P is on the other side
Zero     if A, B, and P are collinear

The result is also 2 * signed area of the triangle ABP
*/
s32
determinant_ab_ap_s32 (s32 ax, s32 ay, s32 bx, s32 by, s32 px, s32 py)
{
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}