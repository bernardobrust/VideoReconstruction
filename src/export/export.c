#include "export.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/frame.h>
#include <libavutil/motion_vector.h>

#include "basic.h"
#include "decode.h"
#include "dyn_arr.h"
#include "platform.h"
#include "thread.h"

// Read this from a config file later
#define EXPORT_BUFFER_SIZE (128 * 1024)

typedef struct
{
  s32 src_x, src_y, dst_x, dst_y, width, height, source;
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
  const ExportFrame *frames;
  const ExportMotionVector *vectors;

  u64 first_frame, end_frame;
  u64 buffered, output_position;
  bool failed;

  byte buffer[EXPORT_BUFFER_SIZE];
} ExportWorker;

local inline void vector_values (const ExportMotionVector *mv, f32 *magnitude,
                                 f32 *angle, f32 *normalized_x,
                                 f32 *normalized_y);
local inline s64 fixed_3_scaled (f32 value);
local inline u64 vector_line_length (u64 index,
                                    const ExportMotionVector *mv);

local inline u64
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

local inline s64
fixed_3_scaled (f32 value)
{
  u64 magnitude = fixed_3_magnitude (value);

  return signbit (value) ? -(s64)magnitude : (s64)magnitude;
}

local inline bool
append_vectors (DynArr64 *vectors, const AVFrame *frame)
{
  AVFrameSideData *side_data;
  const AVMotionVector *source;
  ExportMotionVector *destination;
  u64 vector_count, i;

  assert (vectors != NULL);
  assert (frame != NULL);

  side_data = av_frame_get_side_data (frame, AV_FRAME_DATA_MOTION_VECTORS);

  if (side_data == NULL)
    return true;

  vector_count = side_data->size / sizeof (AVMotionVector);
  if (vector_count == 0)
    return true;

  if (vector_count > UINT64_MAX - vectors->len
      || vector_count > SIZE_MAX / sizeof (*destination)
      || !dyn_arr64_try_reserve (vectors, vectors->len + vector_count))
    return false;

  source = (const AVMotionVector *)side_data->data;
  destination = (ExportMotionVector *)vectors->data + vectors->len;

  for (i = 0; i < vector_count; ++i)
    {
      ExportMotionVector *mv = &destination[i];

      mv->src_x = source[i].src_x;
      mv->src_y = source[i].src_y;
      mv->dst_x = source[i].dst_x;
      mv->dst_y = source[i].dst_y;
      mv->width = source[i].w;
      mv->height = source[i].h;
      mv->source = source[i].source;

      f32 magnitude, angle, normalized_x, normalized_y;

      vector_values (mv, &magnitude, &angle, &normalized_x, &normalized_y);

      mv->magnitude_1000 = fixed_3_scaled (magnitude);
      mv->angle_1000 = fixed_3_scaled (angle);
      mv->normalized_x_1000 = fixed_3_scaled (normalized_x);
      mv->normalized_y_1000 = fixed_3_scaled (normalized_y);
      mv->negative_mask = (signbit (magnitude) ? 1 : 0)
                          | (signbit (angle) ? 2 : 0)
                          | (signbit (normalized_x) ? 4 : 0)
                          | (signbit (normalized_y) ? 8 : 0);
      mv->line_length = vector_line_length (i, mv);
    }

  vectors->len += vector_count;

  return true;
}

local inline s32
read_decoded_frame (VideoPlex *video, bool *flushing)
{
  assert (video != NULL);
  assert (flushing != NULL);

  for (;;)
    {
      s32 result = avcodec_receive_frame (video->codec, video->frame);

      if (result == 0)
        return 1;

      if (result == AVERROR_EOF)
        return 0;

      if (result != AVERROR (EAGAIN))
        return -1;

      if (*flushing)
        return 0;

      result = av_read_frame (video->fmt, video->packet);
      if (result < 0)
        {
          *flushing = true;
          result = avcodec_send_packet (video->codec, NULL);
          if (result < 0 && result != AVERROR_EOF)
            return -1;

          continue;
        }

      if (video->packet->stream_index != video->video_stream)
        {
          av_packet_unref (video->packet);
          continue;
        }

      result = avcodec_send_packet (video->codec, video->packet);
      av_packet_unref (video->packet);

      if (result < 0 && result != AVERROR (EAGAIN))
        return -1;
    }
}

local inline void
vector_values (const ExportMotionVector *mv, f32 *magnitude, f32 *angle,
               f32 *normalized_x, f32 *normalized_y)
{
  f32 dx, dy;

  assert (mv != NULL);
  assert (magnitude != NULL);
  assert (angle != NULL);
  assert (normalized_x != NULL);
  assert (normalized_y != NULL);

  dx = (f32)(mv->dst_x - mv->src_x);
  dy = (f32)(mv->dst_y - mv->src_y);

  *magnitude = hypotf (dx, dy);
  *angle = atan2f (dy, dx);
  *normalized_x = *magnitude == 0.0f ? 0.0f : dx / *magnitude;
  *normalized_y = *magnitude == 0.0f ? 0.0f : dy / *magnitude;
}

local inline u64
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
  u64 low = 0, high = sizeof (powers) / sizeof (powers[0]);

  while (low < high)
    {
      u64 middle = low + (high - low) / 2;

      if (value < powers[middle])
        high = middle;
      else
        low = middle + 1;
    }

  return low + 1;
}

local inline u64
decimal_length_s64 (s64 value)
{
  u64 magnitude = value < 0 ? (u64)(-(value + 1)) + 1 : (u64)value;

  return decimal_length_u64 (magnitude) + (value < 0);
}

local inline u64
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

local inline u64
decimal_length_fixed_3 (s64 value, bool negative_zero)
{
  u64 magnitude = value < 0 ? (u64)(-(value + 1)) + 1 : (u64)value;

  return decimal_length_u64 (magnitude / 1000) + 4
         + (value < 0 || negative_zero);
}

local inline char *
append_u64 (char *output, u64 value)
{
  persist const char digit_pairs[]
      = "00010203040506070809"
        "10111213141516171819"
        "20212223242526272829"
        "30313233343536373839"
        "40414243444546474849"
        "50515253545556575859"
        "60616263646566676869"
        "70717273747576777879"
        "80818283848586878889"
        "90919293949596979899";
  char digits[20];
  u64 count = 0;

  while (value >= 100)
    {
      u64 quotient = value / 100;
      u32 pair = (u32)(value - quotient * 100);

      digits[count++] = digit_pairs[pair * 2 + 1];
      digits[count++] = digit_pairs[pair * 2];
      value = quotient;
    }

  if (value < 10)
    digits[count++] = (char)('0' + value);
  else
    {
      digits[count++] = digit_pairs[value * 2 + 1];
      digits[count++] = digit_pairs[value * 2];
    }

  while (count > 0)
    *output++ = digits[--count];

  return output;
}

local inline char *
append_s64 (char *output, s64 value)
{
  u64 magnitude;

  if (value < 0)
    {
      *output++ = '-';
      magnitude = (u64)(-(value + 1)) + 1;
    }
  else
    magnitude = (u64)value;

  return append_u64 (output, magnitude);
}

local inline char *
append_fixed_3 (char *output, s64 value, bool negative_zero)
{
  u64 magnitude = value < 0 ? (u64)(-(value + 1)) + 1 : (u64)value;
  u64 fractional = magnitude % 1000;

  if (value < 0 || negative_zero)
    *output++ = '-';

  output = append_u64 (output, magnitude / 1000);
  *output++ = '.';
  *output++ = (char)('0' + fractional / 100);
  *output++ = (char)('0' + (fractional / 10) % 10);
  *output++ = (char)('0' + fractional % 10);

  return output;
}

local inline u64
frame_header_length (u64 index, const ExportFrame *frame)
{
  return 6 + decimal_length_u64 (index) + 5
         + decimal_length_s64 (frame->pts) + 10
         + decimal_length_u64 (frame->vector_count) + 1;
}

local inline u64
format_frame_header (char *buffer, u64 index, const ExportFrame *frame)
{
  char *output = buffer;

  assert (buffer != NULL);
  assert (frame != NULL);

  memcpy (output, "FRAME ", 6);
  output += 6;
  output = append_u64 (output, index);
  memcpy (output, "\nPTS ", 5);
  output += 5;
  output = append_s64 (output, frame->pts);
  memcpy (output, "\nMV_COUNT ", 10);
  output += 10;
  output = append_u64 (output, frame->vector_count);
  *output++ = '\n';

  return (u64)(output - buffer);
}

local inline char *
append_vector (char *output, u64 index, const ExportMotionVector *mv)
{
  *output++ = 'M';
  *output++ = 'V';
  *output++ = ' ';
  output = append_u64 (output, index);
  memcpy (output, ": (", 3);
  output += 3;
  output = append_s64 (output, mv->src_x);
  memcpy (output, ", ", 2);
  output += 2;
  output = append_s64 (output, mv->src_y);
  memcpy (output, ") (", 3);
  output += 3;
  output = append_s64 (output, mv->dst_x);
  memcpy (output, ", ", 2);
  output += 2;
  output = append_s64 (output, mv->dst_y);
  memcpy (output, ") (", 3);
  output += 3;
  output = append_s64 (output, mv->width);
  memcpy (output, ", ", 2);
  output += 2;
  output = append_s64 (output, mv->height);
  memcpy (output, ") ", 2);
  output += 2;
  output = append_s64 (output, mv->source);
  *output++ = ' ';
  output = append_fixed_3 (output, mv->magnitude_1000,
                           (mv->negative_mask & 1) != 0);
  *output++ = ' ';
  output = append_fixed_3 (output, mv->angle_1000,
                           (mv->negative_mask & 2) != 0);
  memcpy (output, " (", 2);
  output += 2;
  output = append_fixed_3 (output, mv->normalized_x_1000,
                           (mv->negative_mask & 4) != 0);
  memcpy (output, ", ", 2);
  output += 2;
  output = append_fixed_3 (output, mv->normalized_y_1000,
                           (mv->negative_mask & 8) != 0);
  memcpy (output, ")\n", 2);
  output += 2;

  return output;
}

local inline u64
vector_line_length (u64 index, const ExportMotionVector *mv)
{
  return 3 + decimal_length_u64 (index) + 3
         + decimal_length_s32 (mv->src_x) + 2
         + decimal_length_s32 (mv->src_y) + 3
         + decimal_length_s32 (mv->dst_x) + 2
         + decimal_length_s32 (mv->dst_y) + 3
         + decimal_length_s32 (mv->width) + 2
         + decimal_length_s32 (mv->height) + 2
         + decimal_length_s32 (mv->source) + 1
         + decimal_length_fixed_3 (mv->magnitude_1000,
                                   (mv->negative_mask & 1) != 0)
         + 1 + decimal_length_fixed_3 (mv->angle_1000,
                                       (mv->negative_mask & 2) != 0)
         + 2 + decimal_length_fixed_3 (mv->normalized_x_1000,
                                       (mv->negative_mask & 4) != 0)
         + 2 + decimal_length_fixed_3 (mv->normalized_y_1000,
                                       (mv->negative_mask & 8) != 0)
         + 2;
}

local inline bool
calculate_output_size (ExportFrame *frames, u64 frame_count,
                       const ExportMotionVector *vectors, u64 *total_size)
{
  u64 i, j, total = 0;

  assert (total_size != NULL);
  assert (frame_count == 0 || frames != NULL);

  for (i = 0; i < frame_count; ++i)
    {
      u64 length;

      assert (frames[i].vector_count == 0 || vectors != NULL);

      frames[i].output_offset = total;
      length = frame_header_length (i, &frames[i]);

      if (length > UINT64_MAX - total)
        return false;

      total += length;

      for (j = 0; j < frames[i].vector_count; ++j)
        {
          length = vectors[frames[i].first_vector + j].line_length;

          if (length > UINT64_MAX - total)
            return false;

          total += length;
        }

      if (total == UINT64_MAX)
        return false;

      ++total; // Blank line after each frame.
      frames[i].output_size = total - frames[i].output_offset;
    }

  *total_size = total;

  return true;
}

local inline bool
worker_flush (ExportWorker *worker)
{
  assert (worker != NULL);
  assert (worker->output != NULL);

  if (worker->buffered == 0)
    return true;

  if (worker->output_position > worker->output->size
      || worker->buffered > worker->output->size - worker->output_position)
    return false;

  memcpy ((byte *)worker->output->data + worker->output_position,
          worker->buffer, worker->buffered);

  worker->output_position += worker->buffered;
  worker->buffered = 0;

  return true;
}

local inline bool
worker_append (ExportWorker *worker, const char *text, u64 length)
{
  assert (worker != NULL);
  assert (text != NULL || length == 0);

  while (length > 0)
    {
      u64 available = sizeof (worker->buffer) - worker->buffered,
          copy_size = length < available ? length : available;

      memcpy (worker->buffer + worker->buffered, text, copy_size);
      worker->buffered += copy_size;
      text += copy_size;
      length -= copy_size;

      if (worker->buffered == sizeof (worker->buffer)
          && !worker_flush (worker))
        return false;
    }

  return true;
}

local inline void
write_frames (void *arg)
{
  ExportWorker *worker = (ExportWorker *)arg;
  u64 i, j;
  char line[512];

  assert (worker != NULL);
  assert (worker->output != NULL);
  assert (worker->frames != NULL);

  if (worker->first_frame < worker->end_frame)
    worker->output_position
        = worker->frames[worker->first_frame].output_offset;

  for (i = worker->first_frame; i < worker->end_frame; ++i)
    {
      u64 length = format_frame_header (line, i, &worker->frames[i]);

      assert (worker->frames[i].vector_count == 0 || worker->vectors != NULL);
      assert (length < sizeof (line));

      if (!worker_append (worker, line, length))
        {
          worker->failed = true;
          return;
        }

      for (j = 0; j < worker->frames[i].vector_count; ++j)
        {
          char *end = append_vector (
              line, j, &worker->vectors[worker->frames[i].first_vector + j]);
          length = (u64)(end - line);

          assert (length < sizeof (line));

          if (!worker_append (worker, line, length))
            {
              worker->failed = true;
              return;
            }
        }

      if (!worker_append (worker, "\n", 1))
        {
          worker->failed = true;
          return;
        }
    }

  if (!worker_flush (worker))
    worker->failed = true;
}

bool
export_data_to (const char *video_file, const char *data_file)
{
  Arena arena = { 0 };
  VideoPlex *video = NULL;
  DynArr64 frame_array = { 0 };
  DynArr64 vector_array = { 0 };
  bool flushing = false, success = false;
  PlatformMappedFile output = { .native_file = -1 };
  ExportWorker *workers = NULL;
  PlatformThread *threads = NULL;
  bool *started = NULL;
  s32 worker_count, i;
  u64 output_size = 0;

  assert (video_file != NULL);
  assert (data_file != NULL);

  arena_init (&arena, EXPORT_BUFFER_SIZE);

  if (!dyn_arr64_try_init (&frame_array, 0, sizeof (ExportFrame))
      || !dyn_arr64_try_init (&vector_array, 0, sizeof (ExportMotionVector)))
    {
      fprintf (stderr, "Not enough memory to initialize export arrays.\n");
      goto cleanup;
    }

  worker_count = MAX (1, platform_get_logical_core_count ());

  video = init_video (&arena, (byte *)video_file, worker_count - 3);
  if (video == NULL)
    goto cleanup;

  for (;;)
    {
      s32 result = read_decoded_frame (video, &flushing);

      if (result == 0)
        break;

      if (result < 0)
        {
          fprintf (stderr, "Could not decode video while exporting.\n");
          goto cleanup;
        }

      ExportFrame frame
          = { .pts = video->frame->pts, .first_vector = vector_array.len };

      if (!append_vectors (&vector_array, video->frame))
        {
          fprintf (stderr, "Not enough memory to collect motion vectors.\n");
          goto cleanup;
        }

      frame.vector_count = vector_array.len - frame.first_vector;

      if (!dyn_arr64_try_append (&frame_array, &frame, 1))
        {
          fprintf (stderr, "Not enough memory to collect video frames.\n");
          goto cleanup;
        }

      av_frame_unref (video->frame);
    }

  shutdown_video (video);

  if (!calculate_output_size ((ExportFrame *)frame_array.data, frame_array.len,
                              (ExportMotionVector *)vector_array.data,
                              &output_size))
    {
      fprintf (stderr, "Export output is too large to represent.\n");
      goto cleanup;
    }

  if (output_size > 0
      && !platform_mapped_file_create (&output, data_file, output_size))
    {
      fprintf (stderr, "Could not create mapped output file: %s\n", data_file);
      goto cleanup;
    }

  if (frame_array.len == 0)
    {
      // Create the requested empty file using the platform mapping API
      if (!platform_mapped_file_create (&output, data_file, 0))
        {
          fprintf (stderr, "Could not create output file: %s\n", data_file);
          goto cleanup;
        }

      success = true;
      goto cleanup;
    }

  if (frame_array.len < (u64)worker_count)
    worker_count = (s32)frame_array.len;

  workers = calloc ((u64)worker_count, sizeof (*workers));
  threads = calloc ((u64)worker_count, sizeof (*threads));
  started = calloc ((u64)worker_count, sizeof (*started));

  if (workers == NULL || threads == NULL || started == NULL)
    {
      fprintf (stderr, "Not enough memory to start export workers.\n");
      goto cleanup;
    }

  {
    u64 frame_index = 0, consumed = 0;

    for (i = 0; i < worker_count; ++i)
      {
        u64 target = output_size / (u64)worker_count * (u64)(i + 1)
                     + output_size % (u64)worker_count * (u64)(i + 1)
                           / (u64)worker_count;
        u64 start = frame_index;

        while (frame_index < frame_array.len
               && (consumed < target
                   || frame_array.len - frame_index
                          > (u64)(worker_count - i - 1)))
          consumed += ((ExportFrame *)frame_array.data)[frame_index++]
                          .output_size;

        workers[i].output = &output;
        workers[i].frames = (ExportFrame *)frame_array.data;
        workers[i].vectors = (ExportMotionVector *)vector_array.data;
        workers[i].first_frame = start;
        workers[i].end_frame = frame_index;
      }
  }

  for (i = 0; i < worker_count; ++i)
    {
      if (platform_thread_start (&threads[i], write_frames, &workers[i]))
        started[i] = true;
      else
        write_frames (&workers[i]);
    }

  for (i = 0; i < worker_count; ++i)
    {
      if (started[i])
        platform_thread_join (&threads[i]);
    }

  success = true;

  for (i = 0; i < worker_count; ++i)
    if (workers[i].failed)
      success = false;

  if (!success)
    fprintf (stderr, "Failed while writing mapped export output.\n");

cleanup:

  platform_mapped_file_close (&output);

  return success;
}
