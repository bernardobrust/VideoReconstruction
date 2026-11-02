#include <assert.h>
#include <math.h>
#include <string.h>

#include "basic.h"
#include "utility.h"

// Buffer functions
void
buf_write_u32 (byte *buf, u64 *buf_size, u64 buf_cap, u32 x)
{
  assert (*buf_size + sizeof (x) <= buf_cap);
  assert (((u64)buf + *buf_size) % sizeof (x) == 0);

  memcpy (buf + *buf_size, &x, sizeof x);
  *buf_size += sizeof (x);
}

void
buf_write_u16 (byte *buf, u64 *buf_size, u64 buf_cap, u16 x)
{
  assert (*buf_size + sizeof (x) <= buf_cap);
  assert (((u64)buf + *buf_size) % sizeof (x) == 0);

  *(u16 *)(buf + *buf_size) = x;
  *buf_size += sizeof (x);
}

void
buf_write_string (byte *buf, u64 *buf_size, u64 buf_cap, byte *src,
                  u32 src_len)
{
  u32 padded_len = ROUNDUP_4 (src_len);
  assert (*buf_size + padded_len <= buf_cap);

  buf_write_u32 (buf, buf_size, buf_cap, src_len);
  memcpy (buf + *buf_size, src, src_len);
  memset (buf + *buf_size + src_len, 0, padded_len - src_len);

  *buf_size += ROUNDUP_4 (src_len);
}

u32
buf_read_u32 (byte **buf, u64 *buf_size)
{
  assert (*buf_size >= sizeof (u32));
  assert ((u64)*buf % sizeof (u32) == 0);

  u32 res = *(u32 *)(*buf);
  *buf += sizeof (res);
  *buf_size -= sizeof (res);

  return res;
}

u16
buf_read_u16 (byte **buf, u64 *buf_size)
{
  assert (*buf_size >= sizeof (u16));
  assert ((u64)*buf % sizeof (u16) == 0);

  u16 res = *(u16 *)(*buf);
  *buf += sizeof (res);
  *buf_size -= sizeof (res);

  return res;
}

void
buf_read_n (byte **buf, u64 *buf_size, byte *dst, u64 n)
{
  assert (*buf_size >= n);

  memcpy (dst, *buf, n);
  *buf += n;
  *buf_size -= n;
}

u32
round_up_to_multiple_u32 (u32 value, u32 alignment)
{
  return (value + alignment - 1) / alignment * alignment;
}

u16
read_u16_le (const u8 *buf)
{
  return (u16)(buf[0] | ((u16)buf[1] << 8));
}

u32
read_u32_le (const u8 *buf)
{
  return (u32)buf[0] | ((u32)buf[1] << 8) | ((u32)buf[2] << 16)
         | ((u32)buf[3] << 24);
}

u16
read_u16_be (const u8 *buf)
{
  return (u16)(((u16)buf[0] << 8) | buf[1]);
}

void
write_u16_le (u8 *buf, u16 value)
{
  buf[0] = (u8)value;
  buf[1] = (u8)(value >> 8);
}

void
write_u32_le (u8 *buf, u32 value)
{
  buf[0] = (u8)value;
  buf[1] = (u8)(value >> 8);
  buf[2] = (u8)(value >> 16);
  buf[3] = (u8)(value >> 24);
}

// Formatting functions
u64
decimal_length_u64 (u64 value)
{
  persist const u64 powers[] = { 10ULL,
                                 100ULL,
                                 1000ULL,
                                 10000ULL,
                                 100000ULL,
                                 1000000ULL,
                                 10000000ULL,
                                 100000000ULL,
                                 1000000000ULL,
                                 10000000000ULL,
                                 100000000000ULL,
                                 1000000000000ULL,
                                 10000000000000ULL,
                                 100000000000000ULL,
                                 1000000000000000ULL,
                                 10000000000000000ULL,
                                 100000000000000000ULL,
                                 1000000000000000000ULL,
                                 10000000000000000000ULL };
  u64 low = 0, high = sizeof (powers) / sizeof (powers[0]), middle;

  while (low < high)
    {
      middle = low + (high - low) / 2;

      if (value < powers[middle])
        high = middle;
      else
        low = middle + 1;
    }

  return low + 1;
}

u64
decimal_length_s64 (s64 value)
{
  u64 magnitude = value < 0 ? (u64)(-(value + 1)) + 1 : (u64)value;

  return decimal_length_u64 (magnitude) + (value < 0);
}

u64
decimal_length_s32 (s32 value)
{
  u32 magnitude = value < 0 ? (u32)(-(s64)value) : (u32)value;

  if (magnitude < 10)
    return 1 + (value < 0);
  if (magnitude < 100)
    return 2 + (value < 0);
  if (magnitude < 1000)
    return 3 + (value < 0);
  if (magnitude < 10000)
    return 4 + (value < 0);
  if (magnitude < 100000)
    return 5 + (value < 0);
  if (magnitude < 1000000)
    return 6 + (value < 0);
  if (magnitude < 10000000)
    return 7 + (value < 0);
  if (magnitude < 100000000)
    return 8 + (value < 0);
  if (magnitude < 1000000000)
    return 9 + (value < 0);

  return 10 + (value < 0);
}

u64
decimal_length_fixed_3 (s64 value, bool negative_zero)
{
  u64 magnitude = value < 0 ? (u64)(-(value + 1)) + 1 : (u64)value;

  return decimal_length_u64 (magnitude / 1000) + 4
         + (value < 0 || negative_zero);
}

u64
fixed_3_magnitude (f32 value)
{
  f64 scaled = fabs ((f64)value) * 1000.0;
  u64 rounded = (u64)scaled;
  f64 fraction = scaled - (f64)rounded;

  if (fraction > 0.5 || (fraction == 0.5 && (rounded & 1)))
    ++rounded;

  assert (rounded <= INT64_MAX);

  return rounded;
}

s64
fixed_3_scaled (f32 value)
{
  u64 magnitude = fixed_3_magnitude (value);

  return signbit (value) ? -(s64)magnitude : (s64)magnitude;
}