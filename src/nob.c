#include <stdlib.h>
#include <string.h>

#include "platform/macros.h"

#define NOB_IMPLEMENTATION
#include "lib/nob.h"

#define FLAG_IMPLEMENTATION
#include "lib/flag.h"

#define BUILD_DIR "build/"

// Helper functions
local bool
str_eq (const byte *a, const byte *b)
{
  return strcmp (a, b) == 0;
}

local bool
is_linux_platform (const byte *platform)
{
  return str_eq (platform, "gnu_linux_x11")
         || str_eq (platform, "gnu_linux_wayland");
}

local bool
is_windows_platform (const byte *platform)
{
  return str_eq (platform, "windows");
}

#define WINDOWS_MOD_LIBS_URL                                                  \
  "https://github.com/bernardobrust/VideoReconstruction/releases/download/"   \
  "Experimental/windows_mod_libs.zip"

#define LINUX_MOD_LIBS_URL                                                    \
  "https://github.com/bernardobrust/VideoReconstruction/releases/download/"   \
  "Experimental/gnu_linux_mod_libs.zip"

#define LINUX_LIBS_DIR "lib"

local bool
windows_mod_libs_exist (void)
{
  return nob_file_exists ("lib/windows/lib/avcodec.lib")
         && nob_file_exists ("lib/windows/lib/avformat.lib")
         && nob_file_exists ("lib/windows/lib/avutil.lib")
         && nob_file_exists ("lib/windows/lib/swscale.lib")
         && nob_file_exists ("lib/windows/include/libavcodec/avcodec.h");
}

local bool
pull_windows_mod_libs (void)
{
  const byte *zip_path = BUILD_DIR "windows_mod_libs.zip";

  // If this does not work as expected check if the macro is pointing to the
  // latest version and that the file was zipped corectly
  nob_log (NOB_INFO, "Pulling modified libraries from %s",
           WINDOWS_MOD_LIBS_URL);

  Nob_Cmd cmd = { 0 };
  nob_cmd_append (&cmd, "curl", "-f", "-L", "-o", zip_path,
                  WINDOWS_MOD_LIBS_URL);
  if (!nob_cmd_run (&cmd))
    {
      nob_log (NOB_ERROR, "Failed to download modified libraries from %s",
               WINDOWS_MOD_LIBS_URL);
      nob_cmd_free (cmd);
      nob_delete_file (zip_path);
      return false;
    }
  nob_cmd_free (cmd);

  nob_log (NOB_INFO, "Extracting modified libraries to lib/");
  cmd = (Nob_Cmd){ 0 };
  nob_cmd_append (&cmd, "tar", "-xf", zip_path, "-C", "lib");
  if (!nob_cmd_run (&cmd))
    {
      nob_log (NOB_ERROR, "Failed to extract %s into lib/", zip_path);
      nob_cmd_free (cmd);
      nob_delete_file (zip_path);
      return false;
    }
  nob_cmd_free (cmd);

  nob_delete_file (zip_path);
  nob_log (NOB_INFO, "Successfully pulled and extracted modified libraries!");
  return true;
}

local bool
linux_mod_libs_exist (void)
{
  return nob_file_exists (LINUX_LIBS_DIR "/gnu_linux/lib/libavcodec.a")
         && nob_file_exists (LINUX_LIBS_DIR "/gnu_linux/lib/libavformat.a")
         && nob_file_exists (LINUX_LIBS_DIR "/gnu_linux/lib/libavutil.a")
         && nob_file_exists (LINUX_LIBS_DIR "/gnu_linux/lib/libswscale.a")
         && nob_file_exists (LINUX_LIBS_DIR "/gnu_linux/lib/libdav1d.a")
         && nob_file_exists (LINUX_LIBS_DIR
                             "/gnu_linux/include/libavcodec/avcodec.h")
         && nob_file_exists (LINUX_LIBS_DIR
                             "/gnu_linux/include/libavutil/motion_vector.h")
         && nob_file_exists (LINUX_LIBS_DIR
                             "/gnu_linux/include/dav1d/dav1d.h");
}

local bool
pull_linux_mod_libs (void)
{
  const byte *zip_path = BUILD_DIR "gnu_linux_mod_libs.zip";

  nob_log (NOB_INFO, "Pulling modified libraries from %s", LINUX_MOD_LIBS_URL);

  Nob_Cmd cmd = { 0 };
  nob_cmd_append (&cmd, "curl", "-f", "-L", "-o", zip_path,
                  LINUX_MOD_LIBS_URL);
  if (!nob_cmd_run (&cmd))
    {
      nob_log (NOB_ERROR, "Failed to download modified libraries from %s",
               LINUX_MOD_LIBS_URL);
      nob_cmd_free (cmd);
      if (nob_file_exists (zip_path))
        nob_delete_file (zip_path);
      return false;
    }
  nob_cmd_free (cmd);

  nob_log (NOB_INFO, "Extracting modified libraries to %s/gnu_linux/",
           LINUX_LIBS_DIR);
  cmd = (Nob_Cmd){ 0 };
  nob_cmd_append (&cmd, "unzip", "-o", "-q", zip_path, "-d", LINUX_LIBS_DIR);
  if (!nob_cmd_run (&cmd))
    {
      nob_log (NOB_ERROR, "Failed to extract %s into %s", zip_path,
               LINUX_LIBS_DIR);
      nob_cmd_free (cmd);
      if (nob_file_exists (zip_path))
        nob_delete_file (zip_path);
      return false;
    }
  nob_cmd_free (cmd);

  nob_delete_file (zip_path);
  if (!linux_mod_libs_exist ())
    {
      nob_log (NOB_ERROR, "Downloaded Linux library bundle is incomplete");
      return false;
    }

  nob_log (NOB_INFO, "Successfully pulled and extracted modified libraries!");
  return true;
}

local bool
pull_mod_libs (void)
{
#ifdef _WIN32
  return pull_windows_mod_libs ();
#else
  return pull_linux_mod_libs ();
#endif
}

s32
main (s32 argc, byte **argv)
{
  NOB_GO_REBUILD_URSELF (argc, argv);

  if (!nob_mkdir_if_not_exists (BUILD_DIR))
    return EXIT_FAILURE;

  // CLI parsing
  byte **target = flag_str ("target", "", "Target to build");
  byte **platform = flag_str ("platform", "", "Platform to build");
  byte **mode
      = flag_str ("mode", "", "What kind of binary to generate");

  if (!flag_parse (argc, argv))
    {
      flag_print_error (stderr);
      return EXIT_FAILURE;
    }

  argc = flag_rest_argc ();
  argv = flag_rest_argv ();

  if ((str_eq (*target, "pull") || str_eq (*target, "pull-libs")
          || str_eq (*target, "pull_libs")))
    {
      if (!pull_mod_libs ())
        return EXIT_FAILURE;
      return EXIT_SUCCESS;
    }
  else if (str_eq (*target, "pull-gnu-linux-libs"))
    {
      if (!pull_linux_mod_libs ())
        return EXIT_FAILURE;
      return EXIT_SUCCESS;
    }
  else if (str_eq (*target, "pull-windows-libs"))
    {
      if (!pull_windows_mod_libs ())
        return EXIT_FAILURE;
      return EXIT_SUCCESS;
    }

  // Validation
  bool valid_target = str_eq (*target, "inspector")
                      || str_eq (*target, "reconstructor")
                      || str_eq (*target, "tests");

  if (!valid_target)
    {
      nob_log (NOB_ERROR,
               "Invalid target, use one of "
               "'inspector', 'reconstructor', 'tests' or 'pull_libs'");
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

  bool valid_mode
      = str_eq (*mode, "debug") || str_eq (*mode, "release");

  if (!valid_mode)
    {
      nob_log (NOB_ERROR,
               "Invalid build type, use one of 'debug' or 'release'");
      return EXIT_FAILURE;
    }

  nob_log (INFO, "Building target: %s, for platform: %s", *target, *platform);
  nob_log (INFO, "Build mode: %s", *mode);

  Nob_Cmd compile_cmd = { 0 };

  // Compiler selection
  if (is_windows_platform (*platform))
    nob_cmd_append (&compile_cmd, "cl");
  else
    nob_cmd_append (&compile_cmd, "gcc");

  // Output binary
  byte bin_name[256] = { 0 };

  strcat (bin_name, BUILD_DIR);
  strcat (bin_name, *target);
  strcat (bin_name, "_");
  strcat (bin_name, *mode);
  strcat (bin_name, "_");
  strcat (bin_name, *platform);

  if (is_windows_platform (*platform))
    strcat (bin_name, ".exe");

  // Compiler options
  if (is_windows_platform (*platform))
    {
      nob_cmd_append (&compile_cmd, "/nologo", "/std:c11");

      if (str_eq (*mode, "debug"))
        nob_cmd_append (&compile_cmd, "/W4", "/external:W0",
                        "/external:anglebrackets", "/Zi", "/Od");
      else
        nob_cmd_append (&compile_cmd, "/O2", "/GL", "/DNDEBUG");

      // MSVC output executable
      byte output_option[64] = { 0 };
      snprintf (output_option, sizeof (output_option), "/Fe:%s", bin_name);

      nob_cmd_append (&compile_cmd, output_option);
    }
  else
    {
      nob_cmd_append (&compile_cmd, "--std=c11");

      if (str_eq (*mode, "debug"))
        nob_cmd_append (&compile_cmd, "-Wall", "-Wextra", "-Werror",
                        "-Wpedantic", "-ggdb", "-Og");
      else
        nob_cmd_append (&compile_cmd, "-Ofast", "-march=native", "-flto",
                        "-DNDEBUG");

      nob_cmd_append (&compile_cmd, "-o", bin_name);
    }

  // Entry point
  byte entry_point[256] = { 0 };
  strcat (entry_point, *target);
  strcat (entry_point, "/main.c");
  nob_cmd_append (&compile_cmd, entry_point);

  // Math
  nob_cmd_append (&compile_cmd, "math/basic.c");

  // Data structures
  // Tests include the implementations directly.
  if (str_eq (*target, "tests"))
    nob_cmd_append (&compile_cmd, "tests/dyn_arr.test.c");
  else
    nob_cmd_append (&compile_cmd, "ds/dyn_arr.c");

  // Platform utilities
  nob_cmd_append (&compile_cmd, "platform/utility.c");

  // Platform layer common code
  if (is_linux_platform (*platform))
    nob_cmd_append (&compile_cmd, "platform/platform_gnu_linux.c");

  // Platform layer
  byte platform_layer[256] = { 0 };
  strcat (platform_layer, "platform/platform_");
  strcat (platform_layer, *platform);
  strcat (platform_layer, ".c");
  nob_cmd_append (&compile_cmd, platform_layer);

  // Event dispatcher
  nob_cmd_append (&compile_cmd, "platform/event_dispatcher.c");

  // Input
  nob_cmd_append (&compile_cmd, "input/input.c");

  // Renderer
  nob_cmd_append (&compile_cmd, "renderer/renderer.c");

  // Decoder
  nob_cmd_append (&compile_cmd, "decode/decode.c");

  // Includes
  if (is_windows_platform (*platform))
    nob_cmd_append (&compile_cmd, "/Ilib", "/Ids", "/Iplatform", "/Imath",
                    "/Irenderer", "/Iinput", "/Idecode");
  else
    nob_cmd_append (&compile_cmd, "-Ilib", "-Ids", "-Iplatform", "-Imath",
                    "-Irenderer", "-Iinput", "-Idecode",
                    "-Ilib/gnu_linux/include");

  // Libraries
  if (is_windows_platform (*platform))
    {
      // Modified library pulling
      if (!windows_mod_libs_exist ())
        {
          nob_log (NOB_WARNING,
                   "Modified FFmpeg libraries not found in lib/windows/");
          printf ("They can be pulled from: %s\n", WINDOWS_MOD_LIBS_URL);
          printf ("Do you want to download and extract them now? [Y/n]: ");
          fflush (stdout);

          s32 response = getchar ();
          if (response == 'y' || response == 'Y' || response == '\n'
              || response == '\r')
            {
              if (!pull_windows_mod_libs ())
                return EXIT_FAILURE;
            }
          else
            {
              nob_log (NOB_ERROR,
                       "Modified libraries are required to build on Windows");
              return EXIT_FAILURE;
            }
        }

      nob_cmd_append (&compile_cmd, "/Ilib/windows/include");
      nob_cmd_append (&compile_cmd, "/link", "/LIBPATH:lib/windows/lib",
                      "avformat.lib", "avcodec.lib", "swscale.lib",
                      "avutil.lib", "libdav1d.a", "bcrypt.lib", "shell32.lib");

      if (str_eq (*mode, "release"))
        nob_cmd_append (&compile_cmd, "/LTCG");
    }
  else
    {
      if (!linux_mod_libs_exist ())
        {
          nob_log (NOB_WARNING,
                   "Modified FFmpeg libraries not found in %s/gnu_linux/",
                   LINUX_LIBS_DIR);
          printf ("They can be pulled from: %s\n", LINUX_MOD_LIBS_URL);
          printf ("Do you want to download and extract them now? [Y/n]: ");
          fflush (stdout);

          s32 response = getchar ();
          if (response == 'y' || response == 'Y' || response == '\n'
              || response == '\r')
            {
              if (!pull_linux_mod_libs ())
                return EXIT_FAILURE;
            }
          else
            {
              nob_log (
                  NOB_ERROR,
                  "Modified libraries are required to build on GNU + Linux");
              return EXIT_FAILURE;
            }
        }

      nob_cmd_append (&compile_cmd, "-Llib/gnu_linux/lib", "-lavformat",
                      "-lavcodec", "-ldav1d", "-ldl", "-lswscale", "-lavutil",
                      "-lm", "-latomic", "-pthread");
    }

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc23-extensions"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#endif

  if (!nob_cmd_run (&compile_cmd))
    return EXIT_FAILURE;

#ifdef __clang__
#pragma clang diagnostic pop
#endif

  return EXIT_SUCCESS;
}
