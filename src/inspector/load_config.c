#include "load_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"

enum
{
  CONFIG_MEGABYTE = MEGABYTE (1)
};

local const bool DEFAULT_FULLSCREEN = true;
local const u64 DEFAULT_FRAME_CACHE_BYTES = 256ULL * CONFIG_MEGABYTE;
local const u64 DEFAULT_EXPORTER_OUTPUT_BUFFER_BYTES = 8ULL * CONFIG_MEGABYTE;
local const char CONFIG_FILE_PATH[] = "inspector.conf";

local inline bool
is_space (char c)
{
  return c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f';
}

local inline bool
parse_boolean (const char *value, u64 length, bool *result)
{
  assert (value != NULL && result != NULL);

  if ((length == 4 && memcmp (value, "true", 4) == 0)
      || (length == 1 && value[0] == '1'))
    {
      *result = true;
      return true;
    }

  if ((length == 5 && memcmp (value, "false", 5) == 0)
      || (length == 1 && value[0] == '0'))
    {
      *result = false;
      return true;
    }

  return false;
}

local bool
parse_megabytes (const char *value, u64 length, u64 *result)
{
  u64 megabytes, i;

  assert (value != NULL && result != NULL);
  if (length == 0)
    return false;

  megabytes = 0;
  for (i = 0; i < length; ++i)
    {
      u64 digit;

      if (value[i] < '0' || value[i] > '9')
        return false;

      digit = (u64)(value[i] - '0');
      // Check before multiplying so malformed or huge values cannot wrap
      if (megabytes > (UINT64_MAX - digit) / 10)
        return false;
      megabytes = megabytes * 10 + digit;
    }

  if (megabytes == 0 || megabytes > UINT64_MAX / CONFIG_MEGABYTE)
    return false;

  *result = megabytes * CONFIG_MEGABYTE;

  return true;
}

local bool
token_equals (const char *token, u64 token_length, const char *expected)
{
  u64 expected_length = strlen (expected);

  return token_length == expected_length
         && memcmp (token, expected, expected_length) == 0;
}

local void
warn_config_line (u64 line_number, const char *message)
{
  fprintf (stderr, "Ignoring inspector.conf line %llu: %s\n",
           (unsigned long long)line_number, message);
}

local void
parse_config_line (const char *line, const char *line_end, u64 line_number,
                   InspectorConfig *config)
{
  const char *key_start, *key_end, *value_start, *value_end;
  bool bool_value;
  u64 number_value;

  // Each line is a bounded slice of the mapping and is not null-terminated
  while (line < line_end && is_space (*line))
    ++line;
  if (line == line_end || *line == '#'
      || (line_end - line >= 2 && line[0] == '/' && line[1] == '/'))
    return;

  key_start = line;

  // Stop at whitespace or an optional key/value separator
  while (line < line_end && !is_space (*line) && *line != ':' && *line != '=')
    ++line;
  key_end = line;

  while (line < line_end && is_space (*line))
    ++line;

  // Either ':=' or whitespace
  if (line < line_end && (*line == ':' || *line == '='))
    {
      if (*line == ':')
        {
          ++line;
          if (line == line_end || *line != '=')
            {
              warn_config_line (line_number, "expected ':=' before the value");
              return;
            }
        }

      ++line;
      while (line < line_end && is_space (*line))
        ++line;
    }

  value_start = line;

  // The value ends at whitespace, anything after it must be a comment
  while (line < line_end && !is_space (*line))
    ++line;

  value_end = line;
  while (line < line_end && is_space (*line))
    ++line;

  if (line < line_end && *line != '#'
      && !(line_end - line >= 2 && line[0] == '/' && line[1] == '/'))
    {
      warn_config_line (line_number, "unexpected text after the value");
      return;
    }

  if (value_start == value_end)
    {
      warn_config_line (line_number, "missing value");
      return;
    }

  if (token_equals (key_start, (u64)(key_end - key_start), "auto_full_screen"))
    {
      if (parse_boolean (value_start, (u64)(value_end - value_start),
                         &bool_value))
        config->auto_full_screen = bool_value;
      else
        warn_config_line (line_number, "expected true or false");
    }
  else if (token_equals (key_start, (u64)(key_end - key_start),
                         "frame_cache_capacity"))
    {
      if (parse_megabytes (value_start, (u64)(value_end - value_start),
                           &number_value))
        config->frame_cache_bytes = number_value;
      else
        warn_config_line (line_number,
                          "frame_cache_capacity must be a positive MB value");
    }
  else if (token_equals (key_start, (u64)(key_end - key_start),
                         "exporter_output_buffer"))
    {
      if (parse_megabytes (value_start, (u64)(value_end - value_start),
                           &number_value))
        config->exporter_output_buffer_bytes = number_value;
      else
        warn_config_line (
            line_number, "exporter_output_buffer must be a positive MB value");
    }
  else
    warn_config_line (line_number, "unknown setting");
}

void
inspector_config_load (InspectorConfig *config)
{
  PlatformMappedFile mapped_file = { .native_file = -1 };
  const char *cursor, *file_end, *line_end;
  u64 line_number;

  assert (config != NULL);

  config->auto_full_screen = DEFAULT_FULLSCREEN;
  config->frame_cache_bytes = DEFAULT_FRAME_CACHE_BYTES;
  config->exporter_output_buffer_bytes = DEFAULT_EXPORTER_OUTPUT_BUFFER_BYTES;

  if (!platform_mapped_file_open_readonly (&mapped_file, CONFIG_FILE_PATH))
    {
      s32 file_status = platform_file_exists ((byte *)CONFIG_FILE_PATH);

      if (file_status != 1)
        fprintf (stderr,
                 "Could not read inspector.conf; using default settings\n");
      return;
    }

  if (mapped_file.size > 0)
    {
      // Scan the mapped bytes line by line without copying or allocating
      cursor = mapped_file.data;
      file_end = cursor + mapped_file.size;
      line_number = 1;

      while (cursor < file_end)
        {
          line_end = cursor;
          while (line_end < file_end && *line_end != '\n')
            ++line_end;

          parse_config_line (cursor, line_end, line_number, config);
          cursor = line_end < file_end ? line_end + 1 : file_end;
          ++line_number;
        }
    }

  platform_mapped_file_close (&mapped_file);
}

void
inspector_config_validate (InspectorConfig *config)
{
  u64 available_memory;

  assert (config != NULL);

  available_memory = platform_get_available_memory ();
  if (available_memory > 0 && config->frame_cache_bytes > available_memory)
    {
      fprintf (
          stderr,
          "Frame cache setting (%llu MB) exceeds available RAM (%llu MB); "
          "using the default (%llu MB)\n",
          (u64)(config->frame_cache_bytes / CONFIG_MEGABYTE),
          (u64)(available_memory / CONFIG_MEGABYTE),
          (u64)(DEFAULT_FRAME_CACHE_BYTES / CONFIG_MEGABYTE));
      config->frame_cache_bytes = DEFAULT_FRAME_CACHE_BYTES;
    }
}
