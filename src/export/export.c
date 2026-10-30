#include "export.h"

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/frame.h>
#include <libavutil/motion_vector.h>

#include "decode.h"
#include "dyn_arr.h"
#include "platform.h"
#include "thread.h"
#include "basic.h"

// Read this from a config file later
#define EXPORT_BUFFER_SIZE (128 * 1024)

typedef struct
{
  s32 src_x, src_y, dst_x, dst_y, width, height, source;
} ExportMotionVector;

typedef struct
{
  s64 pts;
  u64 first_vector, vector_count, output_offset;
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

local bool
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
      destination[i] = (ExportMotionVector){ .src_x = source[i].src_x,
                                             .src_y = source[i].src_y,
                                             .dst_x = source[i].dst_x,
                                             .dst_y = source[i].dst_y,
                                             .width = source[i].w,
                                             .height = source[i].h,
                                             .source = source[i].source };
    }

  vectors->len += vector_count;

  return true;
}

local s32
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

local void
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

local int
format_vector (char *buffer, u64 capacity, u64 index,
               const ExportMotionVector *mv)
{
  f32 magnitude, angle, nx, ny;

  assert (buffer != NULL);
  assert (mv != NULL);

  vector_values (mv, &magnitude, &angle, &nx, &ny);

  return snprintf (buffer, capacity,
                   "MV %" PRIu64 ": (%d, %d) (%d, %d) "
                   "(%d, %d) "
                   "%d %.3f %.3f "
                   "(%.3f, %.3f)\n",
                   index, mv->src_x, mv->src_y, mv->dst_x, mv->dst_y,
                   mv->width, mv->height, mv->source, magnitude, angle, nx,
                   ny);
}

local bool
calculate_output_size (ExportFrame *frames, u64 frame_count,
                       const ExportMotionVector *vectors, u64 *total_size)
{
  u64 i, j, total = 0;
  char line[512];

  assert (total_size != NULL);
  assert (frame_count == 0 || frames != NULL);

  for (i = 0; i < frame_count; ++i)
    {
      int length;

      assert (frames[i].vector_count == 0 || vectors != NULL);

      frames[i].output_offset = total;
      length = snprintf (line, sizeof (line),
                         "FRAME %" PRIu64 "\nPTS %" PRId64
                         "\nMV_COUNT %" PRIu64 "\n",
                         i, frames[i].pts, frames[i].vector_count);

      if (length < 0 || (u64)length >= sizeof (line)
          || (u64)length > UINT64_MAX - total)
        return false;

      total += (u64)length;

      for (j = 0; j < frames[i].vector_count; ++j)
        {
          length = format_vector (line, sizeof (line), j,
                                  &vectors[frames[i].first_vector + j]);
          if (length < 0 || (u64)length >= sizeof (line)
              || (u64)length > UINT64_MAX - total)
            return false;

          total += (u64)length;
        }

      if (total == UINT64_MAX)
        return false;

      ++total; // Blank line after each frame.
    }

  *total_size = total;

  return true;
}

local bool
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

local bool
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

local void
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
      int length = snprintf (
          line, sizeof (line),
          "FRAME %" PRIu64 "\nPTS %" PRId64 "\nMV_COUNT %" PRIu64 "\n", i,
          worker->frames[i].pts, worker->frames[i].vector_count);

      assert (worker->frames[i].vector_count == 0 || worker->vectors != NULL);
      if (length < 0 || (u64)length >= sizeof (line)
          || !worker_append (worker, line, (u64)length))
        {
          worker->failed = true;
          return;
        }

      for (j = 0; j < worker->frames[i].vector_count; ++j)
        {
          length = format_vector (
              line, sizeof (line), j,
              &worker->vectors[worker->frames[i].first_vector + j]);

          if (length < 0 || (u64)length >= sizeof (line)
              || !worker_append (worker, line, (u64)length))
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
  VideoPlex *video;
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

  arena_init (&arena, 64 * 1024);

  if (!dyn_arr64_try_init (&frame_array, 0, sizeof (ExportFrame))
      || !dyn_arr64_try_init (&vector_array, 0, sizeof (ExportMotionVector)))
    {
      fprintf (stderr, "Not enough memory to initialize export arrays.\n");
      goto cleanup;
    }

  worker_count = MAX(1, platform_get_logical_core_count () - 3);

  video = init_video (&arena, (byte *)video_file, worker_count);
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

  for (i = 0; i < worker_count; ++i)
    {
      workers[i].output = &output;
      workers[i].frames = (ExportFrame *)frame_array.data;
      workers[i].vectors = (ExportMotionVector *)vector_array.data;
      workers[i].first_frame = frame_array.len * (u64)i / (u64)worker_count;
      workers[i].end_frame
          = frame_array.len * (u64)(i + 1) / (u64)worker_count;

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
