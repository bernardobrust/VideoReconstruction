#include <stdlib.h>
#include <string.h>

/*
** Build configuration
**
** We're ignoring windows for now (no one should use that anyway)
*/

#define NOB_IMPLEMENTATION
#include "lib/nob.h"

#define FLAG_IMPLEMENTATION
#include "lib/flag.h"

// Directories
#define BUILD_DIR "build/"

// Compiler information
#define CC "gcc"
#define C_VERSION "--std=c11"

int
main (int argc, char **argv)
{
  // Build setup
  NOB_GO_REBUILD_URSELF (argc, argv);
  if (!nob_mkdir_if_not_exists (BUILD_DIR))
    return EXIT_FAILURE;

  // Argument parsing for build
  // No default target
  char **target = flag_str ("target", "", "Target to build");
  char **platform
      = flag_str ("platform", "gnu_linux_x11", "Platform to build");
  char **build_type
      = flag_str ("build_type", "debug", "Debug, release or test");

  if (!flag_parse (argc, argv))
    {
      flag_print_error (stderr);
      return EXIT_FAILURE;
    }

  argc = flag_rest_argc ();
  argv = flag_rest_argv ();

  // Validating
  if (!(strcmp (*target, "inspector") == 0
        || strcmp (*target, "reconstructor") == 0))
    {
      // Invalid build target
      nob_log (ERROR,
               "Invalid target, use one of 'inspector' or 'reconstructor'");
    }

  if (!(strcmp (*platform, "gnu_linux_x11") == 0
        || strcmp (*platform, "gnu_linux_wayland") == 0
        || strcmp (*platform, "windows") == 0))
    {
      // Invalid build target
      nob_log (ERROR, "Invalid platform, use one of 'gnu_linux_x11', "
                      "'gnu_linux_wayland', 'windows'");
    }

  if (!(strcmp (*build_type, "debug") == 0
        || strcmp (*build_type, "release") == 0
        || strcmp (*build_type, "test") == 0))
    {
      // Invalid build type
      nob_log (ERROR,
               "Invalid build type, use one of 'debug', 'release', 'test'");
    }

  nob_log (INFO, "Building target: %s, for platform: %s", *target, *platform);
  nob_log (INFO, "Build mode: %s", *build_type);

  // Nob build
  Nob_Cmd cmd = { 0 };

  // Basics
  char bin_name[64] = BUILD_DIR;
  strcat (bin_name, *target);
  strcat (bin_name, "_");
  strcat (bin_name, *build_type);
  strcat (bin_name, "_");
  strcat (bin_name, *platform);

  nob_cmd_append (&cmd, CC, C_VERSION);
  nob_cmd_append (&cmd, "-o", bin_name);

  // Debug information and warnings and release flags
  if (strcmp (*build_type, "debug") == 0 || strcmp (*build_type, "test") == 0)
    nob_cmd_append (&cmd, "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-ggdb",
                    "-Og");
  else
    // Yes, -Ofast will be worth it
    nob_cmd_append (&cmd, "-Ofast", "-march=native", "-flto", "-DNDEBUG");

  // Source files and includes
  // Entry point
  char entry_point[64] = { 0 };
  strcat (entry_point, *target);
  strcat (entry_point, "/");
  strcmp (*build_type, "test") == 0 ? strcat (entry_point, "main.test.c")
                                    : strcat (entry_point, "main.c");
  nob_cmd_append (&cmd, entry_point);

  // Math
  nob_cmd_append (&cmd, "math/basic.c");

  // Data Structures
  // Test files include the sources directly
  nob_cmd_append (&cmd, strcmp (*build_type, "test") == 0 ? "ds/dyn_arr.test.c"
                                                          : "ds/dyn_arr.c");

  // Platform utility (buf_read, buf_write, etc.) and common implementations
  nob_cmd_append (&cmd, "platform/utility.c");
  if (strcmp (*platform, "gnu_linux_x11") == 0
      || strcmp (*platform, "gnu_linux_wayland") == 0)
    nob_cmd_append (&cmd, "platform/platform_gnu_linux.c");

  // Platform layer implementation
  char platform_layer[64] = "platform/platform_";
  strcat (platform_layer, *platform);
  strcat (platform_layer, ".c");
  nob_cmd_append (&cmd, platform_layer);
  nob_cmd_append (&cmd, "platform/event_dispatcher.c");

  // Input
  nob_cmd_append (&cmd, "input/input.c");

  // The renderer
  nob_cmd_append (&cmd, "renderer/renderer.c");

  // Anyway we include all directories
  nob_cmd_append (&cmd, "-Ilib", "-Ids", "-Iplatform", "-Imath", "-Irenderer");

  // Include the own directory
  char self_dir[64] = "-I";
  strcat (self_dir, *target);
  nob_cmd_append (&cmd, self_dir);

  if (!nob_cmd_run (&cmd))
    return EXIT_FAILURE;
}
