#define ASTF_STRIP_PREFIX
#include "utility.h"
#include "astf.h"
#include "utility.test.h"

#include <string.h>

static void
utility_test_round_up (void)
{
  start_test_suite ("Platform utility: round up");

  assert_equal (0, round_up (0, 4));
  assert_equal (4, round_up (1, 4));
  assert_equal (4, round_up (4, 4));
  assert_equal (8, round_up (5, 4));
  assert_equal (8, round_up (7, 4));
  assert_equal (8, round_up (8, 4));
  assert_equal (12, round_up (10, 3));
  assert_equal (15, round_up (13, 5));

  retrieve_results ();
}

static void
utility_test_endian_helpers (void)
{
  start_test_suite ("Platform utility: endian helpers");

  assert_equal (0x1234, read_u16_le ((u8[]){ 0x34, 0x12 }));
  assert_equal (0x1234, read_u16_be ((u8[]){ 0x12, 0x34 }));
  assert_equal (0x12345678, read_u32_le ((u8[]){ 0x78, 0x56, 0x34, 0x12 }));
  assert_equal (0, read_u16_le ((u8[]){ 0, 0 }));
  assert_equal (0xFFFF, read_u16_be ((u8[]){ 0xFF, 0xFF }));
  assert_equal (0xFFFFFFFF, read_u32_le ((u8[]){ 0xFF, 0xFF, 0xFF, 0xFF }));

  u8 encoded[4] = { 0 };
  write_u16_le (encoded, 0x1234);
  assert_equal (0x34, encoded[0]);
  assert_equal (0x12, encoded[1]);

  write_u16_le (encoded, 0xFFFF);
  assert_equal (0xFF, encoded[0]);
  assert_equal (0xFF, encoded[1]);

  write_u32_le (encoded, 0x12345678);
  assert_equal (0x78, encoded[0]);
  assert_equal (0x56, encoded[1]);
  assert_equal (0x34, encoded[2]);
  assert_equal (0x12, encoded[3]);
  assert_equal (0x12345678, read_u32_le (encoded));

  retrieve_results ();
}

static void
utility_test_buffer_serialization (void)
{
  start_test_suite ("Platform utility: aligned buffer serialization");

  u32 storage[12] = { 0 };
  byte *buf = (byte *)storage;
  u64 size = 0;

  buf_write_u32 (buf, &size, sizeof storage, 0x12345678);
  buf_write_u16 (buf, &size, sizeof storage, 0xABCD);
  assert_true (size == 6);

  // Keep the following u32 length field aligned, as required by the format.
  size += 2;
  byte first[] = "abc";
  buf_write_string (buf, &size, sizeof storage, first, 3);
  assert_true (size == 16);
  assert_equal (3, read_u32_le ((u8 *)(buf + 8)));
  assert_equal (0, memcmp (buf + 12, "abc", 3));
  assert_equal (0, buf[15]);

  byte *read_buf = buf;
  u64 remaining = size;
  assert_equal (0x12345678, buf_read_u32 (&read_buf, &remaining));
  assert_equal (0xABCD, buf_read_u16 (&read_buf, &remaining));
  assert_true (remaining == 10);

  read_buf += 2;
  remaining -= 2;
  assert_equal (3, buf_read_u32 (&read_buf, &remaining));

  byte copied[3] = { 0 };
  buf_read_n (&read_buf, &remaining, copied, sizeof copied);
  assert_equal (0, memcmp (copied, first, sizeof copied));
  assert_true (remaining == 1);
  assert_equal (0, *read_buf);

  retrieve_results ();
}

void
utility_all_tests (void)
{
  start_group ("Platform Utility Tests");

  utility_test_round_up ();
  utility_test_endian_helpers ();
  utility_test_buffer_serialization ();

  end_group ();
}
