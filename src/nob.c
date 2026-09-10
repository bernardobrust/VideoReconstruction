#include <stdlib.h>
#include <string.h>

#include "platform/macros.h"

#define NOB_IMPLEMENTATION
#include "lib/nob.h"

#define FLAG_IMPLEMENTATION
#include "lib/flag.h"

#define BUILD_DIR "build/"

// Helper functions
static bool
str_eq (const byte *a, const byte *b)
{
  return strcmp (a, b) == 0;
}

static bool
is_linux_platform (const byte *platform)
{
  return str_eq (platform, "gnu_linux_x11")
         || str_eq (platform, "gnu_linux_wayland");
}

static bool
is_windows_platform (const byte *platform)
{
  return str_eq (platform, "windows");
}

int
main (s32 argc, byte **argv)
{
  NOB_GO_REBUILD_URSELF (argc, argv);

  if (!nob_mkdir_if_not_exists (BUILD_DIR))
    return EXIT_FAILURE;

  // CLI parsing
  byte **target = flag_str ("target", "", "Target to build");
  byte **platform = flag_str ("platform", "", "Platform to build");
  byte **build_type
      = flag_str ("build_type", "", "What kind of binary to generate");

  if (!flag_parse (argc, argv))
    {
      flag_print_error (stderr);
      return EXIT_FAILURE;
    }

  argc = flag_rest_argc ();
  argv = flag_rest_argv ();

  // Validation
  bool valid_target = str_eq (*target, "inspector")
                      || str_eq (*target, "reconstructor")
                      || str_eq (*target, "tests");

  if (!valid_target)
    {
      nob_log (NOB_ERROR, "Invalid target, use one of "
                          "'inspector', 'reconstructor' or 'tests'");
      return EXIT_FAILURE;
    }

  bool valid_platform = str_eq (*platform, "gnu_linux_x11")
                        || str_eq (*platform, "gnu_linux_wayland")
                        || str_eq (*platform, "windows");

  if (!valid_platform)
    {
      nob_log (NOB_ERROR, "Invalid platform, use one of "
                          "'gnu_linux_x11', 'gnu_linux_wayland' or 'windows'");
      return EXIT_FAILURE;
    }

  bool valid_build_type
      = str_eq (*build_type, "debug") || str_eq (*build_type, "release");

  if (!valid_build_type)
    {
      nob_log (NOB_ERROR,
               "Invalid build type, use one of 'debug' or 'release'");
      return EXIT_FAILURE;
    }

  nob_log (INFO, "Building target: %s, for platform: %s", *target, *platform);
  nob_log (INFO, "Build mode: %s", *build_type);

  Nob_Cmd cmd = { 0 };

  // Compiler selection
  if (is_windows_platform (*platform))
    nob_cmd_append (&cmd, "cl");
  else
    nob_cmd_append (&cmd, "gcc");

  // Output binary
  byte bin_name[256] = { 0 };

  strcat (bin_name, BUILD_DIR);
  strcat (bin_name, *target);
  strcat (bin_name, "_");
  strcat (bin_name, *build_type);
  strcat (bin_name, "_");
  strcat (bin_name, *platform);

  if (is_windows_platform (*platform))
    strcat (bin_name, ".exe");

  // Compiler options
  if (is_windows_platform (*platform))
    {
      nob_cmd_append (&cmd, "/nologo", "/std:c11");

      /*if (str_eq (*build_type, "debug"))
        nob_cmd_append (&cmd, "/W4", "/WX", "/external:W0",
                        "/external:anglebrackets", "/Zi", "/Od");*/
      if (str_eq (*build_type, "debug"))
        nob_cmd_append (&cmd, "/W4", "/external:W0", "/external:anglebrackets",
                        "/Zi", "/Od");
      else
        nob_cmd_append (&cmd, "/O2", "/GL", "/DNDEBUG");

      // MSVC output executable
      byte output_option[64] = { 0 };
      snprintf (output_option, sizeof (output_option), "/Fe:%s", bin_name);

      nob_cmd_append (&cmd, output_option);
    }
  else
    {
      nob_cmd_append (&cmd, "--std=c11");

      if (str_eq (*build_type, "debug"))
        nob_cmd_append (&cmd, "-Wall", "-Wextra", "-Werror", "-Wpedantic",
                        "-ggdb", "-Og");
      else
        nob_cmd_append (&cmd, "-Ofast", "-march=native", "-flto", "-DNDEBUG");

      nob_cmd_append (&cmd, "-o", bin_name);
    }

  // Entry point
  byte entry_point[256] = { 0 };

  strcat (entry_point, *target);
  strcat (entry_point, "/main.c");

  nob_cmd_append (&cmd, entry_point);

  // Math
  nob_cmd_append (&cmd, "math/basic.c");

  // Data structures
  // Tests include the implementations directly.
  if (str_eq (*target, "tests"))
    nob_cmd_append (&cmd, "tests/dyn_arr.test.c");
  else
    nob_cmd_append (&cmd, "ds/dyn_arr.c");

  // Platform utilities
  nob_cmd_append (&cmd, "platform/utility.c");

  // Platform layer common code
  if (is_linux_platform (*platform))
    nob_cmd_append (&cmd, "platform/platform_gnu_linux.c");

  // Platform layer
  byte platform_layer[256] = { 0 };

  strcat (platform_layer, "platform/platform_");
  strcat (platform_layer, *platform);
  strcat (platform_layer, ".c");

  nob_cmd_append (&cmd, platform_layer);

  // Event dispatcher
  nob_cmd_append (&cmd, "platform/event_dispatcher.c");

  // Input
  nob_cmd_append (&cmd, "input/input.c");

  // Renderer
  nob_cmd_append (&cmd, "renderer/renderer.c");

  // Decoder
  nob_cmd_append (&cmd, "decode/decode.c");

  // Includes
  if (is_windows_platform (*platform))
    nob_cmd_append (&cmd, "/Ilib", "/Ids", "/Iplatform", "/Imath",
                    "/Irenderer", "/Iinput", "/Idecode");
  else
    nob_cmd_append (&cmd, "-Ilib", "-Ids", "-Iplatform", "-Imath",
                    "-Irenderer", "-Iinput", "-Idecode");

  // Libraries
  if (is_windows_platform (*platform))
    {
      // Mine is at:
      // C:\Users\berna\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg.Shared_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0.1-full_build-shared\include\libavcodec\avcodec.h
      byte *ffmpeg_dir = getenv ("FFMPEG_DIR");
      if (ffmpeg_dir == NULL)
        {
          nob_log (ERROR,
                   "FFMPEG_DIR is not set, please define where you installed "
                   "your FFmpeg libraries on this environment variable");
          return EXIT_FAILURE;
        }

      byte ffmpeg_include[1024];
      byte ffmpeg_lib[1024];

      snprintf (ffmpeg_include, sizeof (ffmpeg_include), "/I%s/include",
                ffmpeg_dir);

      snprintf (ffmpeg_lib, sizeof (ffmpeg_lib), "/LIBPATH:%s/lib",
                ffmpeg_dir);

      nob_cmd_append (&cmd, ffmpeg_include);
      nob_cmd_append (&cmd, "/link", ffmpeg_lib, "avformat.lib", "avcodec.lib",
                      "swscale.lib", "avutil.lib", "shell32.lib");

      if (str_eq (*build_type, "release"))
        nob_cmd_append (&cmd, "/LTCG");
    }
  else
    nob_cmd_append (&cmd, "-lm", "-lavformat", "-lavcodec", "-lswscale",
                    "-lavutil");

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif

  if (!nob_cmd_run (&cmd))
    return EXIT_FAILURE;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

  return EXIT_SUCCESS;
}
