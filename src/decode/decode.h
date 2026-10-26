#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "macros.h"
#include "platform.h"
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
  struct SwsContext *sws;
} VideoPlex;

VideoPlex *init_video (Arena *arena, byte *video_file, s32 decoder_threads);
void shutdown_video (VideoPlex *vp);
s32 decode_next_frame (VideoPlex *vp, RendererPlex *rp);
void decode_set_display_resolution (ScreenResolution resolution);
