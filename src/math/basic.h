#pragma once

#define MIN (a, b) ((a) < (b) ? (a) : (b))
#define ROUNDUP_4(n) (((n) + 3) & -4)
#define CSTRING_LEN(s) (sizeof (s) - 1)

int clamp_int (int v, int min, int max);
