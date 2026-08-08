#define ASTF_IMPLEMENTATION
// I'm using focus editor, it's build window does not display ANSI colors
// correctly, so I'll disable them
#define ASTF_NO_ANSI_COLORS
#include "astf.h"

#include "dyn_arr.test.h"

int
main (void)
{
  dyn_arr_all_tests ();

  return 0;
}
