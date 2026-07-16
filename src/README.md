# Build Instructions
This project is built entirelly using C, with the external dependencies being inluded by the build system itself in header libraries.

**Requirements**:
- A C11 compatible compiler, that's it.

**Building**:
The first time you compile you will have to generate the build tool, simply run:
`gcc nob.c -o nob`

Now you can compile the project with:
`./nob -target TARGET -platform PLATFORM -build_type BUILD_TYPE`

The parameters are as follows:
- TARGET: inspector | reconstructor (see below)
- PLATFORM: gnu_linux_x11 | gnu_linux_wayland | windows
- BUILD_TYPE: debug | release | test

So a release build of the inspector on GNU + Linux running wayland is built running:
`./nob -target inspector -platform gnu_linux_wayland -build_type release`

> NOTE: for now only the wayland layer is done

# Structure
## Common code
This includes the following directories:
1. `platform`, with the platform layers for GNU + Linux (X11 and Wayland) and Windows
2. `ds`, the data structures used in the projects
3. `math`, the mathematical functions and onjects used in the code
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
