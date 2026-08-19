#include "basic.h"

inline int
clamp_int (int v, int min, int max)
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
inline int
determinant_ab_ap_int (int ax, int ay, int bx, int by, int px, int py)
{
  return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}