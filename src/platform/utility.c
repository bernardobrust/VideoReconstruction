#include <assert.h>
#include <string.h>

#include "basic.h"
#include "utility.h"

void
buf_write_u32 (char *buf, unsigned long *buf_size, unsigned long buf_cap,
               unsigned x)
{
  assert (*buf_size + sizeof (x) <= buf_cap);
  assert (((size_t)buf + *buf_size) % sizeof (x) == 0);

  *(unsigned *)(buf + *buf_size) = x;
  *buf_size += sizeof (x);
}

void
buf_write_u16 (char *buf, unsigned long *buf_size, unsigned long buf_cap,
               unsigned short x)
{
  assert (*buf_size + sizeof (x) <= buf_cap);
  assert (((size_t)buf + *buf_size) % sizeof (x) == 0);

  *(unsigned short *)(buf + *buf_size) = x;
  *buf_size += sizeof (x);
}

void
buf_write_string (char *buf, unsigned long *buf_size, unsigned long buf_cap,
                  char *src, unsigned src_len)
{
  assert (*buf_size + src_len <= buf_cap);

  buf_write_u32 (buf, buf_size, buf_cap, src_len);
  memcpy (buf + *buf_size, src, ROUNDUP_4 (src_len));
  *buf_size += ROUNDUP_4 (src_len);
}

unsigned
buf_read_u32 (char **buf, unsigned long *buf_size)
{
  assert (*buf_size >= sizeof (unsigned));
  assert ((size_t)*buf % sizeof (unsigned) == 0);

  unsigned res = *(unsigned *)(*buf);
  *buf += sizeof (res);
  *buf_size -= sizeof (res);

  return res;
}

unsigned short
buf_read_u16 (char **buf, unsigned long *buf_size)
{
  assert (*buf_size >= sizeof (unsigned short));
  assert ((size_t)*buf % sizeof (unsigned short) == 0);

  unsigned short res = *(unsigned short *)(*buf);
  *buf += sizeof (res);
  *buf_size -= sizeof (res);

  return res;
}

void
buf_read_n (char **buf, unsigned long *buf_size, char *dst, unsigned long n)
{
  assert (*buf_size >= n);

  memcpy (dst, *buf, n);
  *buf += n;
  *buf_size -= n;
}

unsigned
round_up (unsigned value, unsigned alignment)
{
  return (value + alignment - 1) / alignment * alignment;
}

unsigned short
read_u16_le (const unsigned char *buf)
{
  return (unsigned short)(buf[0] | ((unsigned short)buf[1] << 8));
}

unsigned
read_u32_le (const unsigned char *buf)
{
  return (unsigned)buf[0] | ((unsigned)buf[1] << 8) | ((unsigned)buf[2] << 16)
         | ((unsigned)buf[3] << 24);
}

unsigned short
read_u16_be (const unsigned char *buf)
{
  return (unsigned short)(((unsigned short)buf[0] << 8) | buf[1]);
}

void
write_u16_le (unsigned char *buf, unsigned short value)
{
  buf[0] = (unsigned char)value;
  buf[1] = (unsigned char)(value >> 8);
}

void
write_u32_le (unsigned char *buf, unsigned value)
{
  buf[0] = (unsigned char)value;
  buf[1] = (unsigned char)(value >> 8);
  buf[2] = (unsigned char)(value >> 16);
  buf[3] = (unsigned char)(value >> 24);
}
