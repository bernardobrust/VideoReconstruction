// Use these definition if and only if we are on Windows

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <timeapi.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "dyn_arr.h"
#include "platform.h"

// IDK what this does really, but it dosen't work without this
#pragma comment (lib, "user32.lib")
#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "winmm.lib")

typedef struct
{
  HWND hwnd;
  HDC hdc;
  int width;
  int height;
  char *image_buffer;
  BITMAPINFO bitmap_info;
  bool time_period_set;
} InternalState;

static LARGE_INTEGER win32_perf_frequency;
static bool win32_perf_frequency_initialized = false;

static void
win32_init_perf_frequency (void)
{
  if (!win32_perf_frequency_initialized)
    {
      QueryPerformanceFrequency (&win32_perf_frequency);
      win32_perf_frequency_initialized = true;
    }
}

static void
win32_enable_dpi_awareness (void)
{
  HMODULE user32 = GetModuleHandleA ("user32.dll");
  if (user32 != NULL)
    {
      typedef BOOL (WINAPI * SetProcessDpiAwarenessContextProc) (
          DPI_AWARENESS_CONTEXT);
      SetProcessDpiAwarenessContextProc set_dpi_awareness_context
          = (SetProcessDpiAwarenessContextProc)(void *)GetProcAddress (
              user32, "SetProcessDpiAwarenessContext");

      if (set_dpi_awareness_context != NULL)
        {
          set_dpi_awareness_context (
              DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
          return;
        }

      typedef BOOL (WINAPI * SetProcessDPIAwareProc) (VOID);
      SetProcessDPIAwareProc set_dpi_aware
          = (SetProcessDPIAwareProc)(void *)GetProcAddress (
              user32, "SetProcessDPIAware");

      if (set_dpi_aware != NULL)
        {
          set_dpi_aware ();
        }
    }
}

static LRESULT CALLBACK
win32_window_proc (HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param)
{
  PlatformState *platform_state = NULL;

  if (msg == WM_NCCREATE)
    {
      CREATESTRUCTA *create = (CREATESTRUCTA *)l_param;
      platform_state = (PlatformState *)create->lpCreateParams;
      SetWindowLongPtrA (hwnd, GWLP_USERDATA, (LONG_PTR)platform_state);
    }
  else
    {
      platform_state
          = (PlatformState *)GetWindowLongPtrA (hwnd, GWLP_USERDATA);
    }

  switch (msg)
    {
    case WM_CLOSE:
      {
        if (platform_state != NULL)
          platform_state->running = false;
        return 0;
      }

    case WM_DESTROY:
      {
        if (platform_state != NULL)
          platform_state->running = false;
        return 0;
      }

    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT:
      {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint (hwnd, &ps);

        if (platform_state != NULL && platform_state->internal_state != NULL)
          {
            InternalState *state
                = (InternalState *)platform_state->internal_state;

            if (state->image_buffer != NULL)
              {
                RECT client_rect;
                GetClientRect (hwnd, &client_rect);
                int client_w = client_rect.right - client_rect.left;
                int client_h = client_rect.bottom - client_rect.top;

                StretchDIBits (hdc, 0, 0, client_w, client_h, 0, 0,
                               state->width, state->height,
                               state->image_buffer, &state->bitmap_info,
                               DIB_RGB_COLORS, SRCCOPY);
              }
          }

        EndPaint (hwnd, &ps);
        return 0;
      }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      {
        WORD vk_code = LOWORD (w_param);
        bool is_press = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
        bool was_down = (l_param & (1 << 30)) != 0;

        if (is_press && was_down)
          break;

        EventType ev;
        bool valid = true;

        switch (vk_code)
          {
          case VK_CONTROL:
            ev = is_press ? KeyCtrlPress : KeyCtrlRelease;
            break;
          case VK_SHIFT:
            ev = is_press ? KeyShiftPress : KeyShiftRelease;
            break;
          case VK_ESCAPE:
            ev = is_press ? KeyEscPress : KeyEscRelease;
            break;
          case '1':
            ev = is_press ? KeyOnePress : KeyOneRelease;
            break;
          case '2':
            ev = is_press ? KeyTwoPress : KeyTwoRelease;
            break;
          case '3':
            ev = is_press ? KeyThreePress : KeyThreeRelease;
            break;
          case 'P':
            ev = is_press ? KeyPPress : KeyPRelease;
            break;
          default:
            valid = false;
            break;
          }

        if (valid)
          dyn_arr_push (&event_queue, &ev);

        if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP)
          {
            if (vk_code == VK_F4 && (l_param & (1 << 29)))
              return DefWindowProcA (hwnd, msg, w_param, l_param);
            if (!valid)
              return DefWindowProcA (hwnd, msg, w_param, l_param);
            return 0;
          }

        return 0;
      }

    case WM_KILLFOCUS:
      {
        EventType ev;
        ev = KeyCtrlRelease;
        dyn_arr_push (&event_queue, &ev);
        ev = KeyShiftRelease;
        dyn_arr_push (&event_queue, &ev);
        ev = KeyEscRelease;
        dyn_arr_push (&event_queue, &ev);
        ev = KeyOneRelease;
        dyn_arr_push (&event_queue, &ev);
        ev = KeyTwoRelease;
        dyn_arr_push (&event_queue, &ev);
        ev = KeyThreeRelease;
        dyn_arr_push (&event_queue, &ev);
        ev = KeyPRelease;
        dyn_arr_push (&event_queue, &ev);
        return 0;
      }

    default:
      break;
    }

  return DefWindowProcA (hwnd, msg, w_param, l_param);
}

bool
platform_init (PlatformState *platform_state, const char *window_name, int x,
               int y, int w, int h, char *image_buffer)
{
  if (platform_state == NULL)
    return false;

  if (w <= 0 || h <= 0)
    return false;

  InternalState *state = (InternalState *)calloc (1, sizeof (InternalState));
  if (state == NULL)
    return false;

  event_queue = *dyn_arr_init (16, sizeof (int));

  win32_init_perf_frequency ();
  win32_enable_dpi_awareness ();

  if (timeBeginPeriod (1) == TIMERR_NOERROR)
    state->time_period_set = true;

  HINSTANCE instance = GetModuleHandleA (NULL);
  const char *class_name = "VideoReconstructionWindowClass";

  WNDCLASSEXA wc = { 0 };
  wc.cbSize = sizeof (wc);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = win32_window_proc;
  wc.hInstance = instance;
  wc.hCursor = LoadCursorA (NULL, IDC_ARROW);
  wc.lpszClassName = class_name;

  RegisterClassExA (&wc);

  RECT window_rect = { 0, 0, w, h };
  DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  AdjustWindowRectEx (&window_rect, style, FALSE, 0);

  int window_w = window_rect.right - window_rect.left;
  int window_h = window_rect.bottom - window_rect.top;

  int screen_w = GetSystemMetrics (SM_CXSCREEN);
  int screen_h = GetSystemMetrics (SM_CYSCREEN);

  int pos_x = (x > 0) ? x : (screen_w - window_w) / 2;
  int pos_y = (y > 0) ? y : (screen_h - window_h) / 2;
  if (pos_x < 0)
    pos_x = CW_USEDEFAULT;
  if (pos_y < 0)
    pos_y = CW_USEDEFAULT;

  HWND hwnd = CreateWindowExA (0, class_name,
                               window_name != NULL ? window_name
                                                   : "Video Reconstruction",
                               style, pos_x, pos_y, window_w, window_h, NULL,
                               NULL, instance, platform_state);

  if (hwnd == NULL)
    {
      if (state->time_period_set)
        timeEndPeriod (1);
      free (state);
      return false;
    }

  HDC hdc = GetDC (hwnd);
  if (hdc == NULL)
    {
      DestroyWindow (hwnd);
      if (state->time_period_set)
        timeEndPeriod (1);
      free (state);
      return false;
    }

  SetStretchBltMode (hdc, COLORONCOLOR);

  state->hwnd = hwnd;
  state->hdc = hdc;
  state->width = w;
  state->height = h;
  state->image_buffer = image_buffer;

  state->bitmap_info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  state->bitmap_info.bmiHeader.biWidth = w;
  state->bitmap_info.bmiHeader.biHeight = -h;
  state->bitmap_info.bmiHeader.biPlanes = 1;
  state->bitmap_info.bmiHeader.biBitCount = 32;
  state->bitmap_info.bmiHeader.biCompression = BI_RGB;

  platform_state->internal_state = state;
  platform_state->running = true;

  ShowWindow (hwnd, SW_SHOW);
  UpdateWindow (hwnd);

  return true;
}

void
platform_shutdown (PlatformState *platform_state)
{
  if (platform_state == NULL || platform_state->internal_state == NULL)
    return;

  InternalState *state = (InternalState *)platform_state->internal_state;

  if (state->hdc != NULL && state->hwnd != NULL)
    {
      ReleaseDC (state->hwnd, state->hdc);
      state->hdc = NULL;
    }

  if (state->hwnd != NULL)
    {
      SetWindowLongPtrA (state->hwnd, GWLP_USERDATA, (LONG_PTR)NULL);
      DestroyWindow (state->hwnd);
      state->hwnd = NULL;
    }

  if (state->time_period_set)
    {
      timeEndPeriod (1);
      state->time_period_set = false;
    }

  free (state);
  platform_state->internal_state = NULL;
  platform_state->running = false;

  dyn_arr_free (&event_queue);
}

bool
platform_update (PlatformState *platform_state)
{
  if (platform_state == NULL || platform_state->internal_state == NULL)
    return false;

  MSG msg;
  while (PeekMessageA (&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT)
        {
          platform_state->running = false;
        }

      TranslateMessage (&msg);
      DispatchMessageA (&msg);
    }

  platform_dispatch_events ();

  return platform_state->running;
}

void
platform_present (PlatformState *platform_state)
{
  if (platform_state == NULL || !platform_state->running)
    return;

  InternalState *state = (InternalState *)platform_state->internal_state;
  if (state == NULL || state->hdc == NULL || state->image_buffer == NULL)
    return;

  RECT client_rect;
  GetClientRect (state->hwnd, &client_rect);
  int client_w = client_rect.right - client_rect.left;
  int client_h = client_rect.bottom - client_rect.top;

  StretchDIBits (state->hdc, 0, 0, client_w, client_h, 0, 0, state->width,
                 state->height, state->image_buffer, &state->bitmap_info,
                 DIB_RGB_COLORS, SRCCOPY);
}

double
platform_get_time (void)
{
  if (!win32_perf_frequency_initialized)
    win32_init_perf_frequency ();

  LARGE_INTEGER counter;
  QueryPerformanceCounter (&counter);

  return ((double)counter.QuadPart * 1000.0)
         / (double)win32_perf_frequency.QuadPart;
}

void
platform_sleep (double ms)
{
  if (ms <= 0.0)
    return;

  if (!win32_perf_frequency_initialized)
    win32_init_perf_frequency ();

  LARGE_INTEGER start, current;
  QueryPerformanceCounter (&start);

  double target_counts
      = (ms * (double)win32_perf_frequency.QuadPart) / 1000.0;

  if (ms > 2.0)
      Sleep ((DWORD)(ms - 1.0));

  for (;;)
    {
      QueryPerformanceCounter (&current);
      if ((double)(current.QuadPart - start.QuadPart) >= target_counts)
        break;
      YieldProcessor ();
    }
}

// Returns: 0 => OK
// 1 => wrong path
// 2 => inaccessible
int
platform_file_exists (char *filepath)
{
  if (filepath == NULL || filepath[0] == '\0')
    return 1;

  DWORD attrs = GetFileAttributesA (filepath);
  if (attrs == INVALID_FILE_ATTRIBUTES)
    {
      DWORD err = GetLastError ();
      if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND
          || err == ERROR_INVALID_NAME || err == ERROR_BAD_NETPATH)
        return 1;
      else if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
        return 2;
      else
        return 1;
    }

  if (attrs & FILE_ATTRIBUTE_DIRECTORY)
    return 0;

  HANDLE file = CreateFileA (
      filepath, GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (file == INVALID_HANDLE_VALUE)
    {
      DWORD err = GetLastError ();
      if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
        return 1;
      else if (err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION)
        return 2;
    }
  else
      CloseHandle (file);

  return 0;
}

void
platform_stop (PlatformState *platform_state)
{
  if (platform_state != NULL)
    platform_state->running = false;
}