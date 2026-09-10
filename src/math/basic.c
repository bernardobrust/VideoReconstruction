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