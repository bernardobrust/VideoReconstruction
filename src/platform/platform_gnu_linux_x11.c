// I have to say that Platform Layers are the most Copy-Pastey Ever invented

// Use these definitions if and only if we're on GNU + Linux and Wayland

// Note: this is some of the worst code I've ever written. It has gotos,
// bithacks and I honestly do not understand a massive part of the code here
// (copy pasted from many places). But it works so I'll take it.

#define _POSIX_C_SOURCE 200112L
#define _DEFAULT_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "platform.h"
#include "utility.h"

// Don't memorize numbers
// I'm adding the +8 here as they are the same as wayland's but with an offset
// of 8
enum X11KeyValues
{
  CTRL = 29 + 8,
  SHIFT = 42 + 8,
  ESC = 1 + 8,
  ONE = 2 + 8,
  TWO = 3 + 8,
  THREE = 4 + 8,
  P = 25 + 8,
};

enum
{
  X11_PROTOCOL_MAJOR = 11,
  X11_PROTOCOL_MINOR = 0,
  X11_AUTH_FAMILY_LOCAL = 256,
  X11_AUTH_FAMILY_WILD = 65535,
  X11_EVENT_EXPOSURE = 12,
  X11_EVENT_DESTROY_NOTIFY = 17,
  X11_EVENT_CLIENT_MESSAGE = 33,
  X11_REPLY = 1,
  X11_ERROR = 0,
  X11_COPY_FROM_PARENT = 0,
  X11_INPUT_OUTPUT = 1,
  X11_Z_PIXMAP = 2,
  X11_ATOM_STRING = 31,
  X11_ATOM_WM_NAME = 39,
  X11_CW_BACK_PIXEL = 1 << 1,
  X11_CW_EVENT_MASK = 1 << 11,
  X11_EVENT_MASK_KEY_PRESS = 1 << 0,
  X11_EVENT_MASK_KEY_RELEASE = 1 << 1,
  X11_EVENT_MASK_EXPOSURE = 1 << 15,
  X11_EVENT_MASK_STRUCTURE_NOTIFY = 1 << 17,
  X11_GC_FOREGROUND = 1 << 2,
  X11_QUERY_EXTENSION = 98,
  X11_SHM_ATTACH = 1,
  X11_SHM_DETACH = 2,
  X11_SHM_PUT_IMAGE = 3,
};

typedef struct
{
  int fd;
  unsigned root;
  unsigned root_visual;
  unsigned window;
  unsigned gc;
  unsigned resource_base;
  unsigned resource_mask;
  unsigned resource_counter;
  unsigned wm_protocols;
  unsigned wm_delete_window;
  unsigned short max_request_words;
  unsigned width;
  unsigned height;
  unsigned depth;
  unsigned bits_per_pixel;
  unsigned scanline_pad;
  unsigned char *image_buffer;
  unsigned char read_buf[8192];
  size_t read_len;
  unsigned char shm_opcode;
  unsigned shmseg;
  int shmid;
  unsigned char *shm_data;
  size_t shm_size;
  bool has_shm;
} InternalState;

static bool
send_all (int fd, const void *data, size_t size)
{
  const unsigned char *p = data;
  while (size > 0)
    {
      ssize_t written = send (fd, p, size, 0);

      if (written < 0 && errno == EINTR)
        continue;

      if (written <= 0)
        return false;

      p += written;
      size -= (size_t)written;
    }

  return true;
}

static bool
read_all (int fd, void *data, size_t size)
{
  unsigned char *p = data;
  while (size > 0)
    {
      ssize_t received = recv (fd, p, size, 0);

      if (received < 0 && errno == EINTR)
        continue;

      if (received <= 0)
        return false;

      p += received;
      size -= (size_t)received;
    }

  return true;
}

static bool
read_xauthority (unsigned char token[16])
{
  const char *path = getenv ("XAUTHORITY");
  char default_path[PATH_MAX];

  if (path == NULL || path[0] == '\0')
    {
      const char *home = getenv ("HOME");

      if (home == NULL
          || snprintf (default_path, sizeof (default_path), "%s/.Xauthority",
                       home)
                 >= (int)sizeof (default_path))
        return false;

      path = default_path;
    }

  FILE *file = fopen (path, "rb");

  if (file == NULL)
    return false;

  bool found = false;
  unsigned char length_buf[2];
  while (fread (length_buf, 1, sizeof (length_buf), file)
         == sizeof (length_buf))
    {
      unsigned short family = read_u16_be (length_buf);
      // An Xauthority entry is family, address, display number, auth name,
      // and auth data; all fields after family are length-prefixed.
      unsigned char *fields[4] = { NULL, NULL, NULL, NULL };
      unsigned short lengths[4] = { 0, 0, 0, 0 };
      bool valid = true;
      for (unsigned i = 0; i < 4; ++i)
        {
          if (fread (length_buf, 1, sizeof (length_buf), file)
              != sizeof (length_buf))
            {
              valid = false;
              break;
            }

          lengths[i] = read_u16_be (length_buf);
          fields[i] = malloc (lengths[i] == 0 ? 1 : lengths[i]);

          if (fields[i] == NULL
              || fread (fields[i], 1, lengths[i], file) != lengths[i])
            {
              valid = false;
              break;
            }
        }

      if (valid
          && (family == X11_AUTH_FAMILY_LOCAL
              || family == X11_AUTH_FAMILY_WILD)
          && lengths[2] == strlen ("MIT-MAGIC-COOKIE-1")
          && memcmp (fields[2], "MIT-MAGIC-COOKIE-1", lengths[2]) == 0
          && lengths[3] == 16)
        {
          memcpy (token, fields[3], 16);
          found = true;
        }

      for (unsigned i = 0; i < 4; ++i)
        free (fields[i]);

      if (!valid || found)
        break;
    }

  fclose (file);

  return found;
}

static int
display_connect (void)
{
  const char *display = getenv ("DISPLAY");

  if (display == NULL || display[0] != ':')
    {
      fprintf (stderr, "Only local X11 displays (DISPLAY=:N) are supported\n");
      return -1;
    }

  char *end = NULL;
  long number = strtol (display + 1, &end, 10);
  if (end == display + 1 || number < 0 || number > INT_MAX
      || (*end != '\0' && *end != '.'))
    {
      fprintf (stderr, "Invalid DISPLAY value: %s\n", display);
      return -1;
    }

  struct sockaddr_un address = { 0 };
  address.sun_family = AF_UNIX;

  if (snprintf (address.sun_path, sizeof (address.sun_path),
                "/tmp/.X11-unix/X%ld", number)
      >= (int)sizeof (address.sun_path))
    return -1;

  int fd = socket (AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0
      || connect (fd, (struct sockaddr *)&address, sizeof (address)) < 0)
    {
      if (fd >= 0)
        close (fd);

      perror ("Could not connect to X11 display");

      return -1;
    }

  return fd;
}

static unsigned
next_resource_id (InternalState *state)
{
  unsigned id = state->resource_base;
  unsigned value = ++state->resource_counter;

  for (unsigned bit = 0; bit < 32; ++bit)
    if (state->resource_mask & (1u << bit))
      {
        id |= (value & 1u) << bit;
        value >>= 1;
      }

  return id;
}

static bool
send_request (InternalState *state, unsigned char opcode, unsigned char detail,
              const unsigned char *body, size_t body_size)
{
  size_t size = 4 + body_size;

  if (size % 4 != 0 || size / 4 > state->max_request_words)
    return false;

  unsigned char *request = calloc (1, size);

  if (request == NULL)
    return false;

  request[0] = opcode;
  request[1] = detail;

  write_u16_le (request + 2, (unsigned short)(size / 4));

  memcpy (request + 4, body, body_size);
  bool result = send_all (state->fd, request, size);

  free (request);

  return result;
}

static bool
read_reply (InternalState *state, unsigned char reply[32])
{
  if (!read_all (state->fd, reply, 32))
    return false;

  unsigned long_words = read_u32_le (reply + 4);
  if (reply[0] != X11_REPLY || long_words != 0)
    {
      if (long_words > 0)
        {
          size_t extra = (size_t)long_words * 4;
          unsigned char discard[256];

          while (extra > 0)
            {
              size_t chunk
                  = extra < sizeof (discard) ? extra : sizeof (discard);

              if (!read_all (state->fd, discard, chunk))
                break;

              extra -= chunk;
            }
        }
      fprintf (stderr, "X11 server rejected a request (error %u)\n", reply[1]);

      return false;
    }

  return true;
}

static bool
intern_atom (InternalState *state, const char *name, unsigned *atom)
{
  size_t name_length = strlen (name);
  size_t body_size = 4 + round_up ((unsigned)name_length, 4);
  unsigned char *body = calloc (1, body_size);

  if (body == NULL)
    return false;

  write_u16_le (body, (unsigned short)name_length);

  memcpy (body + 4, name, name_length);
  bool result = send_request (state, 16, 0, body, body_size);
  free (body);

  unsigned char reply[32];

  if (!result || !read_reply (state, reply))
    return false;

  *atom = read_u32_le (reply + 8);

  return true;
}

static bool
query_extension (InternalState *state, const char *name,
                 unsigned char *major_opcode)
{
  size_t name_length = strlen (name);
  size_t body_size = 4 + round_up ((unsigned)name_length, 4);
  unsigned char *body = calloc (1, body_size);

  if (body == NULL)
    return false;

  write_u16_le (body, (unsigned short)name_length);

  memcpy (body + 4, name, name_length);
  bool result = send_request (state, X11_QUERY_EXTENSION, 0, body, body_size);
  free (body);

  unsigned char reply[32];

  if (!result || !read_reply (state, reply))
    return false;

  if (reply[8] == 0)
    return false;

  if (major_opcode != NULL)
    *major_opcode = reply[9];

  return true;
}

static bool
set_title (InternalState *state, const char *title)
{
  size_t title_length = strlen (title);
  size_t body_size = 20 + round_up ((unsigned)title_length, 4);
  unsigned char *body = calloc (1, body_size);

  if (body == NULL)
    return false;

  write_u32_le (body, state->window);
  write_u32_le (body + 4, X11_ATOM_WM_NAME);
  write_u32_le (body + 8, X11_ATOM_STRING);

  body[12] = 8;

  write_u32_le (body + 16, (unsigned)title_length);

  memcpy (body + 20, title, title_length);
  bool result = send_request (state, 18, 0, body, body_size);

  free (body);

  return result;
}

static bool
create_window (InternalState *state, int x, int y, int w, int h)
{
  unsigned char body[36] = { 0 };
  state->window = next_resource_id (state);

  write_u32_le (body, state->window);
  write_u32_le (body + 4, state->root);
  write_u16_le (body + 8, (unsigned short)x);
  write_u16_le (body + 10, (unsigned short)y);
  write_u16_le (body + 12, (unsigned short)w);
  write_u16_le (body + 14, (unsigned short)h);
  write_u16_le (body + 18, X11_INPUT_OUTPUT);
  write_u32_le (body + 24, X11_CW_BACK_PIXEL | X11_CW_EVENT_MASK);
  write_u32_le (body + 32,
                X11_EVENT_MASK_EXPOSURE | X11_EVENT_MASK_STRUCTURE_NOTIFY
                    | X11_EVENT_MASK_KEY_PRESS | X11_EVENT_MASK_KEY_RELEASE);
  write_u32_le (body + 20, state->root_visual);

  return send_request (state, 1, (unsigned char)state->depth, body,
                       sizeof (body));
}

static bool
create_gc (InternalState *state)
{
  unsigned char body[16] = { 0 };
  state->gc = next_resource_id (state);
  write_u32_le (body, state->gc);
  write_u32_le (body + 4, state->root);
  write_u32_le (body + 8, X11_GC_FOREGROUND);
  write_u32_le (body + 12, 0x00ffffff);

  return send_request (state, 55, 0, body, sizeof (body));
}

static void
dispatch_event (PlatformState *platform_state, const unsigned char event[32])
{
  InternalState *state = platform_state->internal_state;
  unsigned char type = event[0] & 0x7f;

  if (type == X11_ERROR)
    {
      fprintf (stderr,
               "X11 error %u from request %u (minor %u, resource 0x%x)\n",
               event[1], event[10], read_u16_le (event + 8),
               read_u32_le (event + 4));
      platform_state->running = false;
    }

  else if (type == X11_EVENT_DESTROY_NOTIFY)
    platform_state->running = false;

  else if (type == X11_EVENT_CLIENT_MESSAGE
           && read_u32_le (event + 8) == state->wm_protocols
           && read_u32_le (event + 12) == state->wm_delete_window)
    platform_state->running = false;

  else if (type == 2 || type == 3)
    {
      unsigned char keycode = event[1];
      bool is_press = (type == 2);
      EventType ev;
      bool valid = true;

      switch (keycode)
        {
        case CTRL:
          ev = is_press ? KeyCtrlPress : KeyCtrlRelease;
          break;
        case SHIFT:
          ev = is_press ? KeyShiftPress : KeyShiftRelease;
          break;
        case ESC:
          ev = is_press ? KeyEscPress : KeyEscRelease;
          break;
        case ONE:
          ev = is_press ? KeyOnePress : KeyOneRelease;
          break;
        case TWO:
          ev = is_press ? KeyTwoPress : KeyTwoRelease;
          break;
        case THREE:
          ev = is_press ? KeyThreePress : KeyThreeRelease;
          break;
        case P:
          ev = is_press ? KeyPPress : KeyPRelease;
          break;
        default:
          valid = false;
          break;
        }

      if (valid)
        dyn_arr_push (&event_queue, &ev);
    }
}

// ----------------------------------------------------------------
// Platform layer
bool
platform_init (PlatformState *platform_state, const char *window_name, int x,
               int y, int w, int h, char *image_buffer)
{
  if (w <= 0 || h <= 0 || w > USHRT_MAX || h > USHRT_MAX)
    return false;

  InternalState *state = calloc (1, sizeof (*state));
  if (state == NULL)
    return false;

  event_queue = *dyn_arr_init (16, sizeof (int));

  state->fd = display_connect ();

  if (state->fd < 0)
    {
      free (state);
      return false;
    }

  unsigned char token[16] = { 0 };
  bool has_token = read_xauthority (token);
  const char *auth_name = has_token ? "MIT-MAGIC-COOKIE-1" : "";
  unsigned auth_name_length = (unsigned)strlen (auth_name);
  unsigned auth_data_length = has_token ? sizeof (token) : 0;
  size_t setup_size
      = 12 + round_up (auth_name_length, 4) + round_up (auth_data_length, 4);
  unsigned char *setup = calloc (1, setup_size);

  if (setup == NULL)
    goto fail;

  setup[0] = 'l';

  write_u16_le (setup + 2, X11_PROTOCOL_MAJOR);
  write_u16_le (setup + 4, X11_PROTOCOL_MINOR);
  write_u16_le (setup + 6, (unsigned short)auth_name_length);
  write_u16_le (setup + 8, (unsigned short)auth_data_length);

  memcpy (setup + 12, auth_name, auth_name_length);
  memcpy (setup + 12 + round_up (auth_name_length, 4), token,
          auth_data_length);
  bool setup_sent = send_all (state->fd, setup, setup_size);

  free (setup);

  if (!setup_sent)
    goto fail;

  unsigned char prefix[8];

  if (!read_all (state->fd, prefix, sizeof (prefix)))
    goto fail;

  size_t additional_size = (size_t)read_u16_le (prefix + 6) * 4;
  unsigned char *additional = malloc (additional_size);

  if (prefix[0] != 1 || additional == NULL
      || !read_all (state->fd, additional, additional_size))
    {
      if (prefix[0] != 1)
        fprintf (stderr, "X11 setup failed: %.*s\n", (int)prefix[1],
                 additional == NULL ? "" : (char *)additional);
      free (additional);
      goto fail;
    }

  if (additional_size < 32)
    {
      free (additional);
      goto fail;
    }

  state->resource_base = read_u32_le (additional + 4);
  state->resource_mask = read_u32_le (additional + 8);
  state->max_request_words = read_u16_le (additional + 18);

  unsigned vendor_length = read_u16_le (additional + 16);
  unsigned root_count = additional[20];
  unsigned format_count = additional[21];

  size_t offset = 32 + round_up (vendor_length, 4);

  if (root_count == 0 || offset + (size_t)format_count * 8 > additional_size)
    {
      free (additional);
      goto fail;
    }

  offset += (size_t)format_count * 8;

  if (offset + 40 > additional_size)
    {
      free (additional);
      goto fail;
    }

  state->root = read_u32_le (additional + offset);
  state->root_visual = read_u32_le (additional + offset + 32);
  state->depth = additional[offset + 38];
  state->bits_per_pixel = 32;
  state->scanline_pad = 32;

  for (unsigned i = 0; i < format_count; ++i)
    if (additional[32 + round_up (vendor_length, 4) + i * 8] == state->depth)
      {
        state->bits_per_pixel
            = additional[32 + round_up (vendor_length, 4) + i * 8 + 1];
        state->scanline_pad
            = additional[32 + round_up (vendor_length, 4) + i * 8 + 2];
      }

  free (additional);

  state->width = (unsigned)w;
  state->height = (unsigned)h;
  state->image_buffer = (unsigned char *)image_buffer;
  state->shmid = -1;

  if (query_extension (state, "MIT-SHM", &state->shm_opcode))
    {
      unsigned row_bytes = round_up (state->width * state->bits_per_pixel,
                                     state->scanline_pad)
                           / 8;
      state->shm_size = (size_t)row_bytes * state->height;
      state->shmid = shmget (IPC_PRIVATE, state->shm_size, IPC_CREAT | 0600);

      if (state->shmid >= 0)
        {
          state->shm_data = (unsigned char *)shmat (state->shmid, NULL, 0);
          if (state->shm_data != (void *)-1)
            {
              state->shmseg = next_resource_id (state);
              unsigned char attach_body[12] = { 0 };
              write_u32_le (attach_body, state->shmseg);
              write_u32_le (attach_body + 4, (unsigned)state->shmid);
              attach_body[8] = 0;

              if (send_request (state, state->shm_opcode, X11_SHM_ATTACH,
                                attach_body, sizeof (attach_body)))
                {
                  state->has_shm = true;
                }
            }
          else
              state->shm_data = NULL;
        }
    }

  platform_state->internal_state = state;
  platform_state->running = true;

  if (!create_window (state, x, y, w, h) || !set_title (state, window_name)
      || !intern_atom (state, "WM_PROTOCOLS", &state->wm_protocols)
      || !intern_atom (state, "WM_DELETE_WINDOW", &state->wm_delete_window))
    goto fail_with_state;

  if (state->has_shm && state->shmid >= 0)
    shmctl (state->shmid, IPC_RMID, NULL);

  unsigned char protocols[24] = { 0 };
  write_u32_le (protocols, state->window);
  write_u32_le (protocols + 4, state->wm_protocols);
  write_u32_le (protocols + 8, 4);

  protocols[12] = 32;

  write_u32_le (protocols + 16, 1);
  write_u32_le (protocols + 20, state->wm_delete_window);

  if (!send_request (state, 18, 0, protocols, sizeof (protocols))
      || !create_gc (state)
      || !send_request (
          state, 8, 0,
          (unsigned char[]){ (unsigned char)state->window,
                             (unsigned char)(state->window >> 8),
                             (unsigned char)(state->window >> 16),
                             (unsigned char)(state->window >> 24) },
          4))
    goto fail_with_state;
  return true;

fail_with_state:
  platform_shutdown (platform_state);

  return false;

fail:
  close (state->fd);
  free (state);

  return false;
}

void
platform_shutdown (PlatformState *platform_state)
{
  InternalState *state = platform_state->internal_state;

  if (state == NULL)
    return;

  if (state->has_shm && state->shmseg != 0 && state->shm_opcode != 0)
    {
      unsigned char body[4] = { 0 };
      write_u32_le (body, state->shmseg);
      send_request (state, state->shm_opcode, X11_SHM_DETACH, body,
                    sizeof (body));
    }

  if (state->shm_data != NULL && state->shm_data != (void *)-1)
    shmdt (state->shm_data);

  if (state->shmid >= 0)
    shmctl (state->shmid, IPC_RMID, NULL);

  if (state->window != 0)
    {
      unsigned char body[4];
      write_u32_le (body, state->window);
      send_request (state, 4, 0, body, sizeof (body));
    }

  close (state->fd);
  free (state);
  dyn_arr_free (&event_queue);

  platform_state->internal_state = NULL;
}

bool
platform_update (PlatformState *platform_state)
{
  InternalState *state = platform_state->internal_state;

  if (state == NULL)
    return false;

  while (true)
    {
      ssize_t received
          = recv (state->fd, state->read_buf + state->read_len,
                  sizeof (state->read_buf) - state->read_len, MSG_DONTWAIT);
      if (received > 0)
        state->read_len += (size_t)received;
      else if (received == 0)
        {
          platform_state->running = false;
          break;
        }
      else if (errno == EAGAIN || errno == EWOULDBLOCK)
        break;
      else if (errno != EINTR)
        {
          platform_state->running = false;
          break;
        }
      if (state->read_len == sizeof (state->read_buf))
        break;
    }

  while (state->read_len >= 32)
    {
      dispatch_event (platform_state, state->read_buf);
      memmove (state->read_buf, state->read_buf + 32, state->read_len - 32);
      state->read_len -= 32;
    }

  platform_dispatch_events (platform_state);

  return platform_state->running;
}

void
platform_stop (PlatformState *platform_state)
{
  platform_state->running = false;
}

void
platform_present (PlatformState *platform_state)
{
  InternalState *state = platform_state->internal_state;

  if (state == NULL || !platform_state->running)
    return;

  unsigned bytes_per_pixel = (state->bits_per_pixel + 7) / 8;
  unsigned row_bytes
      = round_up (state->width * state->bits_per_pixel, state->scanline_pad)
        / 8;

  if (state->has_shm && state->shm_data != NULL)
    {
      for (unsigned row = 0; row < state->height; ++row)
        memcpy (state->shm_data + (size_t)row * row_bytes,
                state->image_buffer + (size_t)row * state->width * 4,
                state->width * (bytes_per_pixel < 4 ? bytes_per_pixel : 4));

      unsigned char body[36] = { 0 };
      write_u32_le (body, state->window);
      write_u32_le (body + 4, state->gc);
      write_u16_le (body + 8, (unsigned short)state->width);
      write_u16_le (body + 10, (unsigned short)state->height);
      write_u16_le (body + 12, 0);
      write_u16_le (body + 14, 0);
      write_u16_le (body + 16, (unsigned short)state->width);
      write_u16_le (body + 18, (unsigned short)state->height);
      write_u16_le (body + 20, 0);
      write_u16_le (body + 22, 0);
      body[24] = (unsigned char)state->depth;
      body[25] = X11_Z_PIXMAP;
      body[26] = 0;
      body[27] = 0;
      write_u32_le (body + 28, state->shmseg);
      write_u32_le (body + 32, 0);

      bool sent = send_request (state, state->shm_opcode, X11_SHM_PUT_IMAGE,
                                body, sizeof (body));
      if (!sent)
        platform_state->running = false;

      return;
    }

  unsigned max_data = state->max_request_words * 4 - 24;
  unsigned rows = max_data / row_bytes;

  if (rows == 0)
    return;

  for (unsigned y = 0; y < state->height; y += rows)
    {
      unsigned chunk_height
          = state->height - y < rows ? state->height - y : rows;
      size_t image_size = (size_t)row_bytes * chunk_height;
      unsigned char *body = calloc (1, 20 + image_size);

      if (body == NULL)
        return;

      write_u32_le (body, state->window);
      write_u32_le (body + 4, state->gc);
      write_u16_le (body + 8, (unsigned short)state->width);
      write_u16_le (body + 10, (unsigned short)chunk_height);
      write_u16_le (body + 14, (unsigned short)y);
      body[17] = (unsigned char)state->depth;

      for (unsigned row = 0; row < chunk_height; ++row)
        memcpy (body + 20 + (size_t)row * row_bytes,
                state->image_buffer + ((size_t)y + row) * state->width * 4,
                state->width * (bytes_per_pixel < 4 ? bytes_per_pixel : 4));

      bool sent
          = send_request (state, 72, X11_Z_PIXMAP, body, 20 + image_size);
      free (body);

      if (!sent)
        {
          platform_state->running = false;
          return;
        }
    }
}
