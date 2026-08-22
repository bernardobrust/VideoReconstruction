#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "input.h"
#include "platform.h"
#include "renderer.h"

int
main (int argc, char **argv)
{
  // Argument parsing for the video to inspect
  if (argc <= 1)
    {
      fprintf (stderr,
               "Please provide a path to the video file to inspect.\n");
      return EXIT_FAILURE;
    }

  char *video_file = argv[1];

  int file_exists = platform_file_exists (video_file);
  if (file_exists == 0)
    printf ("Video to inspect: %s\n", video_file);
  else if (file_exists == 1)
    {
      fprintf (stderr, "File does not exist. Is the path correct?\n");
      return EXIT_FAILURE;
    }
  else
    {
      fprintf (stderr,
               "Could not access path. Do you have permission to open it?\n");
      return EXIT_FAILURE;
    }

  // I'll drop the entire code here for decoding and factor it out latter just
  // for testing
  AVFormatContext *fmt = NULL;

  if (avformat_open_input (&fmt, video_file, NULL, NULL) < 0)
    {
      fprintf (stderr, "Could not open video file.\n");
      return EXIT_FAILURE;
    }

  if (avformat_find_stream_info (fmt, NULL) < 0)
    {
      fprintf (stderr,
               "Could not find stream info. Is the file correctly encoded?\n");
      return EXIT_FAILURE;
    }

  int video_stream = -1;

  for (unsigned i = 0; i < fmt->nb_streams; i++)
    {
      if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
          video_stream = i;
          break;
        }
    }

  if (video_stream < 0)
    {
      fprintf (stderr,
               "No video stream found. Is the file correclty encoded?\n");
      return EXIT_FAILURE;
    }

  AVStream *stream = fmt->streams[video_stream];

  const AVCodec *decoder = avcodec_find_decoder (stream->codecpar->codec_id);
  if (!decoder)
    {
      fprintf (stderr, "No decoder found, did you install the FFmpeg "
                       "libraries corectly?\n");
      return EXIT_FAILURE;
    }

  printf ("Decoder: %s\n", decoder->name);

  // This project assumes the video is AV1 encoded
  if (stream->codecpar->codec_id != AV_CODEC_ID_AV1)
    {
      fprintf (stderr, "Video is not AV1 encoded, but you can convert it to "
                       "AV1 using FFmpeg.\n");
      return EXIT_FAILURE;
    }

  AVCodecContext *codec = avcodec_alloc_context3 (decoder);
  if (!codec)
    {
      fprintf (stderr, "Could not allocate codec context.\n");
      return EXIT_FAILURE;
    }

  if (avcodec_parameters_to_context (codec, stream->codecpar) < 0)
    {
      fprintf (stderr, "Could not copy codec parameters.\n");
      return EXIT_FAILURE;
    }

  if (avcodec_open2 (codec, decoder, NULL) < 0)
    {
      fprintf (stderr, "Could not open decoder\n");
      return EXIT_FAILURE;
    }

  printf ("Resolution: %dx%d\n", codec->width, codec->height);

  AVFrame *frame = av_frame_alloc ();
  if (!frame)
    {
      fprintf (stderr, "Could not allocate frame\n");
      return EXIT_FAILURE;
    }

  AVPacket *packet = av_packet_alloc ();
  if (!packet)
    {
      fprintf (stderr, "Could not allocate packet\n");
      return EXIT_FAILURE;
    }

  int got_frame = 0;
  while (!got_frame && av_read_frame (fmt, packet) >= 0)
    {
      // Ignore audio/subtitle/etc, we don't need that
      if (packet->stream_index != video_stream)
        {
          av_packet_unref (packet);
          continue;
        }

      // Give compressed packet to decoder
      if (avcodec_send_packet (codec, packet) < 0)
        {
          fprintf (stderr, "Error sending packet to decoder\n");
          av_packet_unref (packet);
          break;
        }

      // A packet may produce zero, one, or multiple frames
      while (!got_frame)
        {
          int ret = avcodec_receive_frame (codec, frame);

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

      av_packet_unref (packet);
    }

  if (!got_frame)
    {
      fprintf (stderr, "Could not decode first frame.\n");
      return EXIT_FAILURE;
    }

  printf ("Decoded frame: %dx%d, pixel format %s\n", frame->width,
          frame->height, av_get_pix_fmt_name (frame->format));

  int width = frame->width;
  int height = frame->height;
  struct SwsContext *sws
      = sws_getContext (width, height, frame->format, width, height,
                        AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

  if (!sws)
    {
      fprintf (stderr, "Could not create scaler.\n");
      return EXIT_FAILURE;
    }

  uint32_t *image = malloc ((size_t)width * height * sizeof (uint32_t));

  if (!image)
    {
      fprintf (stderr, "Could not allocate image.\n");
      return EXIT_FAILURE;
    }

  uint8_t *dst_data[4] = { (uint8_t *)image, NULL, NULL, NULL };
  int dst_linesize[4] = { width * 4, 0, 0, 0 };

  sws_scale (sws, (const uint8_t *const *)frame->data, frame->linesize, 0,
             height, dst_data, dst_linesize);

  // Done decoding 1 frame

  // Initialize the renderer and platform
  RendererPlex rp = init_renderer (width, height);

  PlatformState platform_state = { 0 };
  platform_init (&platform_state, "Inspector", 0, 0, rp.w, rp.h,
                 (char *)rp.image_buffer);

  // Main app loop
  while (platform_update (&platform_state))
    {
      if (input_is_key_pressed (ESC))
        platform_stop (&platform_state);

      memcpy (rp.image_buffer, image,
              (size_t)rp.w * rp.h * sizeof (*rp.image_buffer));

      renderer_present (&platform_state, rp);
    }

  platform_stop (&platform_state);

  return EXIT_SUCCESS;
}
