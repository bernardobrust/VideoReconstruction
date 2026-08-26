#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct
{
  int video_stream;
  char *video_file;
  AVFormatContext *fmt;
  AVStream *stream;
  const AVCodec *decoder;
  AVCodecContext *codec;
  AVFrame *frame;
  AVPacket *packet;
} VideoPlex;

VideoPlex *init_video (char *video_file);
unsigned *decode_next_frame ();