#pragma once

#include "macros.h"
#include "platform.h"
#include "thread.h"

// Plase check docs/export_format for the file format specification

typedef struct
{
  s32 src_x, src_y, dst_x, dst_y, width, height, source;
  // 3 deciaml digits of precision, max precision of AV1 dedcoded motion
  // vectors is 1/8 pixel
  s64 magnitude_1000, angle_1000, normalized_x_1000, normalized_y_1000;
  u64 line_length;
  u8 negative_mask;
} ExportMotionVector;

typedef struct
{
  s64 pts;
  u64 first_vector, vector_count, output_offset, output_size;
} ExportFrame;

typedef struct
{
  PlatformMappedFile *output;
  ExportFrame *frames;
  ExportMotionVector *vectors;

  u64 first_frame, end_frame, buffered, output_position, buffer_size;
  byte *buffer;
  bool failed;
} ExportWorker;

bool export_data_to (const char *video_file, const char *data_file,
                     u64 output_buffer_size);
