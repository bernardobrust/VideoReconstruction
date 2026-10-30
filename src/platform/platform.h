// General platform interface

#pragma once

#include "arena.h"
#include "dyn_arr.h"
#include "macros.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

typedef struct
{
  void *internal_state;
  bool running;
  Arena arena;
} PlatformState;

typedef struct
{
  s32 width, height;
} ScreenResolution;

typedef struct
{
  void *data;
  u64 size;
  intptr_t native_file;
  void *native_mapping;
} PlatformMappedFile;

typedef enum
{
  KeyCtrlPress,
  KeyCtrlRelease,
  KeyShiftPress,
  KeyShiftRelease,
  KeyEscPress,
  KeyEscRelease,
  KeyOnePress,
  KeyOneRelease,
  KeyTwoPress,
  KeyTwoRelease,
  KeyThreePress,
  KeyThreeRelease,
  KeyPPress,
  KeyPRelease,
  KeyLeftPress,
  KeyLeftRelease,
  KeyRightPress,
  KeyRightRelease,
  KeyUpPress,
  KeyUpRelease,
  KeyDownPress,
  KeyDownRelease,
  KeySpacePress,
  KeySpaceRelease,
  KeyFPress,
  KeyFRelease,
  MouseLeftPress,
  MouseLeftRelease,
  MouseRightPress,
  MouseRightRelease,
  MouseMove,
} EventType;

typedef struct
{
  EventType type;
  s32 x, y;
} PlatformEvent;

// platform_update will push the events onto the queue, from where
// platform_dispatch_events will call the associated function (such as
// input_set_key_pressed)
extern DynArr event_queue;
void platform_dispatch_events (void);

#define PLATFORM_EVENT_CAPACITY 4096

inline local void *
platform_storage_init (PlatformState *platform_state, u64 internal_size,
                       u64 internal_alignment, u64 arena_capacity)
{
  assert (platform_state != NULL);
  arena_init (&platform_state->arena, arena_capacity);

  void *state = arena_alloc (&platform_state->arena, internal_size,
                             internal_alignment);
  void *events = arena_alloc (&platform_state->arena,
                              PLATFORM_EVENT_CAPACITY * sizeof (PlatformEvent),
                              _Alignof (PlatformEvent));
  assert (state != NULL && events != NULL);

  memset (state, 0, internal_size);
  event_queue = dyn_arr_init_fixed (events, PLATFORM_EVENT_CAPACITY,
                                    sizeof (PlatformEvent));
  return state;
}

inline local void
platform_storage_release (PlatformState *platform_state)
{
  dyn_arr_free (&event_queue);
  arena_release (&platform_state->arena);
}

inline local void
platform_event_push (EventType type, s32 x, s32 y)
{
  PlatformEvent ev = { type, x, y };
  if (!dyn_arr_try_push (&event_queue, &ev))
    {
      platform_dispatch_events ();
      bool pushed = dyn_arr_try_push (&event_queue, &ev);
      assert (pushed);
      (void)pushed;
    }
}

void platform_init (PlatformState *platform_state, const byte *window_name,
                    s32 x, s32 y, s32 w, s32 h, byte *image_buffer);
void platform_shutdown (PlatformState *platform_state);

bool platform_update (PlatformState *platform_state);
void platform_present (PlatformState *platform_state);
ScreenResolution
platform_get_screen_resolution (PlatformState *platform_state);
void platform_set_fullscreen (PlatformState *platform_state, bool fullscreen);

f64 platform_get_time (void);
void platform_sleep (f64 ms);
s32 platform_file_exists (byte *filepath);
bool platform_mapped_file_create (PlatformMappedFile *file, const char *path,
                                  u64 size);
void platform_mapped_file_close (PlatformMappedFile *file);

void platform_stop (PlatformState *platform_state);
