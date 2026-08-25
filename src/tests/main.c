#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ASTF_IMPLEMENTATION
#define ASTF_STRIP_PREFIX
// I'm using focus editor, it's build window does not display ANSI colors
// correctly, so the bellow flag may be usefull
// #define ASTF_NO_ANSI_COLORS
#include "astf.h"

#define FLAG_IMPLEMENTATION
#include "flag.h"

#include "dyn_arr.test.h"

int
main (int argc, char **argv)
{
  // CLI parsing
  // enabled tests is a whitelist
  // disabled tests is a blacklist
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
  bool dyn_arr; // ...

  // Whitelist
  if (enabled_tests->count > 0)
    {
      dyn_arr = false;

      for (long unsigned int i = 0; i < enabled_tests->count; ++i)
        {
          // We use the bool to short-circuit and avoid doing strcmp every
          // iteration
          if (dyn_arr == false
              && strcmp (enabled_tests->items[i], "dyn_arr") == 0)
            dyn_arr = true;
        }
    }
  // Blacklist
  else
    {
      dyn_arr = true;

      for (long unsigned int i = 0; i < disabled_tests->count; ++i)
        {
          if (dyn_arr == true
              && strcmp (disabled_tests->items[i], "dyn_arr") == 0)
            dyn_arr = false;
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

  stop_testing ();
}
