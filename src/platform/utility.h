#pragma once

#include "macros.h"

// Write and read aligned native-endian values in Wayland protocol buffers
void buf_write_u32 (byte *buf, u64 *buf_size, u64 buf_cap, u32 x);
void buf_write_u16 (byte *buf, u64 *buf_size, u64 buf_cap, u16 x);
void buf_write_string (byte *buf, u64 *buf_size, u64 buf_cap, byte *src,
                       u32 src_len);

u32 buf_read_u32 (byte **buf, u64 *buf_size);
u16 buf_read_u16 (byte **buf, u64 *buf_size);
void buf_read_n (byte **buf, u64 *buf_size, byte *dst, u64 n);

// Alignment and endian helpers used by the X11 protocol implementation
u32 round_up_to_multiple_u32 (u32 value, u32 alignment);

u16 read_u16_le (const u8 *buf);
u32 read_u32_le (const u8 *buf);
u16 read_u16_be (const u8 *buf);

void write_u16_le (u8 *buf, u16 value);
void write_u32_le (u8 *buf, u32 value);

// Return the number of characters required to format numeric values
u64 decimal_length_u64 (u64 value);
u64 decimal_length_s64 (s64 value);
u64 decimal_length_s32 (s32 value);
u64 decimal_length_fixed_3 (s64 value, bool negative_zero);

u64 fixed_3_magnitude (f32 value);
s64 fixed_3_scaled (f32 value);
