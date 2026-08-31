### Ongoing:
- @fix Fix colloring (colors with high R are with high B)
- Rescale video to arbitrary resolution (will be used latter for drawing frames side-by-side)
- Windows platform layer

#### Per-file basis
- @decode.c check if dereferencing `vp` every time is worth it, or create temp vars and set them all latter
- @dyn_arr.c check if $2$ is a reasonable scale factor, or add a macro to change it
- @tests/main.c check if this strategy for argument handling is good

### General
- [x] Update this TODO list with a more concrete plan

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
- [ ] Windows platform layer
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
- [ ] Draw transparent rectangle on top of frame (and arrow)
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
- [ ] TODO ...

### Extra (inspector)
- [ ] Test coverage (like Like FFmpeg's FATE but a lot simpler)
- [ ] Paralelized build?
- [ ] Data analysis

### Documentation (both)
- [ ] Maybe explain the geometry of the renderer better?
