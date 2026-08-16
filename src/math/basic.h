#pragma once

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN3(a, b, c) (MIN ((a), (b)) < (c) ? MIN ((a), (b)) : (c))
#define MAX3(a, b, c) (MAX ((a), (b)) > (c) ? MAX ((a), (b)) : (c))
#define ROUNDUP_4(n) (((n) + 3) & -4)
#define CSTRING_LEN(s) (sizeof (s) - 1)

int clamp_int (int v, int min, int max);
int determinant_ab_ap_int (int ax, int ay, int bx, int by, int px, int py);