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
- [ ] FFmpeg integration (`libavformat`, `libavcodec`, `libavutil`)
- [ ] Get testing data

### Platform Layer (shared)
We won't need audio for this project

- [x] Raw X11 platform Layer
- [x] Raw Wayland platform Layer
- [ ] Windows platform layer
- [ ] Mac platform layer (?)

### Math Library (shared)
- [ ] Basic utilities (clamp, 2D lerp, etc.)
- [ ] ...

### Data Structures (shared)
- [x] Dynamic array
- [ ] ...

### Systems (inspector)
- [ ] Event system (key presses and UI buttons)
- [ ] Input system (input struct)

### Renderer (CPU / software renderer) (inspector)
- [ ] Open window + set defaults
- [ ] Draw rectangle
- [ ] Framerate cap (30 should be good to go as a default)
- [ ] Draw triangle
- [ ] Draw circle
- [ ] Draw arrow
- [ ] Draw text
- [ ] Draw frame of a video
- [ ] Draw transparent rectangle on top of frame
- [ ] Scale video resolution to block dimensions
- [ ] TODO ...

### Core (inspector)
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
- [ ] Test coverage
- [ ] Data analysis
