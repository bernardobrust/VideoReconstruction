# Video Inspector

The inspector opens a video and displays codec data alongside its frames. It can draw motion vectors over the video and zoom into selected regions.

## Research goals

Planned analysis includes motion-vector use in computer vision, luma-based depth cues, feature detection, and comparisons between video and multi-view geometry. Background and references are collected in [`sources.md`](../../sources.md).

## Build and run

Build instructions are in the [project README](../README.md). Run the generated inspector executable with `-inspect` and a video path:

```bash
build/inspector_<mode>_<platform> -inspect path/to/video.mp4
```

The inspector reads `inspector.conf` from the current working directory. Its settings and defaults are documented in [`config_file.md`](../docs/config_file.md).

## Controls

- **Space:** Start or pause playback.
- **Left / Right:** Pause and step backward or forward one frame.
- **Down / Up:** Decrease or increase playback speed.
- **F:** Toggle fullscreen.
- **T:** Toggle the motion-vector overlay.
- **Ctrl + Left Mouse Button:** Drag to select and zoom into a region.
- **Ctrl + Right Mouse Button:** Undo the most recent zoom level.
- **Right Mouse Button:** Drag to pan the current zoom.

## Export

Export video data without opening the inspector window by passing the video and destination paths:

```bash
build/inspector_<mode>_<platform> -export path/to/video.mp4 path/to/output.dat
```

The output structure is documented in the [export format](../docs/export_format.md).
