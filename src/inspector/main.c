#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include "decode.h"
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
    printf ("Video to inspect: %s.\n", video_file);
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

  VideoPlex *vp = init_video (video_file);
  if (vp == NULL)
    return EXIT_FAILURE;

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
      fprintf (stderr, "Could not decode first frame.\n");
      return EXIT_FAILURE;
    }

  int width = vp->frame->width, height = vp->frame->height,
      format = vp->frame->format;
  printf ("Decoded frame: %dx%d, pixel format %s\n", width, height,
          av_get_pix_fmt_name (format));

  struct SwsContext *sws
      = sws_getContext (width, height, format, width, height, AV_PIX_FMT_RGBA,
                        SWS_BILINEAR, NULL, NULL, NULL);

  if (!sws)
    {
      fprintf (stderr, "Could not create scaler.\n");
      return EXIT_FAILURE;
    }

  unsigned *image = malloc ((size_t)width * height * sizeof (unsigned));
  if (!image)
    {
      fprintf (stderr, "Could not allocate image.\n");
      return EXIT_FAILURE;
    }

  uint8_t *dst_data[4] = { (uint8_t *)image, NULL, NULL, NULL };
  int dst_linesize[4] = { width * 4, 0, 0, 0 };

  sws_scale (sws, (const uint8_t *const *)vp->frame->data, vp->frame->linesize,
             0, height, dst_data, dst_linesize);

  // Done decoding 1 frame

  // Initialize the renderer and platform
  RendererPlex *rp = init_renderer (width, height);

  PlatformState platform_state = { 0 };
  platform_init (&platform_state, "Inspector", 0, 0, rp->w, rp->h,
                 (char *)rp->image_buffer);

  // Main app loop
  while (platform_update (&platform_state))
    {
      if (input_is_key_pressed (ESC))
        platform_stop (&platform_state);

      memcpy (rp->image_buffer, image,
              (size_t)rp->w * rp->h * sizeof (*rp->image_buffer));

      renderer_present (&platform_state, rp);
    }

  platform_stop (&platform_state);

  return EXIT_SUCCESS;
}
