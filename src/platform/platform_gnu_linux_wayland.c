// I have to say that Platform Layers are the most Copy-Pastey Ever invented

// Use these definitions if and only if we're on GNU + Linux and Wayland

#define _POSIX_C_SOURCE 200112L

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "basic.h"
#include "platform.h"
#include "utility.h"

// Don't memorize numbers
enum WaylandKeyValues
{
  CTRL = 29,
  SHIFT = 42,
  ESC = 1,
  ONE = 2,
  TWO = 3,
  THREE = 4,
  P = 25,
};

static unsigned current_id = 1;

static const unsigned display_object_id = 1;
static const unsigned short wl_registry_event_global = 0;
static const unsigned short shm_pool_event_format = 0;
static const unsigned short wl_buffer_event_release = 0;
static const unsigned short xdg_wm_base_event_ping = 0;
static const unsigned short xdg_toplevel_event_configure = 0;
static const unsigned short xdg_toplevel_event_close = 1;
static const unsigned short xdg_surface_event_configure = 0;
static const unsigned short wl_display_get_registry_opcode = 1;
static const unsigned short wl_registry_bind_opcode = 0;
static const unsigned short wl_compositor_create_surface_opcode = 0;
static const unsigned short xdg_wm_base_pong_opcode = 3;
static const unsigned short xdg_surface_ack_configure_opcode = 4;
static const unsigned short wl_shm_create_pool_opcode = 0;
static const unsigned short xdg_wm_base_get_xdg_surface_opcode = 2;
static const unsigned short wl_shm_pool_create_buffer_opcode = 0;
static const unsigned short wl_surface_attach_opcode = 1;
static const unsigned short xdg_surface_get_toplevel_opcode = 1;
static const unsigned short wl_surface_commit_opcode = 6;
static const unsigned short wl_display_error_event = 0;
static const unsigned format_xrgb8888 = 1;
static const unsigned header_size = 8;
static const unsigned color_channels = 4;

typedef enum
{
  STATE_NONE,
  STATE_SURFACE_ACKED_CONFIGURE,
  STATE_SURFACE_ATTACHED,
} StateState;

typedef struct
{
  unsigned wl_registry;
  unsigned wl_shm;
  unsigned wl_shm_pool;
  unsigned wl_buffer;
  unsigned xdg_wm_base;
  unsigned xdg_surface;
  unsigned wl_compositor;
  unsigned wl_surface;
  unsigned xdg_toplevel;
  unsigned wl_seat;
  unsigned wl_keyboard;
  unsigned stride;

  unsigned width;
  unsigned height;

  unsigned shm_pool_size;
  int shm_fd;
  unsigned char *shm_pool_data;

  StateState state;

  int fd;
  char *image_buffer;

  _Alignas (16) char read_buf[8192];
  unsigned long read_buf_len;
} InternalState;

// Internal functions
// ----------------------------------------------------------------
static int
display_connect ()
{
  char *xdg_runtime_dir = getenv ("XDG_RUNTIME_DIR");

  if (xdg_runtime_dir == NULL)
    {
      fprintf (stderr, "XDG_RUNTIME_DIR is not set\n");
      return -1;
    }

  unsigned long xdg_runtime_dir_len = strlen (xdg_runtime_dir);
  struct sockaddr_un addr = { 0 };
  addr.sun_family = AF_UNIX;

  assert (xdg_runtime_dir_len <= sizeof (addr.sun_path) - 1);

  unsigned long socket_path_len = 0;

  memcpy (addr.sun_path, xdg_runtime_dir, xdg_runtime_dir_len);
  socket_path_len += xdg_runtime_dir_len;
  addr.sun_path[socket_path_len++] = '/';
  char *display = getenv ("WAYLAND_DISPLAY");

  if (display == NULL)
    {
      char display_default[] = "wayland-0";
      unsigned long display_default_len = strlen (display_default);

      memcpy (addr.sun_path + socket_path_len, display_default,
              display_default_len);
      socket_path_len += display_default_len;
    }
  else
    {
      unsigned long display_len = strlen (display);
      memcpy (addr.sun_path + socket_path_len, display, display_len);
      socket_path_len += display_len;
    }

  int fd = socket (AF_UNIX, SOCK_STREAM, 0);

  if (fd == -1)
    {
      exit (errno);
    }

  if (connect (fd, (struct sockaddr *)&addr, sizeof (addr)) == -1)
    {
      exit (errno);
    }

  return fd;
}

static unsigned
wl_display_get_registry (int fd)
{
  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), display_object_id);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_display_get_registry_opcode);

  unsigned short msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, MSG_DONTWAIT))
    {
      exit (errno);
    }

  return current_id;
}

static unsigned
wl_registry_bind (int fd, unsigned registry, unsigned name, char *interface,
                  unsigned interface_len, unsigned version)
{
  unsigned long msg_size = 0;
  char msg[512] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), registry);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_registry_bind_opcode);

  unsigned short msg_announced_size
      = header_size + sizeof (name) + sizeof (interface_len)
        + ROUNDUP_4 (interface_len) + sizeof (version) + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), name);
  buf_write_string (msg, &msg_size, sizeof (msg), interface, interface_len);
  buf_write_u32 (msg, &msg_size, sizeof (msg), version);

  ++current_id;

  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  assert (msg_size == ROUNDUP_4 (msg_size));

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    {
      exit (errno);
    }

  return current_id;
}

static unsigned
wl_compositor_create_surface (int fd, InternalState *state)
{
  assert (state->wl_compositor > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_compositor);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 wl_compositor_create_surface_opcode);

  unsigned short msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

static void
create_shared_memory_file (unsigned long size, InternalState *state)
{
  char name[255] = "/";

  // Generate unique name
  for (unsigned long i = 1; i < 16; ++i)
    {
      name[i] = ((double)rand ()) / (double)RAND_MAX * 26 + 'a';
    }
  name[16] = '\0';

  int fd = shm_open (name, O_RDWR | O_EXCL | O_CREAT, 0600);

  if (fd == -1)
    exit (errno);

  assert (shm_unlink (name) != -1);

  if (ftruncate (fd, size) == -1)
    exit (errno);

  state->shm_pool_data = (unsigned char *)mmap (
      NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  assert ((void *)-1 != state->shm_pool_data);
  assert (state->shm_pool_data != NULL);

  state->shm_fd = fd;
}

static void
xdg_wm_base_pong (int fd, InternalState *state, unsigned ping)
{
  assert (state->xdg_wm_base > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_wm_base);
  buf_write_u16 (msg, &msg_size, sizeof (msg), xdg_wm_base_pong_opcode);

  unsigned short msg_announced_size = header_size + sizeof (ping);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), ping);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

static void
xdg_surface_ack_configure (int fd, InternalState *state, unsigned configure)
{
  assert (state->xdg_surface > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 xdg_surface_ack_configure_opcode);

  unsigned short msg_announced_size = header_size + sizeof (configure);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), configure);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

static unsigned
wl_shm_create_pool (int fd, InternalState *state)
{
  assert (state->shm_pool_size > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_shm);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_shm_create_pool_opcode);

  unsigned short msg_announced_size
      = header_size + sizeof (current_id) + sizeof (state->shm_pool_size);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->shm_pool_size);

  assert (ROUNDUP_4 (msg_size) == msg_size);

  char buf[CMSG_SPACE (sizeof (state->shm_fd))] = "";

  struct iovec io = { .iov_base = msg, .iov_len = msg_size };
  struct msghdr socket_msg = { .msg_name = NULL,
                               .msg_namelen = 0,
                               .msg_iov = &io,
                               .msg_iovlen = 1,
                               .msg_control = buf,
                               .msg_controllen = sizeof (buf),
                               .msg_flags = 0 };

  struct cmsghdr *cmsg = CMSG_FIRSTHDR (&socket_msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN (sizeof (state->shm_fd));

  *((int *)CMSG_DATA (cmsg)) = state->shm_fd;

  socket_msg.msg_controllen = CMSG_SPACE (sizeof (state->shm_fd));

  if (sendmsg (fd, &socket_msg, 0) == -1)
    {
      exit (errno);
    }

  return current_id;
}

static unsigned
xdg_wm_base_get_xdg_surface (int fd, InternalState *state)
{
  assert (state->xdg_wm_base > 0);
  assert (state->wl_surface > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_wm_base);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 xdg_wm_base_get_xdg_surface_opcode);

  unsigned short msg_announced_size
      = header_size + sizeof (current_id) + sizeof (state->wl_surface);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

static unsigned
wl_shm_pool_create_buffer (int fd, InternalState *state)
{
  assert (state->wl_shm_pool > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_shm_pool);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 wl_shm_pool_create_buffer_opcode);

  unsigned short msg_announced_size
      = header_size + sizeof (current_id) + sizeof (unsigned) * 5;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  unsigned offset = 0;
  buf_write_u32 (msg, &msg_size, sizeof (msg), offset);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->width);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->height);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->stride);

  unsigned format = format_xrgb8888;
  buf_write_u32 (msg, &msg_size, sizeof (msg), format);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

static void
wl_surface_attach (int fd, InternalState *state)
{
  assert (state->wl_surface > 0);
  assert (state->wl_buffer > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_surface_attach_opcode);

  unsigned short msg_announced_size
      = header_size + sizeof (state->wl_buffer) + sizeof (unsigned) * 2;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_buffer);

  unsigned x = 0, y = 0;

  buf_write_u32 (msg, &msg_size, sizeof (msg), x);
  buf_write_u32 (msg, &msg_size, sizeof (msg), y);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

static void
wl_surface_damage (int fd, InternalState *state)
{
  assert (state->wl_surface > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg), 2);

  unsigned short msg_announced_size = header_size + sizeof (unsigned) * 4;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), 0);
  buf_write_u32 (msg, &msg_size, sizeof (msg), 0);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->width);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->height);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

static unsigned
xdg_surface_get_toplevel (int fd, InternalState *state)
{
  assert (state->xdg_surface > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 xdg_surface_get_toplevel_opcode);

  unsigned short msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

static void
wl_surface_commit (int fd, InternalState *state)
{
  assert (state->wl_surface > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_surface_commit_opcode);

  unsigned short msg_announced_size = header_size;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    {
      exit (errno);
    }
}

static void
xdg_toplevel_set_title (int fd, InternalState *state, const char *title)
{
  assert (state->xdg_toplevel > 0);

  unsigned long msg_size = 0;
  char msg[512] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_toplevel);
  buf_write_u16 (msg, &msg_size, sizeof (msg), 2);

  unsigned title_len = strlen (title) + 1;
  unsigned short msg_announced_size
      = header_size + sizeof (title_len) + ROUNDUP_4 (title_len);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_string (msg, &msg_size, sizeof (msg), (char *)title, title_len);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

static unsigned
wl_seat_get_keyboard (int fd, InternalState *state)
{
  assert (state->wl_seat > 0);

  unsigned long msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_seat);
  buf_write_u16 (msg, &msg_size, sizeof (msg), 1);

  unsigned short msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

static void
handle_message (int fd, PlatformState *platform_state, char **msg,
                unsigned long *msg_len)
{
  InternalState *state = (InternalState *)platform_state->internal_state;

  assert (*msg_len >= 8);

  unsigned object_id = buf_read_u32 (msg, msg_len);

  assert (object_id <= current_id);

  unsigned short opcode = buf_read_u16 (msg, msg_len);
  unsigned short announced_size = buf_read_u16 (msg, msg_len);

  assert (ROUNDUP_4 (announced_size) <= announced_size);

  unsigned message_header_size
      = sizeof (object_id) + sizeof (opcode) + sizeof (announced_size);

  assert (announced_size <= message_header_size + *msg_len);

  if (object_id == state->wl_registry && opcode == wl_registry_event_global)
    {
      unsigned name = buf_read_u32 (msg, msg_len);
      unsigned interface_len = buf_read_u32 (msg, msg_len);
      unsigned padded_interface_len = ROUNDUP_4 (interface_len);
      char interface[512] = "";

      assert (padded_interface_len <= sizeof (interface));

      buf_read_n (msg, msg_len, interface, padded_interface_len);

      assert (interface[interface_len - 1] == 0);

      unsigned version = buf_read_u32 (msg, msg_len);

      assert (announced_size
              == sizeof (object_id) + sizeof (announced_size) + sizeof (opcode)
                     + sizeof (name) + sizeof (interface_len)
                     + padded_interface_len + sizeof (version));

      char wl_shm_interface[] = "wl_shm";

      if (strcmp (wl_shm_interface, interface) == 0)
        state->wl_shm = wl_registry_bind (fd, state->wl_registry, name,
                                          interface, interface_len, version);

      char xdg_wm_base_interface[] = "xdg_wm_base";

      if (strcmp (xdg_wm_base_interface, interface) == 0)
        state->xdg_wm_base = wl_registry_bind (
            fd, state->wl_registry, name, interface, interface_len, version);

      char wl_compositor_interface[] = "wl_compositor";

      if (strcmp (wl_compositor_interface, interface) == 0)
        state->wl_compositor = wl_registry_bind (
            fd, state->wl_registry, name, interface, interface_len, version);

      char wl_seat_interface[] = "wl_seat";

      if (strcmp (wl_seat_interface, interface) == 0)
        state->wl_seat = wl_registry_bind (fd, state->wl_registry, name,
                                           interface, interface_len, version);

      return;
    }
  else if (object_id == display_object_id && opcode == wl_display_error_event)
    {
      unsigned target_object_id = buf_read_u32 (msg, msg_len);
      unsigned code = buf_read_u32 (msg, msg_len);
      char error[512] = "";
      unsigned error_len = buf_read_u32 (msg, msg_len);

      buf_read_n (msg, msg_len, error, ROUNDUP_4 (error_len));

      fprintf (stderr, "fatal error: target_object_id=%u code=%u error=%s\n",
               target_object_id, code, error);

      exit (EINVAL);
    }
  else if (object_id == state->wl_shm && opcode == shm_pool_event_format)
    {
      // unsigned format = buf_read_u32(msg, msg_len);

      return;
    }
  else if (object_id == state->wl_buffer && opcode == wl_buffer_event_release)
    {
      return;
    }
  else if (object_id == state->xdg_wm_base && opcode == xdg_wm_base_event_ping)
    {
      unsigned ping = buf_read_u32 (msg, msg_len);

      xdg_wm_base_pong (fd, state, ping);

      return;
    }
  else if (object_id == state->xdg_toplevel)
    {
      if (opcode == xdg_toplevel_event_configure)
        {
          __attribute__ ((unused)) unsigned w = buf_read_u32 (msg, msg_len);
          __attribute__ ((unused)) unsigned h = buf_read_u32 (msg, msg_len);
          unsigned len = buf_read_u32 (msg, msg_len);
          char buf[256] = "";

          assert (len <= sizeof (buf));

          buf_read_n (msg, msg_len, buf, len);
        }
      else if (opcode == xdg_toplevel_event_close)
        {
          platform_state->running = false;
        }
      else if (opcode == 3)
        {
          unsigned array_len = buf_read_u32 (msg, msg_len);
          unsigned padded_len = ROUNDUP_4 (array_len);
          *msg += padded_len;
          *msg_len -= padded_len;
        }
      return;
    }
  else if (object_id == state->xdg_surface
           && opcode == xdg_surface_event_configure)
    {
      unsigned configure = buf_read_u32 (msg, msg_len);

      xdg_surface_ack_configure (fd, state, configure);

      state->state = STATE_SURFACE_ACKED_CONFIGURE;

      return;
    }
  else if (state->wl_seat != 0 && object_id == state->wl_seat)
    {
      if (opcode == 0)
        {
          unsigned capabilities = buf_read_u32 (msg, msg_len);

          if (capabilities & 2)
            {
              if (state->wl_keyboard == 0)
                {
                  state->wl_keyboard = wl_seat_get_keyboard (fd, state);
                }
            }
        }
      else if (opcode == 1)
        {
          unsigned name_len = buf_read_u32 (msg, msg_len);
          unsigned padded_len = ROUNDUP_4 (name_len);
          *msg += padded_len;
          *msg_len -= padded_len;
        }
      return;
    }
  else if (state->wl_keyboard != 0 && object_id == state->wl_keyboard)
    {
      if (opcode == 1)
        {
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
          unsigned keys_len = buf_read_u32 (msg, msg_len);
          *msg += ROUNDUP_4 (keys_len);
          *msg_len -= ROUNDUP_4 (keys_len);
        }
      else if (opcode == 2)
        {
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
        }
      else if (opcode == 3)
        {
          // Pending input inplementation
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
          // unsigned key = buf_read_u32(msg, msg_len);
          // unsigned key_state = buf_read_u32(msg, msg_len);

          // Input system
          /*
          input::Key tracked_key = input::Key::BLANK;
          switch (key)
            {
            case KeyValues::w:
              {
                tracked_key = input::Key::w;
              }
              break;
            case KeyValues::a:
              {
                tracked_key = input::Key::a;
              }
              break;
            case KeyValues::s:
              {
                tracked_key = input::Key::s;
              }
              break;
            case KeyValues::d:
              {
                tracked_key = input::Key::d;
              }
              break;
            case KeyValues::ESC:
              {
                tracked_key = input::Key::ESC;
              }
              break;
            default:
              break;
            }

          if (tracked_key != input::Key::BLANK)
            {
              key_state ? input::set_key_pressed (tracked_key)
                        : input::set_key_released (tracked_key);
            }
            */
        }
      else if (opcode == 4)
        {
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
        }
      else if (opcode == 5)
        {
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
        }
      return;
    }
  else if (state->wl_surface != 0 && object_id == state->wl_surface)
    {
      if (opcode == 0 || opcode == 1)
        {
          buf_read_u32 (msg, msg_len);
        }
      return;
    }

  fprintf (stderr, "object_id=%u opcode=%u msg_len=%lu\n", object_id, opcode,
           *msg_len);

  assert (0 && "Unimplemented message recived");
}

static void
read_and_dispatch (PlatformState *platform_state, bool block)
{
  InternalState *state = (InternalState *)platform_state->internal_state;

  while (true)
    {
      long bytes_received
          = recv (state->fd, state->read_buf + state->read_buf_len,
                  sizeof (state->read_buf) - state->read_buf_len,
                  block ? 0 : MSG_DONTWAIT);
      if (bytes_received < 0)
        {
          if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
              break;
            }
          else if (errno == EINTR)
            {
              continue;
            }
          else
            {
              fprintf (stderr, "recv failed: %s\n", strerror (errno));

              exit (errno);
            }
        }
      else if (bytes_received == 0)
        {
          fprintf (stderr, "Wayland connection closed by server.\n");

          exit (0);
        }

      state->read_buf_len += bytes_received;
      block = false;
    }

  while (state->read_buf_len >= 8)
    {
      unsigned short announced_size = *(unsigned short *)(state->read_buf + 6);
      if (announced_size < 8)
        {
          fprintf (stderr, "Invalid Wayland message size: %d\n",
                   announced_size);

          exit (EINVAL);
        }
      if (state->read_buf_len < announced_size)
        {
          break; // Incomplete message
        }

      char *msg_ptr = state->read_buf;
      unsigned long msg_len = announced_size;
      handle_message (state->fd, platform_state, &msg_ptr, &msg_len);

      memmove (state->read_buf, state->read_buf + announced_size,
               state->read_buf_len - announced_size);
      state->read_buf_len -= announced_size;
    }
}
// ----------------------------------------------------------------

// Platform layer
bool
platform_init (PlatformState *platform_state, const char *window_name, int x,
               int y, int w, int h, char *image_buffer)
{
  // x and y not used
  (void)x;
  (void)y;

  platform_state->internal_state = malloc (sizeof (InternalState));
  assert (platform_state->internal_state != NULL
          && "Failed to alocate memory from internal state");

  event_queue = *dyn_arr_init(16, sizeof(int));

  InternalState *state = (InternalState *)platform_state->internal_state;
  memset (state, 0, sizeof (InternalState));

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  assert (gettimeofday (&tv, NULL) != -1);

  srand (tv.tv_sec * 1000 * 1000 + tv.tv_usec);

  int fd = display_connect ();
  if (fd == -1)
    {
      return false;
    }
  state->fd = fd;
  state->image_buffer = image_buffer;

  platform_state->running = true;

  state->wl_registry = wl_display_get_registry (fd);
  state->width = w;
  state->height = h;
  state->stride = w * color_channels;

  // Single buffering
  state->shm_pool_size = h * state->stride;
  create_shared_memory_file (state->shm_pool_size, state);

  while (state->wl_compositor == 0 || state->wl_shm == 0
         || state->xdg_wm_base == 0)
    {
      read_and_dispatch (platform_state, true);
    }

  state->wl_surface = wl_compositor_create_surface (fd, state);
  state->wl_shm_pool = wl_shm_create_pool (fd, state);
  state->wl_buffer = wl_shm_pool_create_buffer (fd, state);
  state->xdg_surface = xdg_wm_base_get_xdg_surface (fd, state);
  state->xdg_toplevel = xdg_surface_get_toplevel (fd, state);

  xdg_toplevel_set_title (fd, state, window_name);

  wl_surface_commit (fd, state);

  while (state->state != STATE_SURFACE_ACKED_CONFIGURE)
    {
      read_and_dispatch (platform_state, true);
    }

  return true;
}

void
platform_shutdown (PlatformState *platform_state)
{
  InternalState *state = (InternalState *)platform_state->internal_state;
  if (state == NULL)
    return;

  if (state->shm_pool_data != NULL)
      munmap (state->shm_pool_data, state->shm_pool_size);

  if (state->shm_fd != -1)
      close (state->shm_fd);

  if (state->fd != -1)
      close (state->fd);

  free (state);
  dyn_arr_free(&event_queue);
  platform_state->internal_state = NULL;
}

bool
platform_update (PlatformState *platform_state)
{
  read_and_dispatch (platform_state, false);

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
  InternalState *state = (InternalState *)platform_state->internal_state;

  // Copy local image buffer to shared memory
  memcpy (state->shm_pool_data, state->image_buffer, state->shm_pool_size);

  // Attach, damage, and commit
  wl_surface_attach (state->fd, state);
  wl_surface_damage (state->fd, state);
  wl_surface_commit (state->fd, state);
}
