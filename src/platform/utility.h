#pragma once

#include "macros.h"

// Used in Wayland layer
void buf_write_u32 (char *buf, u64 *buf_size, u64 buf_cap, u32 x);
void buf_write_u16 (char *buf, u64 *buf_size, u64 buf_cap, u16 x);
void buf_write_string (char *buf, u64 *buf_size, u64 buf_cap, char *src,
                       u32 src_len);

u32 buf_read_u32 (char **buf, u64 *buf_size);
u16 buf_read_u16 (char **buf, u64 *buf_size);
void buf_read_n (char **buf, u64 *buf_size, char *dst, u64 n);

// Used in X11 layer
u32 round_up (u32 value, u32 alignment);

u16 read_u16_le (const u8 *buf);
u32 read_u32_le (const u8 *buf);
u16 read_u16_be (const u8 *buf);

void write_u16_le (u8 *buf, u16 value);
void write_u32_le (u8 *buf, u32 value);
