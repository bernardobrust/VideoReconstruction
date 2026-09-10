#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "macros.h"

typedef struct
{
  s32 video_stream;
  char *video_file;
  AVFormatContext *fmt;
  AVStream *stream;
  const AVCodec *decoder;
  AVCodecContext *codec;
  AVFrame *frame;
  AVPacket *packet;
} VideoPlex;

VideoPlex *init_video (char *video_file);
s32 decode_next_frame (VideoPlex *vp, u32 *image);