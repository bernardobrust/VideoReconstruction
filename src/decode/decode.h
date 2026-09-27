#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "macros.h"
#include "renderer.h"

typedef struct
{
  s32 video_stream;
  byte *video_file;
  AVFormatContext *fmt;
  AVStream *stream;
  const AVCodec *decoder;
  AVCodecContext *codec;
  AVFrame *frame;
  AVPacket *packet;
} VideoPlex;

VideoPlex *init_video (byte *video_file);
s32 decode_next_frame (VideoPlex *vp, RendererPlex *rp);