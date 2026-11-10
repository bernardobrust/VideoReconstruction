#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "dyn_arr.h"
#include "macros.h"
#include "platform.h"
#include "renderer.h"
#include "renderer_pool.h"

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
  DynArr *frame_timestamps;
} VideoPlex;

VideoPlex *init_video (Arena *arena, byte *video_file, s32 decoder_threads);
void shutdown_video (VideoPlex *video_plex);

s32 decode_next_frame (VideoPlex *video_plex, RendererPlex *renderer);
s32 decode_seek_to_frame (VideoPlex *video_plex, s64 frame_number);
s64 video_frame_number_from_timestamp (VideoPlex *video_plex, s64 timestamp);
void video_record_frame_timestamp (VideoPlex *video_plex, s64 frame_number);
