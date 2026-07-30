# Build Instructions
This project only uses C (no Make, CMake or SCons), with the external dependencies being included by the build system itself in header libraries.

**Requirements**:
- A C11 compatible compiler, that's it (tested mainly with `gcc`).

**Building**:
The first time you compile you will have to generate the build tool, simply run (uses `gcc` by default):
`gcc nob.c -o nob`

Now you can compile the project with:
`./nob -target TARGET -platform PLATFORM -build_type BUILD_TYPE`

The parameters are as follows:
- TARGET: inspector | reconstructor (see below)
- PLATFORM: gnu_linux_x11 | gnu_linux_wayland | windows
- BUILD_TYPE: debug | release | test

For example, to build the inspector for GNU + Linux with X11:
`./nob -target inspector -platform gnu_linux_x11 -build_type release`

Both GNU + Linux back-ends speak their display protocols directly, so no X11 or Wayland development packages are required.

# Structure
## Common code
This includes the following directories:
1. `platform`, with the platform layers for GNU + Linux (X11 and Wayland) and Windows (unimplemented)
2. `ds`, the data structures used in the projects
3. `math`, the mathematical functions and objects used in the code
4. `lib`, third-party code used in the project
5. `test_files`, files used to test the correctness of the programs

For now the following are present in `lib/` (we still don't have the video processing integration):
- [nob](https://github.com/tsoding/nob.h), the build system
- [flag](https://github.com/tsoding/flag.h), for parsing the build system arguments
- [astf](https://github.com/bernardobrust/ASTF-V2), for automated testing

## Inspector
The inspector is a tool to visualize compression data of the videos, such as color channels, chroma channels and inter prediction.

## Reconstructor
The reconstructor is a tool to reconstruct 3d models from video data using computer vision.
