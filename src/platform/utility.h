#pragma once

// Used in Wayland layer
void buf_write_u32 (char *buf, unsigned long *buf_size, unsigned long buf_cap,
                    unsigned x);

void buf_write_u16 (char *buf, unsigned long *buf_size, unsigned long buf_cap,
                    unsigned short x);

void buf_write_string (char *buf, unsigned long *buf_size,
                       unsigned long buf_cap, char *src, unsigned src_len);

unsigned buf_read_u32 (char **buf, unsigned long *buf_size);

unsigned short buf_read_u16 (char **buf, unsigned long *buf_size);

void buf_read_n (char **buf, unsigned long *buf_size, char *dst,
                 unsigned long n);

// Used in X11 layer
unsigned round_up (unsigned value, unsigned alignment);

unsigned short read_u16_le (const unsigned char *buf);

unsigned read_u32_le (const unsigned char *buf);

unsigned short read_u16_be (const unsigned char *buf);

void write_u16_le (unsigned char *buf, unsigned short value);

void write_u32_le (unsigned char *buf, unsigned value);
