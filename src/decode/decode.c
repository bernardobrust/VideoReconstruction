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

unsigned *
decode_next_frame ()
{
  return NULL;
}
