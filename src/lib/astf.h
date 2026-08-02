// * A simple test framework V2 (astf2)

/*
 * astf.h - A simple C testing framework.
 * Copyright (C) 2026  Bernardo Brust (the adm)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Colors
#define astf_output_fail "\x1b[31m"
#define astf_output_pass "\x1b[32m"
#define astf_output_warn "\x1b[33m"
#define astf_output_info "\x1b[36m"
#define astf_output_normal "\x1b[0m"

// Context
typedef struct
{
  unsigned passed;
  unsigned failed;
  const char *test_suite_name;
} astf_ctx;

extern astf_ctx _astf_global_ctx;

void astf_start_test_suite (const char *name);
void astf_retrieve_results ();

void _astf_ae_int (int exp, int act, const char *file, int line);
void _astf_ae_long (long exp, long act, const char *file, int line);
void _astf_ae_unsigned (unsigned exp, unsigned act, const char *file,
                        int line);
void _astf_ae_ulong (unsigned long exp, unsigned long act, const char *file,
                     int line);
void _astf_ae_unsigned_short (unsigned short exp, unsigned short act,
                              const char *file, int line);
void _astf_ae_str (const char *exp, const char *act, const char *file,
                   int line);
void _astf_ae_ptr (const void *exp, const void *act, const char *file,
                   int line);
void _astf_ae_float (float exp, float act, float eps, const char *file,
                     int line);
void _astf_ae_double (double exp, double act, double eps, const char *file,
                      int line);
void _astf_assert_not_null (const void *exp, const char *file, int line);

#define astf_assert_equal(expected, actual)                                   \
  _Generic ((expected),                                                       \
      int: _astf_ae_int,                                                      \
      unsigned int: _astf_ae_unsigned,                                        \
      unsigned short: _astf_ae_unsigned_short,                                \
      long: _astf_ae_long,                                                    \
      unsigned long: _astf_ae_ulong,                                          \
      char *: _astf_ae_str,                                                   \
      const char *: _astf_ae_str,                                             \
      default: _astf_ae_ptr) (expected, actual, __FILE__, __LINE__)

#define astf_assert_cond(expected_bool, condition)                            \
  _astf_ae_bool (expected_bool, (bool)(condition), __FILE__, __LINE__)

#define astf_assert_true(cond) astf_assert_cond (true, cond)
#define astf_assert_false(cond) astf_assert_cond (false, cond)

#define astf_assert_approx(expected, actual, epsilon)                         \
  _Generic ((expected), float: _astf_ae_float, double: _astf_ae_double) (     \
      expected, actual, epsilon, __FILE__, __LINE__)

#define astf_assert_null(pointer) astf_assert_equal (NULL, pointer)
#define astf_assert_not_null(pointer)                                         \
  _astf_assert_not_null (pointer, __FILE__, __LINE__)

#define astf_assert_range(val, min, max)                                      \
  _Generic ((val),                                                            \
      int: _astf_ae_range_int,                                                \
      unsigned int: _astf_ae_range_uint,                                      \
      unsigned short: _astf_ae_range_ushort,                                  \
      long: _astf_ae_range_long,                                              \
      unsigned long: _astf_ae_range_ulong,                                    \
      float: _astf_ae_range_float,                                            \
      double: _astf_ae_range_double) (val, min, max, __FILE__, __LINE__)

#ifdef ASTF_IMPLEMENTATION
astf_ctx _astf_global_ctx = { 0 };

void
astf_start_test_suite (const char *name)
{
  _astf_global_ctx.test_suite_name = name;
  _astf_global_ctx.passed = 0;
  _astf_global_ctx.failed = 0;
  printf (astf_output_info "\n--- Starting Suite: %s ---\n", name);
}

// Assert
// -----------------------------------------------------------------------------
void
_astf_ae_int (int exp, int act, const char *file, int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %d but got %d\n\n",
              file, line, exp, act);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_unsigned (unsigned exp, unsigned act, const char *file, int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %u but got %u\n\n",
              file, line, exp, act);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_unsigned_short (unsigned short exp, unsigned short act,
                         const char *file, int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %hu but got %hu\n\n",
              file, line, exp, act);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_long (long exp, long act, const char *file, int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %ld but got %ld\n\n",
              file, line, exp, act);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_ulong (unsigned long exp, unsigned long act, const char *file,
                int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %lu but got %lu\n\n",
              file, line, exp, act);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_str (const char *exp, const char *act, const char *file, int line)
{
  if (exp && act && strcmp (exp, act) == 0)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected \"%s\" but got \"%s\"\n\n",
              file, line, exp ? exp : "NULL", act ? act : "NULL");
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_bool (bool exp, bool act, const char *file, int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %s but got %s\n\n",
              file, line, exp ? "true" : "false", act ? "true" : "false");
      _astf_global_ctx.failed++;
    }
}

// Floating point types (using Epsilon)
void
_astf_ae_double (double exp, double act, double eps, const char *file,
                 int line)
{
  if (fabs (exp - act) < eps)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %f but got %f (Epsilon: %f)\n\n",
              file, line, exp, act, eps);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_float (float exp, float act, float eps, const char *file, int line)
{
  if (fabsf (exp - act) < eps)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected %f but got %f (Epsilon: %f)\n\n",
              file, line, (double)exp, (double)act, (double)eps);
      _astf_global_ctx.failed++;
    }
}

// Pointers
void
_astf_ae_ptr (const void *exp, const void *act, const char *file, int line)
{
  if (exp == act)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s: %d\n" astf_output_warn
                               "Expected pointer %p but got %p\n\n",
              file, line, exp, act);
      _astf_global_ctx.failed++;
    }
}

void
_astf_assert_not_null (const void *ptr, const char *file, int line)
{
  if (ptr != NULL)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail
              "[FAILED] %s: %d\n" astf_output_warn
              "Expected pointer to not be null, but it is\n\n",
              file, line);
      _astf_global_ctx.failed++;
    }
}

// Ranges
void
_astf_ae_range_int (int v, int min, int max, const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %d out of range [%d, %d]\n\n",
              f, l, v, min, max);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_range_uint (unsigned int v, unsigned int min, unsigned int max,
                     const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %u out of range [%u, %u]\n\n",
              f, l, v, min, max);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_range_long (long v, long min, long max, const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %ld out of range [%ld, %ld]\n\n",
              f, l, v, min, max);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_range_ulong (unsigned long v, unsigned long min, unsigned long max,
                      const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %lu out of range [%lu, %lu]\n\n",
              f, l, v, min, max);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_range_ushort (unsigned short v, unsigned short min,
                       unsigned short max, const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %hu out of range [%hu, %hu]\n\n",
              f, l, v, min, max);
      _astf_global_ctx.failed++;
    }
}

// Floating Point Range Implementations
void
_astf_ae_range_float (float v, float min, float max, const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %f out of range [%f, %f]\n\n",
              f, l, (double)v, (double)min, (double)max);
      _astf_global_ctx.failed++;
    }
}

void
_astf_ae_range_double (double v, double min, double max, const char *f, int l)
{
  if (v >= min && v <= max)
    _astf_global_ctx.passed++;
  else
    {
      printf (astf_output_fail "[FAILED] %s:%d\n" astf_output_warn
                               "Value %f out of range [%f, %f]\n\n",
              f, l, v, min, max);
      _astf_global_ctx.failed++;
    }
}

// -----------------------------------------------------------------------------

// Get results
void
astf_retrieve_results (void)
{
  printf (astf_output_info "--- Results for %s ---\n",
          _astf_global_ctx.test_suite_name);
  printf (astf_output_pass "Passed: %d\n", _astf_global_ctx.passed);
  printf (astf_output_fail "Failed: %d\n" astf_output_normal,
          _astf_global_ctx.failed);
}
#endif

#ifndef ASTF_STRIP_PREFIX_GUARD_
#define ASTF_STRIP_PREFIX_GUARD_

#ifdef ASTF_STRIP_PREFIX

#define start_test_suite astf_start_test_suite
#define retrieve_results astf_retrieve_results

#define assert_equal astf_assert_equal
#define assert_approx astf_assert_approx
#define assert_cond astf_assert_cond
#define assert_true astf_assert_true
#define assert_false astf_assert_false
#define assert_null astf_assert_null
#define assert_not_null astf_assert_not_null
#define assert_range astf_assert_range

#endif

#endif
