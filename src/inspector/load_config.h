#pragma once

#include "macros.h"

typedef struct
{
  bool auto_full_screen;
  u64 frame_cache_bytes;
  u64 exporter_output_buffer_bytes;
} InspectorConfig;

void inspector_config_load (InspectorConfig *config);
void inspector_config_validate (InspectorConfig *config);
