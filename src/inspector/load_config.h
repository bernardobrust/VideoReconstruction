#pragma once

#include "macros.h"

// See ../docs/config_file.md for the supported settings and their syntax

typedef struct
{
  bool auto_full_screen; // Start the inspector in fullscreen mode
  u64 frame_cache_bytes, exporter_output_buffer_bytes; // Memory limits in bytes
} InspectorConfig;

void inspector_config_load (InspectorConfig *config);
void inspector_config_validate (InspectorConfig *config);
