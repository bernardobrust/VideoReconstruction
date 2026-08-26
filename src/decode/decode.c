#include "decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

/*
typedef struct
{
  int video_stream;
  char *video_file;
  AVFormatContext *fmt;
  AVStream *stream;
  AVCodec *decoder;
  AVCodecContext *codec;
  AVFrame *frame;
  AVPacket *packet;
} VideoPlex;
*/

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
               "No video stream found. Is the file correclty encoded?\n");
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

int
decode_next_frame (VideoPlex *vp, unsigned *image)
{
  // Decode a single frame
  int got_frame = 0;
  while (!got_frame && av_read_frame (vp->fmt, vp->packet) >= 0)
    {
      // Ignore audio/subtitle/etc, we don't need that
      if (vp->packet->stream_index != vp->video_stream)
        {
          av_packet_unref (vp->packet);
          continue;
        }

      // Give compressed packet to decoder
      if (avcodec_send_packet (vp->codec, vp->packet) < 0)
        {
          fprintf (stderr, "Error sending packet to decoder\n");
          av_packet_unref (vp->packet);
          break;
        }

      // A packet may produce zero, one, or multiple frames
      while (!got_frame)
        {
          int ret = avcodec_receive_frame (vp->codec, vp->frame);
          if (ret == 0)
            {
              got_frame = 1;
              break;
            }

          if (ret == AVERROR (EAGAIN) || ret == AVERROR_EOF)
            break;

          fprintf (stderr,
                   "Error receiving decoded frame (ignored for now).\n");
          break;
        }

      av_packet_unref (vp->packet);
    }

  if (!got_frame)
    {
      fprintf (stderr, "Could not decode frame or it's the last one.\n");
      return 1;
    }

  int w = vp->codec->width, h = vp->codec->height, format = vp->frame->format;
  printf ("Decoded frame: %dx%d, pixel format %s\n", w, h,
          av_get_pix_fmt_name (format));

  struct SwsContext *sws = sws_getContext (w, h, format, w, h, AV_PIX_FMT_RGBA,
                                           SWS_BILINEAR, NULL, NULL, NULL);
  if (!sws)
    {
      fprintf (stderr, "Could not create scaler.\n");
      return 2;
    }

  uint8_t *dst_data[4] = { (uint8_t *)image, NULL, NULL, NULL };
  int dst_linesize[4] = { w * 4, 0, 0, 0 };

  sws_scale (sws, (const uint8_t *const *)vp->frame->data, vp->frame->linesize,
             0, h, dst_data, dst_linesize);

  return 0;
}
