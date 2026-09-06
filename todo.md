### Notes:
As it turns out libdav1d does not export motion vectors in FFmpeg the same way HEVC does. So I'll basically have to hack the library and export them myself.

#### Per-file basis
- decode.c : @OPTIMIZATION check if dereferencing `vp` every time is worth it, or create temp vars and set them all latter
- decode.c : @IMPROVEMENT check if the "RGBA" vs "BGRA" problem is platform-specific, more specifically, if it has to do with "XRGB8888" on wayland, or with endianess.
- dyn_arr.c : @OPTIMIZATION check if $2$ is a reasonable scale factor, or add a macro to change it
- tests/main.c : @IMPROVEMENT check if this strategy for argument handling is good
- platform.h : @FEATURE add a function to get monitor resolution (or maybe a function to get the monitor size and position)
- renderer.c : @FIX, @OPTIMIZATION the circle looks weird, it likelly has to do with the loops tho i'm not certain

### Performance
- @PERFORMANCE For now the main bottleneck is the decode -> send image buffer to display loop, which is not paralelized. We can improve this latter by using a queue of decoded frames and a separate thread for the renderer
- @PERFORMANCE Keep in mind that we NEED the entire get video data -> render to be fast enough to support live video streams, so we need it to support arbitrary frame and bit rates
- @PERFORMANCE The transparent shape drawing is a massive performance bottleneck, we need to optimize it. Maybe we can use a different approach for this, like using a separate buffer for the transparent shapes and then blending it with the video frame

### Detour (top priority)
- [ ] Hack libdav1d to export motion vectors in a way that we can use them in the inspector. This will require some research and experimentation, but it should be doable.
- [ ] Statically link the modified version into this repo
- [ ] Change the decoder to use the modified version of libdav1d and use the motion vectors to render them on top of the video frame

### General
- [x] Update this TODO list with a more concrete plan
- [x] Add non-online sources (like books) to [sources](sources.md)

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
- [ ] Get testing data (I have some mock videos but we'll need more latter)
- [ ] Translation units of `shared` and individual projects via inclusion of `.c` files

### Platform Layer (shared)
We won't need audio for this project

- [x] Raw X11 platform Layer
- [x] Raw Wayland platform Layer
- [x] Windows platform layer
- [ ] Floating windows on window managers
- [ ] Full screen support
- [ ] Mac platform layer? (Sounds like a pain for little gain)

### Math Library (shared)
- [ ] Basic utilities (clamp, 2D lerp, etc.)
- [ ] ...

### Data Structures (shared)
- [x] Dynamic array
- [x] Dynamic array tests
- [x] Move tests to separate testing file for shared code
- [ ] ...

### Renderer (CPU / software renderer) (shared)
Should we test the rendering primitives?

- [x] Open window + set defaults
- [x] Draw rectangle
- [x] Draw triangle
- [x] Draw circle
- [x] Draw rotated rectangle (with angle and with orientation)
- [x] Draw arrow
- [x] Framerate stability on video framerate
- [ ] UI fixed framerate
- [ ] Draw text
- [x] Draw frame of a video
- [x] Draw transparent rectangle on top of frame (and arrow)
- [ ] Scale video resolution to block dimensions
- [ ] Paralelize the renderer

### Systems (inspector)
- [x] Event system (key presses and UI buttons)
- [x] Input system (input struct)
- [ ] Add mouse to the input system

### Core (inspector)
- [x] Take the video to use as a parameter of the inspector binary
- [ ] Scale/downscale video to a given WxH (ongoing)
- [ ] App state PLEX (a.k.a fat struct)
- [ ] Render motion vectors per block
- [ ] Compression data...
- [ ] Vector scale represented as a color
- [ ] Next and previous frame
- [ ] Jump to specific frame/time
- [ ] Video timeline
- [ ] Side-by-side frame comparison (basic)
- [ ] Side-by-side frame comparison (mapping block translations)

### Extra (inspector)
- [ ] Test coverage (like Like FFmpeg's FATE but a lot simpler)
- [ ] Paralelized build?
- [ ] Data analysis

### Documentation (both)
- [ ] Maybe explain the geometry of the renderer better?
