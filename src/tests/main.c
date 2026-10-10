#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ASTF_IMPLEMENTATION
#define ASTF_STRIP_PREFIX
// #define ASTF_NO_ANSI_COLORS
#include "astf.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#include "dyn_arr.test.h"
#include "input.test.h"
#include "math.test.h"
#include "renderer.test.h"
#include "utility.test.h"

#include "macros.h"

int
main (s32 argc, byte **argv)
{
  // CLI parsing
  // enabled_tests defines a whitelist
  // disabled_tests defines a blacklist
  Flag_List *enabled_tests = flag_list ("enable", "List flag");
  Flag_List *disabled_tests = flag_list ("disable", "List flag");

  if (!flag_parse (argc, argv))
    {
      flag_print_error (stderr);
      return EXIT_FAILURE;
    }

  argc = flag_rest_argc ();
  argv = flag_rest_argv ();

  if (enabled_tests->count > 0 && disabled_tests->count > 0)
    {
      printf ("Please work with either a whitelist (-enable) or a blacklist "
              "(-disable)\n");
      return EXIT_FAILURE;
    }

  // Tests to run
  bool dyn_arr, math, input, utility, renderer;

  // Whitelist
  if (enabled_tests->count > 0)
    {
      dyn_arr = false;
      math = input = utility = renderer = false;

      for (u64 i = 0; i < enabled_tests->count; ++i)
        {
          // We use the bool to short-circuit and avoid doing strcmp every
          // iteration
          if (dyn_arr == false
              && strcmp (enabled_tests->items[i], "dyn_arr") == 0)
            dyn_arr = true;
          if (math == false && strcmp (enabled_tests->items[i], "math") == 0)
            math = true;
          if (input == false && strcmp (enabled_tests->items[i], "input") == 0)
            input = true;
          if (utility == false
              && strcmp (enabled_tests->items[i], "utility") == 0)
            utility = true;
          if (renderer == false
              && strcmp (enabled_tests->items[i], "renderer") == 0)
            renderer = true;
        }
    }
  // Blacklist
  else
    {
      dyn_arr = true;
      math = input = utility = renderer = true;

      for (u64 i = 0; i < disabled_tests->count; ++i)
        {
          if (dyn_arr == true
              && strcmp (disabled_tests->items[i], "dyn_arr") == 0)
            dyn_arr = false;
        }
      for (u64 i = 0; i < disabled_tests->count; ++i)
        {
          if (strcmp (disabled_tests->items[i], "math") == 0)
            math = false;
          if (strcmp (disabled_tests->items[i], "input") == 0)
            input = false;
          if (strcmp (disabled_tests->items[i], "utility") == 0)
            utility = false;
          if (strcmp (disabled_tests->items[i], "renderer") == 0)
            renderer = false;
        }
    }

  // Running the tests
  start_testing ();

  if (dyn_arr)
    {
      start_group ("Dynamic Array Tests");
      dyn_arr_all_tests ();
      end_group ();
    }
  if (math)
    {
      start_group ("Math Macro Tests");
      math_macro_tests ();
      end_group ();
      start_group ("Math Function Tests");
      math_function_tests ();
      end_group ();
      start_group ("Math Geometry Tests");
      math_geometry_tests ();
      end_group ();
    }
  if (input)
    {
      start_group ("Input Tests");
      input_all_tests ();
      end_group ();
    }
  if (utility)
    {
      start_group ("Platform Utility Tests");
      utility_all_tests ();
      end_group ();
    }
  if (renderer)
    {
      start_group ("Renderer Color Tests");
      renderer_color_tests ();
      end_group ();
      start_group ("Renderer Rasterization Tests");
      renderer_raster_tests ();
      end_group ();
      start_group ("Renderer Geometry Tests");
      renderer_geometry_tests ();
      end_group ();
      start_group ("Renderer Transparency Tests");
      renderer_transparency_tests ();
      end_group ();
    }

  stop_testing ();
}
