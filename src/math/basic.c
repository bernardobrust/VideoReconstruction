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

  return max_val + 0.3375f * min_val; // Maximum relative error is about 5.5%
}

// Return the signed doubled area of triangle ABP. The sign identifies which
// side of directed segment AB contains P, and zero means the points are collinear
s32
determinant_ab_ap_s32 (s32 ax, s32 ay, s32 bx, s32 by, s32 px, s32 py)
{
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}
