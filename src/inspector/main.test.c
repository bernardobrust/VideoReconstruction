#define ASTF_IMPLEMENTATION
#include "astf.h"

int main(void) {
    astf_start_test_suite("Math Suite");

    int result = 2 + 2;
    astf_assert_equal(4, result);
    astf_assert_true(result > 0);

    astf_retrieve_results();

    return 0;
}
