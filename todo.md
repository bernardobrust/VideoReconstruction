## Notes
### Per-file basis
- main.c : @FIX the problems with the UI framerate are already showing when highlighing the zoom area and pressing commands
- decode.c : @OPTIMIZATION check if dereferencing `vp` every time is worth it, or create temp vars and set them all latter
- decode.c : @IMPROVEMENT check if the "RGBA" vs "BGRA" problem is platform-specific, more specifically, if it has to do with "XRGB8888" on wayland, or with endianess.
- decode.c : @IMPROVEMENT calculate the constants based on the display's size + all contants in general
- dyn_arr.c : @OPTIMIZATION check if $2$ is a reasonable scale factor, or add a macro to change it
- renderer.c : @FIX, @OPTIMIZATION the circle looks weird, it likelly has to do with the loops tho i'm not certain
- basic.c : @IMPROVEMENT `_Generic` macro for `clamp` functions?
- nob.c : @FIX for some reason when pulling libraries for the first time it may successfully pull but not compile, requiering nob to be ran a second time

## Checklists
### Modified libraries
- [ ] Hack libdav1d (and FFmpeg as a consequence) to export compression data:
	- [x] MVs
	- [ ] Transforms
	- [ ] Intra prediction
	- [ ] ...
- [ ] Build a sample program to check that, likelly a test file
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
- [ ] Update [ideas](ideas.md) as the project advances
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
- [ ] Auto updates?
- [ ] Get testing data (I have some mock videos but we'll need more latter)
- [ ] Translation units of `shared` and individual projects via inclusion of `.c` files

### Performance
- [ ] Separate threads for rendering and video decoding
- [ ] Cache the decoded frames
- [ ] Cache the UI
- [ ] Paralelized renderer

### Platform Layer (shared)
We won't need audio for this project

- [x] Raw X11 platform Layer
- [x] Raw Wayland platform Layer
- [x] Windows platform layer
- [x] Get screen resolution function
- [x] Full screen support
- [ ] OS independent threading
- [ ] Mac platform layer? (Sounds like a pain for little gain)

### Math Library (shared)
- [x] Basic utilities (clamp, round, fast operations, etc.)
- [x] Tests for basic utilities
- [ ] ...

### Data Structures (shared)
- [x] Dynamic array
- [x] Dynamic array tests
- [x] Move tests to separate testing file for shared code
- [ ] Add others latter as needed ...

### Renderer (CPU / software renderer) (shared)
Should we test the rendering primitives?

- [x] Open window + set defaults
- [x] Draw rectangle
- [x] Draw triangle
- [x] Draw circle
- [x] Draw rotated rectangle (with angle and with orientation)
- [x] Draw arrow
- [x] Framerate stability on video framerate
- [ ] UI fixed framerate (right now we syncronize to the video's frame rate)
- [ ] Draw text
- [x] Draw frame of a video
- [x] Draw transparent rectangle on top of frame (and arrow)
- [ ] Scale video resolution to pane dimensions
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
- [ ] Render motion vectors per block (a.k.a hightlight the block)
- [x] Modify vector color based on magnitude (better than scaling the arrow)
- [ ] More compression data...
- [ ] Next and previous frame
- [ ] Jump to specific frame/time
- [ ] Video timeline
- [ ] Side-by-side frame comparison (basic)
- [ ] Side-by-side frame comparison (mapping block translations)
- [ ] Config file with preferences (such as video cache size)

### Extra (inspector)
- [ ] Test coverage (like Like FFmpeg's FATE but a lot simpler)
- [ ] Paralelized build (?)
- [ ] Data analysis
