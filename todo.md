## Notes
### Per-file basis
- decode.c : @OPTIMIZATION check whether dereferencing `vp` repeatedly is worth replacing with temporary variables (benchmark)
- decode.c : @IMPROVEMENT check whether the "RGBA" vs. "BGRA" issue is platform-specific, possibly related to "XRGB8888" on Wayland or to endianness.

## Checklists
### Modified libraries
- [ ] Hack libdav1d (and FFmpeg as a consequence) to export compression data:
	- [x] MVs
	- [ ] Transforms
	- [ ] Intra prediction
	- [ ] ...
- [ ] Build a sample program to check that, likely a test file
- [ ] Generate the patches for the changes made and add them to a `lib/patches` dir in order to keep modifications explicit
- [x] Change the decoder to use the modified versions of the libs

### General
- [x] Update this TODO list with a more concrete plan
- [x] Add non-online sources (like books) to [sources](sources.md)
- [x] Check if the macros for data types broke builds for other platforms. Also check if any doc file was changed.

### Research
We'll add a lot more stuff here as the project advances

- [x] Write a draft for the paper
- [x] Update/Improve the paper
- [ ] Define a file format for the data
- [ ] Add the paper (after it gets approved)

### Build System (shared)
- [x] Setup the build system (nob + flag)
- [x] Setup debug build
- [x] Setup release build
- [x] Setup test build
- [x] FFmpeg integration
- [x] Modified library integration for Windows
- [x] Modified library integration for GNU + Linux (I'll likely do this from a native machine to make sure every step works)
- [ ] Check the compilation flags of the libraries, optimize as much as possible
- [ ] Get testing data (I have some mock videos, but we'll need more later)
- [ ] Translation units of `shared` and individual projects via inclusion of `.c` files

### Performance
- [x] Separate threads for rendering and video decoding
- [x] Cache the decoded frames
- [ ] Cache the UI
- [x] Parallelized renderer
- [ ] Lock-free paralelism (if possible ofc)

### Platform Layer (shared)
We won't need audio for this project

- [x] Raw X11 platform Layer
- [x] Raw Wayland platform Layer
- [x] Windows platform layer
- [x] Get screen resolution function
- [x] Full screen support
- [x] OS independent threading
- [ ] Mac platform layer? (Sounds like a pain for little gain)

### Math Library (shared)
- [x] Basic utilities (clamp, round, fast operations, etc.)
- [x] Tests for basic utilities
- [ ] ...

### Data Structures (shared)
- [x] Dynamic array
- [x] Dynamic array tests
- [x] Move tests to separate testing file for shared code
- [x] Arena + tests
- [ ] Add others later as needed ...

### Renderer (CPU / software renderer) (shared)
Should we test the rendering primitives?

- [x] Open window + set defaults
- [x] Draw rectangle
- [x] Draw triangle
- [x] Draw circle
- [x] Draw rotated rectangle (with angle and with orientation)
- [x] Draw arrow
- [x] Framerate stability on video framerate
- [ ] UI fixed framerate (right now we synchronize to the video's frame rate)
- [ ] Draw text (baked font into a `.h` file)
- [x] Draw frame of a video
- [x] Draw transparent rectangle on top of frame (and arrow)
- [ ] Scale video resolution to pane dimensions
- [ ] UI
- [ ] Make UI fit well in the window (responsive measures)

### Systems (inspector)
- [x] Event system (key presses and UI buttons)
- [x] Input system (input struct)
- [x] Add mouse to the input system

### Core (inspector)
- [x] Take the video to use as a parameter of the inspector binary
- [x] Pause
- [x] Slow down and speed up
- [x] Zoom-in and Zoom-out
- [ ] Scale/downscale video to a given WxH
- [x] Render motion vectors on top of the frame
- [ ] Render motion vectors per block (a.k.a. highlight the block)
- [x] Modify vector color based on magnitude (better than scaling the arrow)
- [ ] More compression data...
- [ ] Next and previous frame
- [ ] Jump to specific frame/time
- [ ] Video timeline
- [ ] Side-by-side frame comparison (basic)
- [ ] Side-by-side frame comparison (mapping block translations)
- [ ] Config file with preferences (such as video cache size)
- [ ] Export data to file (plaintext)
- [ ] Export and inspect at the same time

### Extra (inspector)
- [ ] Test coverage (like Like FFmpeg's FATE but a lot simpler)
- [ ] Parallelized build (?)
- [ ] Data analysis
