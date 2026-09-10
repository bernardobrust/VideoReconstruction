// I have to say that Platform Layers are the most Copy-Pasty Ever invented

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

global u32 current_id = 1;

global const u32 display_object_id = 1;
global const u16 wl_registry_event_global = 0;
global const u16 shm_pool_event_format = 0;
global const u16 wl_buffer_event_release = 0;
global const u16 xdg_wm_base_event_ping = 0;
global const u16 xdg_toplevel_event_configure = 0;
global const u16 xdg_toplevel_event_close = 1;
global const u16 xdg_surface_event_configure = 0;
global const u16 wl_display_get_registry_opcode = 1;
global const u16 wl_registry_bind_opcode = 0;
global const u16 wl_compositor_create_surface_opcode = 0;
global const u16 xdg_wm_base_pong_opcode = 3;
global const u16 xdg_surface_ack_configure_opcode = 4;
global const u16 wl_shm_create_pool_opcode = 0;
global const u16 xdg_wm_base_get_xdg_surface_opcode = 2;
global const u16 wl_shm_pool_create_buffer_opcode = 0;
global const u16 wl_surface_attach_opcode = 1;
global const u16 xdg_surface_get_toplevel_opcode = 1;
global const u16 wl_surface_commit_opcode = 6;
global const u16 wl_display_error_event = 0;
global const u32 format_xrgb8888 = 1;
global const u32 header_size = 8;
global const u32 color_channels = 4;

typedef enum
{
  STATE_NONE,
  STATE_SURFACE_ACKED_CONFIGURE,
  STATE_SURFACE_ATTACHED,
} StateState;

typedef struct
{
  u32 wl_registry;
  u32 wl_shm;
  u32 wl_shm_pool;
  u32 wl_buffer;
  u32 xdg_wm_base;
  u32 xdg_surface;
  u32 wl_compositor;
  u32 wl_surface;
  u32 xdg_toplevel;
  u32 wl_seat;
  u32 wl_keyboard;
  u32 stride;

  u32 width;
  u32 height;

  u32 shm_pool_size;
  s32 shm_fd;
  u8 *shm_pool_data;

  StateState state;

  s32 fd;
  char *image_buffer;

  _Alignas (16) char read_buf[8192];
  u64 read_buf_len;
} InternalState;

// Internal functions
// ----------------------------------------------------------------
local s32
display_connect (void)
{
  char *xdg_runtime_dir = getenv ("XDG_RUNTIME_DIR");

  if (xdg_runtime_dir == NULL)
    {
      fprintf (stderr, "XDG_RUNTIME_DIR is not set\n");
      return -1;
    }

  u64 xdg_runtime_dir_len = strlen (xdg_runtime_dir);
  struct sockaddr_un addr = { 0 };
  addr.sun_family = AF_UNIX;

  assert (xdg_runtime_dir_len <= sizeof (addr.sun_path) - 1);

  u64 socket_path_len = 0;

  memcpy (addr.sun_path, xdg_runtime_dir, xdg_runtime_dir_len);
  socket_path_len += xdg_runtime_dir_len;
  addr.sun_path[socket_path_len++] = '/';
  char *display = getenv ("WAYLAND_DISPLAY");

  if (display == NULL)
    {
      char display_default[] = "wayland-0";
      u64 display_default_len = strlen (display_default);

      memcpy (addr.sun_path + socket_path_len, display_default,
              display_default_len);
      socket_path_len += display_default_len;
    }
  else
    {
      u64 display_len = strlen (display);
      memcpy (addr.sun_path + socket_path_len, display, display_len);
      socket_path_len += display_len;
    }

  s32 fd = socket (AF_UNIX, SOCK_STREAM, 0);

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

local u32
wl_display_get_registry (s32 fd)
{
  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), display_object_id);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_display_get_registry_opcode);

  u16 msg_announced_size = header_size + sizeof (current_id);

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

local u32
wl_registry_bind (s32 fd, u32 registry, u32 name, char *interface,
                  u32 interface_len, u32 version)
{
  u64 msg_size = 0;
  char msg[512] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), registry);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_registry_bind_opcode);

  u16 msg_announced_size = header_size + sizeof (name) + sizeof (interface_len)
                           + ROUNDUP_4 (interface_len) + sizeof (version)
                           + sizeof (current_id);

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

local u32
wl_compositor_create_surface (s32 fd, InternalState *state)
{
  assert (state->wl_compositor > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_compositor);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 wl_compositor_create_surface_opcode);

  u16 msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

local void
create_shared_memory_file (u64 size, InternalState *state)
{
  char name[255] = "/";

  // Generate unique name
  for (u64 i = 1; i < 16; ++i)
    name[i] = ((double)rand ()) / (double)RAND_MAX * 26 + 'a';

  name[16] = '\0';

  s32 fd = shm_open (name, O_RDWR | O_EXCL | O_CREAT, 0600);

  if (fd == -1)
    exit (errno);

  s32 shm_ret = shm_unlink (name);
  assert (shm_ret == 0 || errno == ENOENT);

  if (ftruncate (fd, size) == -1)
    exit (errno);

  state->shm_pool_data
      = (u8 *)mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  assert ((void *)-1 != state->shm_pool_data);
  assert (state->shm_pool_data != NULL);

  state->shm_fd = fd;
}

local void
xdg_wm_base_pong (s32 fd, InternalState *state, u32 ping)
{
  assert (state->xdg_wm_base > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_wm_base);
  buf_write_u16 (msg, &msg_size, sizeof (msg), xdg_wm_base_pong_opcode);

  u16 msg_announced_size = header_size + sizeof (ping);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), ping);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

local void
xdg_surface_ack_configure (s32 fd, InternalState *state, u32 configure)
{
  assert (state->xdg_surface > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 xdg_surface_ack_configure_opcode);

  u16 msg_announced_size = header_size + sizeof (configure);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), configure);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

local u32
wl_shm_create_pool (s32 fd, InternalState *state)
{
  assert (state->shm_pool_size > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_shm);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_shm_create_pool_opcode);

  u16 msg_announced_size
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

  *((s32 *)CMSG_DATA (cmsg)) = state->shm_fd;

  socket_msg.msg_controllen = CMSG_SPACE (sizeof (state->shm_fd));

  if (sendmsg (fd, &socket_msg, 0) == -1)
    exit (errno);

  return current_id;
}

local u32
xdg_wm_base_get_xdg_surface (s32 fd, InternalState *state)
{
  assert (state->xdg_wm_base > 0);
  assert (state->wl_surface > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_wm_base);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 xdg_wm_base_get_xdg_surface_opcode);

  u16 msg_announced_size
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

local u32
wl_shm_pool_create_buffer (s32 fd, InternalState *state)
{
  assert (state->wl_shm_pool > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_shm_pool);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 wl_shm_pool_create_buffer_opcode);

  u16 msg_announced_size
      = header_size + sizeof (current_id) + sizeof (u32) * 5;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  u32 offset = 0;
  buf_write_u32 (msg, &msg_size, sizeof (msg), offset);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->width);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->height);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->stride);

  u32 format = format_xrgb8888;
  buf_write_u32 (msg, &msg_size, sizeof (msg), format);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

local void
wl_surface_attach (s32 fd, InternalState *state)
{
  assert (state->wl_surface > 0);
  assert (state->wl_buffer > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_surface_attach_opcode);

  u16 msg_announced_size
      = header_size + sizeof (state->wl_buffer) + sizeof (u32) * 2;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_buffer);

  u32 x = 0, y = 0;

  buf_write_u32 (msg, &msg_size, sizeof (msg), x);
  buf_write_u32 (msg, &msg_size, sizeof (msg), y);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

local void
wl_surface_damage (s32 fd, InternalState *state)
{
  assert (state->wl_surface > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg), 2);

  u16 msg_announced_size = header_size + sizeof (u32) * 4;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_u32 (msg, &msg_size, sizeof (msg), 0);
  buf_write_u32 (msg, &msg_size, sizeof (msg), 0);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->width);
  buf_write_u32 (msg, &msg_size, sizeof (msg), state->height);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

local u32
xdg_surface_get_toplevel (s32 fd, InternalState *state)
{
  assert (state->xdg_surface > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg),
                 xdg_surface_get_toplevel_opcode);

  u16 msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

local void
wl_surface_commit (s32 fd, InternalState *state)
{
  assert (state->wl_surface > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_surface);
  buf_write_u16 (msg, &msg_size, sizeof (msg), wl_surface_commit_opcode);

  u16 msg_announced_size = header_size;

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
      exit (errno);
}

local void
xdg_toplevel_set_title (s32 fd, InternalState *state, const char *title)
{
  assert (state->xdg_toplevel > 0);

  u64 msg_size = 0;
  char msg[512] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->xdg_toplevel);
  buf_write_u16 (msg, &msg_size, sizeof (msg), 2);

  u32 title_len = strlen (title) + 1;
  u16 msg_announced_size
      = header_size + sizeof (title_len) + ROUNDUP_4 (title_len);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  buf_write_string (msg, &msg_size, sizeof (msg), (char *)title, title_len);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);
}

local u32
wl_seat_get_keyboard (s32 fd, InternalState *state)
{
  assert (state->wl_seat > 0);

  u64 msg_size = 0;
  char msg[128] = "";

  buf_write_u32 (msg, &msg_size, sizeof (msg), state->wl_seat);
  buf_write_u16 (msg, &msg_size, sizeof (msg), 1);

  u16 msg_announced_size = header_size + sizeof (current_id);

  assert (ROUNDUP_4 (msg_announced_size) == msg_announced_size);

  buf_write_u16 (msg, &msg_size, sizeof (msg), msg_announced_size);
  ++current_id;
  buf_write_u32 (msg, &msg_size, sizeof (msg), current_id);

  if ((long)msg_size != send (fd, msg, msg_size, 0))
    exit (errno);

  return current_id;
}

local void
handle_message (s32 fd, PlatformState *platform_state, char **msg,
                u64 *msg_len)
{
  InternalState *state = (InternalState *)platform_state->internal_state;

  assert (*msg_len >= 8);

  u32 object_id = buf_read_u32 (msg, msg_len);

  assert (object_id <= current_id);

  u16 opcode = buf_read_u16 (msg, msg_len);
  u16 announced_size = buf_read_u16 (msg, msg_len);

  assert (ROUNDUP_4 (announced_size) <= announced_size);

  u32 message_header_size
      = sizeof (object_id) + sizeof (opcode) + sizeof (announced_size);

  assert (announced_size <= message_header_size + *msg_len);

  if (object_id == state->wl_registry && opcode == wl_registry_event_global)
    {
      u32 name = buf_read_u32 (msg, msg_len);
      u32 interface_len = buf_read_u32 (msg, msg_len);
      u32 padded_interface_len = ROUNDUP_4 (interface_len);
      char interface[512] = "";

      assert (padded_interface_len <= sizeof (interface));

      buf_read_n (msg, msg_len, interface, padded_interface_len);

      assert (interface[interface_len - 1] == 0);

      u32 version = buf_read_u32 (msg, msg_len);

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
      u32 target_object_id = buf_read_u32 (msg, msg_len);
      u32 code = buf_read_u32 (msg, msg_len);
      char error[512] = "";
      u32 error_len = buf_read_u32 (msg, msg_len);

      buf_read_n (msg, msg_len, error, ROUNDUP_4 (error_len));

      fprintf (stderr, "fatal error: target_object_id=%u code=%u error=%s\n",
               target_object_id, code, error);

      exit (EINVAL);
    }
  else if (object_id == state->wl_shm && opcode == shm_pool_event_format)
    {
      // u32 format = buf_read_u32(msg, msg_len);

      return;
    }
  else if (object_id == state->wl_buffer && opcode == wl_buffer_event_release)
    {
      return;
    }
  else if (object_id == state->xdg_wm_base && opcode == xdg_wm_base_event_ping)
    {
      u32 ping = buf_read_u32 (msg, msg_len);

      xdg_wm_base_pong (fd, state, ping);

      return;
    }
  else if (object_id == state->xdg_toplevel)
    {
      if (opcode == xdg_toplevel_event_configure)
        {
          __attribute__ ((unused)) u32 w = buf_read_u32 (msg, msg_len);
          __attribute__ ((unused)) u32 h = buf_read_u32 (msg, msg_len);
          u32 len = buf_read_u32 (msg, msg_len);
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
          u32 array_len = buf_read_u32 (msg, msg_len);
          u32 padded_len = ROUNDUP_4 (array_len);
          *msg += padded_len;
          *msg_len -= padded_len;
        }
      return;
    }
  else if (object_id == state->xdg_surface
           && opcode == xdg_surface_event_configure)
    {
      u32 configure = buf_read_u32 (msg, msg_len);

      xdg_surface_ack_configure (fd, state, configure);

      state->state = STATE_SURFACE_ACKED_CONFIGURE;

      return;
    }
  else if (state->wl_seat != 0 && object_id == state->wl_seat)
    {
      if (opcode == 0)
        {
          u32 capabilities = buf_read_u32 (msg, msg_len);

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
          u32 name_len = buf_read_u32 (msg, msg_len);
          u32 padded_len = ROUNDUP_4 (name_len);
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
          u32 keys_len = buf_read_u32 (msg, msg_len);
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
          buf_read_u32 (msg, msg_len);
          buf_read_u32 (msg, msg_len);
          u32 key = buf_read_u32 (msg, msg_len);
          u32 key_state = buf_read_u32 (msg, msg_len);

          EventType ev;
          bool valid = true;
          bool is_press = (key_state != 0);

          switch (key)
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

  assert (0 && "Unimplemented message received");
}

local void
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
      u16 announced_size = *(u16 *)(state->read_buf + 6);
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
      u64 msg_len = announced_size;
      handle_message (state->fd, platform_state, &msg_ptr, &msg_len);

      memmove (state->read_buf, state->read_buf + announced_size,
               state->read_buf_len - announced_size);
      state->read_buf_len -= announced_size;
    }
}
// ----------------------------------------------------------------

// Platform layer
bool
platform_init (PlatformState *platform_state, const char *window_name, s32 x,
               s32 y, s32 w, s32 h, char *image_buffer)
{
  // x and y not used
  (void)x;
  (void)y;

  platform_state->internal_state = malloc (sizeof (InternalState));
  assert (platform_state->internal_state != NULL
          && "Failed to allocate memory from internal state");

  event_queue = *dyn_arr_init (16, sizeof (int));

  InternalState *state = (InternalState *)platform_state->internal_state;
  memset (state, 0, sizeof (InternalState));

  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;

  s32 time_ret = gettimeofday (&tv, NULL);
  assert (time_ret != -1);

  srand (tv.tv_sec * 1000 * 1000 + tv.tv_usec);

  s32 fd = display_connect ();
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
  dyn_arr_free (&event_queue);
  platform_state->internal_state = NULL;
}

bool
platform_update (PlatformState *platform_state)
{
  read_and_dispatch (platform_state, false);
  platform_dispatch_events ();

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
