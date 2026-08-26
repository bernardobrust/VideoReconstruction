#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

typedef struct
{
  int video_stream, w, h, format;
  char *video_file;
  AVFormatContext *fmt;
  AVStream *stream;
  AVCodec *decoder;
  AVCodecContext *codec;
  AVFrame *frame;
  AVPacket *packet;
} VideoPlex;

VideoPlex *init_video (char *video_file);
unsigned *decode_next_frame ();