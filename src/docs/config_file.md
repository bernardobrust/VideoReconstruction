# Config file

## Config format and data
The configuration file `inspector.conf` contains user-defined values used to run the video inspector. The inspector looks for it in the current working directory.

If the file is missing or a setting is omitted, its default is used. Invalid entries are ignored. Boolean values accept `true` or `false`; capacity settings are positive integer megabytes. Settings may use either a space or `:=` between the name and value. `exporter_output_buffer` applies to each parallel output worker.

If `frame_cache_capacity` exceeds currently available RAM, the inspector prints a warning and uses the default cache size.

The config file is defined as follows:
```
auto_full_screen := bool
frame_cache_capacity := megabytes (integer)
exporter_output_buffer := megabytes (integer)

Example (these are the default values):
--------------------------------------
auto_full_screen true
frame_cache_capacity 256
exporter_output_buffer 8
--------------------------------------
```