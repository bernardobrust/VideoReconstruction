#include "decode.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libavutil/motion_vector.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>

#include "basic.h"

VideoPlex *
init_video (Arena *arena, byte *video_file, s32 decoder_threads)
{
  assert (arena != NULL && decoder_threads > 0);

#ifndef DNDEBUG
  printf ("Using %d decoder threads\n", decoder_threads);
#endif

  VideoPlex *video_plex
      = arena_alloc (arena, sizeof (*video_plex), _Alignof (VideoPlex));
  assert (video_plex != NULL);

  memset (video_plex, 0, sizeof (*video_plex));

  if (avformat_open_input (&video_plex->fmt, video_file, NULL, NULL) < 0)
    {
      fprintf (stderr, "Could not open video file.\n");
      goto fail;
    }

  if (avformat_find_stream_info (video_plex->fmt, NULL) < 0)
    {
      fprintf (stderr,
               "Could not find stream info. Is the file correctly encoded?\n");
      goto fail;
    }

  video_plex->video_stream = -1;
  for (u32 i = 0; i < video_plex->fmt->nb_streams; ++i)
    {
      if (video_plex->fmt->streams[i]->codecpar->codec_type
          == AVMEDIA_TYPE_VIDEO)
        {
          video_plex->video_stream = i;
          break;
        }
    }

  if (video_plex->video_stream < 0)
    {
      fprintf (stderr,
               "No video stream found. Is the file correctly encoded?\n");
      goto fail;
    }

  video_plex->stream = video_plex->fmt->streams[video_plex->video_stream];
  video_plex->decoder = avcodec_find_decoder_by_name ("libdav1d");
  if (!video_plex->decoder)
    {
      fprintf (stderr, "No decoder found, did you install the FFmpeg "
                       "libraries corectly?\n");
      goto fail;
    }

  video_plex->codec = avcodec_alloc_context3 (video_plex->decoder);
  assert (video_plex->codec != NULL);

  if (avcodec_parameters_to_context (video_plex->codec,
                                     video_plex->stream->codecpar)
      < 0)
    {
      fprintf (stderr, "Could not copy codec parameters\n");
      goto fail;
    }

  // The libdav1d wrapper maps thread_count to dav1d's thread count. Keep it at
  // least one because zero selects automatic core detection
  video_plex->codec->thread_count = decoder_threads;

  // Important: we need to set the export_mvs flag to get motion vectors
  av_opt_set_int (video_plex->codec, "flags2", AV_CODEC_FLAG2_EXPORT_MVS, 0);
  if (avcodec_open2 (video_plex->codec, video_plex->decoder, NULL) < 0)
    {
      fprintf (stderr, "Could not open decoder\n");
      goto fail;
    }

#ifndef NDEBUG
  printf ("Resolution: %dx%d\n", video_plex->codec->width,
          video_plex->codec->height);
#endif

  video_plex->frame = av_frame_alloc ();
  assert (video_plex->frame != NULL);

  video_plex->packet = av_packet_alloc ();
  assert (video_plex->packet != NULL);

  return video_plex;

fail:
  return NULL;
}

void
shutdown_video (VideoPlex *video_plex)
{
  assert (video_plex != NULL);

  if (video_plex->sws != NULL)
    sws_freeContext (video_plex->sws);

  av_packet_free (&video_plex->packet);
  av_frame_free (&video_plex->frame);
  avcodec_free_context (&video_plex->codec);
  avformat_close_input (&video_plex->fmt);
}

// We may want to factor the scaling out of here
// Convention:
// negative = error
// 0 = ok
// 1 = end
s32
decode_next_frame (VideoPlex *video_plex, RendererPlex *renderer)
{
  bool got_frame = false;

  assert (video_plex != NULL && renderer != NULL);

  while (!got_frame)
    {
      s32 ret = av_read_frame (video_plex->fmt, video_plex->packet);

      if (ret < 0)
        {
          // Input is exhausted, flush the decoder
          ret = avcodec_send_packet (video_plex->codec, NULL);

          if (ret < 0 && ret != AVERROR_EOF)
            {
              fprintf (stderr, "Error flushing decoder.\n");
              return -1;
            }

          // Receive any frames buffered inside the decoder
          while (!got_frame)
            {
              ret = avcodec_receive_frame (video_plex->codec,
                                           video_plex->frame);

              if (ret == 0)
                {
                  got_frame = true;
                  break;
                }

              // No more frames. This really is the end
              if (ret == AVERROR_EOF)
                return 1;

              // Shouldn't normally happen after flushing, but isn't an error
              if (ret == AVERROR (EAGAIN))
                return 1;

              fprintf (stderr, "Error receiving flushed frame.\n");
              return -1;
            }

          break;
        }

      // Ignore non-video packets
      if (video_plex->packet->stream_index != video_plex->video_stream)
        {
          av_packet_unref (video_plex->packet);
          continue;
        }

      ret = avcodec_send_packet (video_plex->codec, video_plex->packet);
      av_packet_unref (video_plex->packet);

      if (ret < 0)
        {
          fprintf (stderr, "Error sending packet to decoder.\n");
          return -1;
        }

      // A packet can produce zero, one, or multiple frames
      while (!got_frame)
        {
          ret = avcodec_receive_frame (video_plex->codec, video_plex->frame);

          if (ret == 0)
            {
              got_frame = true;
              break;
            }

          if (ret == AVERROR (EAGAIN))
            break;

          if (ret == AVERROR_EOF)
            return 1;

          fprintf (stderr, "Error receiving decoded frame.\n");
          return -1;
        }
    }

  // Convert the decoded frame. We may want to factor this out
  s32 width = video_plex->codec->width, height = video_plex->codec->height;
  s32 pixel_format = video_plex->frame->format;

  // Using RGBA was swapping the colors, so we use BGRA instead.
  video_plex->sws = sws_getCachedContext (
      video_plex->sws, width, height, pixel_format, width, height,
      AV_PIX_FMT_BGRA, SWS_BILINEAR, NULL, NULL, NULL);
  assert (video_plex->sws != NULL);

  u8 *dst_data[4] = { (u8 *)renderer->image_buffer, NULL, NULL, NULL };
  s32 dst_linesize[4] = { width * 4, 0, 0, 0 };

  sws_scale (video_plex->sws, (const u8 *const *)video_plex->frame->data,
             video_plex->frame->linesize, 0, height, dst_data, dst_linesize);

  return 0;
}
