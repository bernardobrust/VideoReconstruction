#define ASTF_STRIP_PREFIX
#include "utility.h"
#include "astf.h"
#include "utility.test.h"
#include <string.h>

void
utility_all_tests (void)
{
  start_test_suite ("Platform serialization and endian helpers");

  assert_equal (12, round_up (10, 4));

  assert_equal (0x1234, read_u16_le ((u8[]){ 0x34, 0x12 }));
  assert_equal (0x12345678, read_u32_le ((u8[]){ 0x78, 0x56, 0x34, 0x12 }));
  assert_equal (0x1234, read_u16_be ((u8[]){ 0x12, 0x34 }));

  u8 encoded[4];
  write_u16_le (encoded, 0x1234);

  assert_equal (0x34, encoded[0]);
  assert_equal (0x12, encoded[1]);

  write_u32_le (encoded, 0x12345678);

  assert_equal (0x78, encoded[0]);
  assert_equal (0x12, encoded[3]);

  u32 storage[8] = { 0 };
  byte *buf = (byte *)storage;
  u64 size = 0;

  buf_write_u32 (buf, &size, sizeof storage, 0x12345678);
  buf_write_u16 (buf, &size, sizeof storage, 0xABCD);

  size += 2;
  byte str[] = "abc";
  buf_write_string (buf, &size, sizeof storage, str, 3);

  assert_true (size == 16);

  byte *read_buf = buf;
  u64 remaining = size;

  assert_equal (0x12345678, buf_read_u32 (&read_buf, &remaining));
  assert_equal (0xABCD, buf_read_u16 (&read_buf, &remaining));

  read_buf += 2;
  remaining -= 2;

  assert_equal (3, buf_read_u32 (&read_buf, &remaining));

  byte copied[3];

  buf_read_n (&read_buf, &remaining, copied, 3);
  assert_equal (0, memcmp (copied, "abc", 3));
  assert_true (remaining == 1);
  assert_equal (0, *read_buf);

  retrieve_results ();
}
