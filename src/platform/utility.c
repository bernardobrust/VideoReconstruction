#include <assert.h>
#include <string.h>

#include "basic.h"
#include "utility.h"

void
buf_write_u32 (char *buf, u64 *buf_size, u64 buf_cap, u32 x)
{
  assert (*buf_size + sizeof (x) <= buf_cap);
  assert (((u64)buf + *buf_size) % sizeof (x) == 0);

  memcpy (buf + *buf_size, &x, sizeof x);
  *buf_size += sizeof (x);
}

void
buf_write_u16 (char *buf, u64 *buf_size, u64 buf_cap, u16 x)
{
  assert (*buf_size + sizeof (x) <= buf_cap);
  assert (((u64)buf + *buf_size) % sizeof (x) == 0);

  *(u16 *)(buf + *buf_size) = x;
  *buf_size += sizeof (x);
}

void
buf_write_string (char *buf, u64 *buf_size, u64 buf_cap, char *src,
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
buf_read_u32 (char **buf, u64 *buf_size)
{
  assert (*buf_size >= sizeof (u32));
  assert ((u64)*buf % sizeof (u32) == 0);

  u32 res = *(u32 *)(*buf);
  *buf += sizeof (res);
  *buf_size -= sizeof (res);

  return res;
}

u16
buf_read_u16 (char **buf, u64 *buf_size)
{
  assert (*buf_size >= sizeof (u16));
  assert ((u64)*buf % sizeof (u16) == 0);

  u16 res = *(u16 *)(*buf);
  *buf += sizeof (res);
  *buf_size -= sizeof (res);

  return res;
}

void
buf_read_n (char **buf, u64 *buf_size, char *dst, u64 n)
{
  assert (*buf_size >= n);

  memcpy (dst, *buf, n);
  *buf += n;
  *buf_size -= n;
}

inline u32
round_up (u32 value, u32 alignment)
{
  return (value + alignment - 1) / alignment * alignment;
}

inline u16
read_u16_le (const u8 *buf)
{
  return (u16)(buf[0] | ((u16)buf[1] << 8));
}

inline u32
read_u32_le (const u8 *buf)
{
  return (u32)buf[0] | ((u32)buf[1] << 8) | ((u32)buf[2] << 16)
         | ((u32)buf[3] << 24);
}

inline u16
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
