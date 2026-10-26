#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ASTF_IMPLEMENTATION
#define ASTF_STRIP_PREFIX
// #define ASTF_NO_ANSI_COLORS
#include "astf.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#include "arena.test.h"
#include "dyn_arr.test.h"
#include "input.test.h"
#include "math.test.h"
#include "renderer.test.h"
#include "threading.test.h"
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
  bool dyn_arr, arena, math, input, utility, renderer, threading;

  // Whitelist
  if (enabled_tests->count > 0)
    {
      dyn_arr = false;
      arena = math = input = utility = renderer = threading = false;

      for (u64 i = 0; i < enabled_tests->count; ++i)
        {
          // We use the bool to short-circuit and avoid doing strcmp every
          // iteration
          if (dyn_arr == false
              && strcmp (enabled_tests->items[i], "dyn_arr") == 0)
            dyn_arr = true;
          if (math == false && strcmp (enabled_tests->items[i], "math") == 0)
            math = true;
          if (arena == false && strcmp (enabled_tests->items[i], "arena") == 0)
            arena = true;
          if (input == false && strcmp (enabled_tests->items[i], "input") == 0)
            input = true;
          if (utility == false
              && strcmp (enabled_tests->items[i], "utility") == 0)
            utility = true;
          if (renderer == false
              && strcmp (enabled_tests->items[i], "renderer") == 0)
            renderer = true;
          if (threading == false
              && strcmp (enabled_tests->items[i], "threading") == 0)
            threading = true;
        }
    }
  // Blacklist
  else
    {
      dyn_arr = true;
      arena = math = input = utility = renderer = threading = true;

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
          if (strcmp (disabled_tests->items[i], "arena") == 0)
            arena = false;
          if (strcmp (disabled_tests->items[i], "input") == 0)
            input = false;
          if (strcmp (disabled_tests->items[i], "utility") == 0)
            utility = false;
          if (strcmp (disabled_tests->items[i], "renderer") == 0)
            renderer = false;
          if (strcmp (disabled_tests->items[i], "threading") == 0)
            threading = false;
        }
    }

  // Running the tests
  start_testing ();

  if (dyn_arr)
    dyn_arr_all_tests ();
  if (arena)
    arena_all_tests ();
  if (math)
    math_all_tests ();
  if (input)
    input_all_tests ();
  if (utility)
    utility_all_tests ();
  if (renderer)
    renderer_all_tests ();
  if (threading)
    threading_all_tests ();

  stop_testing ();
}
