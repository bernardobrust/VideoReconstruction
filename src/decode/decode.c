#include "decode.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

VideoPlex *
init_video (char *video_file)
{
  VideoPlex *vp = (VideoPlex *)malloc (sizeof (VideoPlex));
  if (!vp)
    {
      fprintf (stderr, "Could not allocate VideoPlex.\n");
      return NULL;
    }

  vp->fmt = NULL;
  if (avformat_open_input (&vp->fmt, video_file, NULL, NULL) < 0)
    {
      fprintf (stderr, "Could not open video file.\n");
      return NULL;
    }

  if (avformat_find_stream_info (vp->fmt, NULL) < 0)
    {
      fprintf (stderr,
               "Could not find stream info. Is the file correctly encoded?\n");
      return NULL;
    }

  vp->video_stream = -1;
  for (unsigned i = 0; i < vp->fmt->nb_streams; ++i)
    {
      if (vp->fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
          vp->video_stream = i;
          break;
        }
    }

  if (vp->video_stream < 0)
    {
      fprintf (stderr,
               "No video stream found. Is the file correctly encoded?\n");
      return NULL;
    }

  vp->stream = vp->fmt->streams[vp->video_stream];
  vp->decoder = avcodec_find_decoder (vp->stream->codecpar->codec_id);
  if (!vp->decoder)
    {
      fprintf (stderr, "No decoder found, did you install the FFmpeg "
                       "libraries corectly?\n");
      return NULL;
    }

  printf ("Decoder: %s\n", vp->decoder->name);

  // This project assumes the video is AV1 encoded
  if (vp->stream->codecpar->codec_id != AV_CODEC_ID_AV1)
    {
      fprintf (stderr, "Video is not AV1 encoded, but you can convert it to "
                       "AV1 using FFmpeg.\n");
      return NULL;
    }

  vp->codec = avcodec_alloc_context3 (vp->decoder);
  if (!vp->codec)
    {
      fprintf (stderr, "Could not allocate codec context.\n");
      return NULL;
    }

  if (avcodec_parameters_to_context (vp->codec, vp->stream->codecpar) < 0)
    {
      fprintf (stderr, "Could not copy codec parameters.\n");
      return NULL;
    }

  if (avcodec_open2 (vp->codec, vp->decoder, NULL) < 0)
    {
      fprintf (stderr, "Could not open decoder\n");
      return NULL;
    }

  printf ("Resolution: %dx%d\n", vp->codec->width, vp->codec->height);

  vp->frame = av_frame_alloc ();
  if (!vp->frame)
    {
      fprintf (stderr, "Could not allocate frame\n");
      return NULL;
    }

  vp->packet = av_packet_alloc ();
  if (!vp->packet)
    {
      fprintf (stderr, "Could not allocate packet\n");
      return NULL;
    }

  return vp;
}

// We may want to factor the scaling out of here
// Convention:
// negative = error
// 0 = ok
// 1 = end
int
decode_next_frame (VideoPlex *vp, unsigned *image)
{
  bool got_frame = false;
  while (!got_frame)
    {
      int ret = av_read_frame (vp->fmt, vp->packet);

      if (ret < 0)
        {
          // Input is exhausted, flush the decoder
          ret = avcodec_send_packet (vp->codec, NULL);

          if (ret < 0 && ret != AVERROR_EOF)
            {
              fprintf (stderr, "Error flushing decoder.\n");
              return -1;
            }

          // Receive any frames buffered inside the decoder
          while (!got_frame)
            {
              ret = avcodec_receive_frame (vp->codec, vp->frame);

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
      if (vp->packet->stream_index != vp->video_stream)
        {
          av_packet_unref (vp->packet);
          continue;
        }

      ret = avcodec_send_packet (vp->codec, vp->packet);
      av_packet_unref (vp->packet);

      if (ret < 0)
        {
          fprintf (stderr, "Error sending packet to decoder.\n");
          return -1;
        }

      // A packet can produce zero, one, or multiple frames
      while (!got_frame)
        {
          ret = avcodec_receive_frame (vp->codec, vp->frame);

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
  int w = vp->codec->width, h = vp->codec->height, format = vp->frame->format;

  struct SwsContext *sws = sws_getContext (w, h, format, w, h, AV_PIX_FMT_RGBA,
                                           SWS_BILINEAR, NULL, NULL, NULL);

  if (!sws)
    {
      fprintf (stderr, "Could not create scaler.\n");
      return -2;
    }

  uint8_t *dst_data[4] = { (uint8_t *)image, NULL, NULL, NULL };
  int dst_linesize[4] = { w * 4, 0, 0, 0 };

  sws_scale (sws, (const uint8_t *const *)vp->frame->data, vp->frame->linesize,
             0, h, dst_data, dst_linesize);

  sws_freeContext (sws);

  return 0;
}
