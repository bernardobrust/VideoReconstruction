#pragma once

#include "macros.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN3(a, b, c) (MIN ((a), (b)) < (c) ? MIN ((a), (b)) : (c))
#define MAX3(a, b, c) (MAX ((a), (b)) > (c) ? MAX ((a), (b)) : (c))

#define ROUNDUP_4(n) (((n) + 3) & -4)

#define CSTRING_LEN(s) (sizeof (s) - 1)

// Computes floor(x / 255), very usefull in the renderer for alpha blending
#define DIV_255(x) (u8) ((x + 1 + ((x) >> 8)) >> 8)

s32 clamp_s32 (s32 v, s32 min, s32 max);
f32 clamp_f32 (f32 v, f32 min, f32 max);
f32 hypot_f32 (f32 a, f32 b);

s32 determinant_ab_ap_s32 (s32 ax, s32 ay, s32 bx, s32 by, s32 px, s32 py);