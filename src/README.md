# Build Instructions
This project only uses C, the build system is a separate C file [`nob.c`](nob.c) (check the structure section).

### Requirements:
- A C11 compatible compiler, tested mainly with `gcc` on GNU + Linux, `cl` (msvc, make sure you are using a 64-bit version) on windows.
- `curl` on both platforms to download the modified libraries when needed.
- `unzip` on GNU + Linux or `tar` on Windows to extract the library bundle.

The modified FFmpeg and dav1d libraries are downloaded from the project release the first time you build if they are missing. You can also fetch them explicitly with `./nob pull_libs`; on GNU + Linux, use `./nob pull-linux-libs`.

### Building:
The first time you compile you will have to generate the build tool, simply run (uses `gcc` by default):
`gcc nob.c -o nob` or `cl nob.c`

Now you can compile the project with:
`./nob(.exe) -target TARGET -platform PLATFORM -build_type BUILD_TYPE`

On GNU + Linux and Windows, the platform libraries are pulled if they are not found. You will be asked if you want to download them now; accept for the project to build.

The parameters are as follows:
- TARGET: inspector | reconstructor | tests (see below) | pull_libs
- PLATFORM: gnu_linux_x11 | gnu_linux_wayland | windows (windows does not exist for now)
- BUILD_TYPE: debug | release

For example, to build the inspector for GNU + Linux with X11:
`./nob -target inspector -platform gnu_linux_x11 -build_type release`

Both GNU + Linux back-ends speak their display protocols directly, so no X11 or Wayland development packages are required.

The generated executable will be in the form ""TARGET_BUILD_TYPE_PLATFORM(.exe)"" in the `build/` directory. Exemple: `build/inspector_release_gnu_linux_x11` or `build\inspector_debug_windows.exe`

### Running:
You can tun the inspector using the executable and passing a video path:
``build/inspector_...(.exe) video_path``

### Testing:
Testing can be performed with either a whitelist or a blacklist:
`./build/tests_...(.exe) -enable TESTS_TO_RUN` or `./build/tests_...(.exe) -disable TESTS_TO_EXCLUDE`

Not providing any option will run all of the tests.

List of tests:
- dyn_arr: dynamic array tests

# Structure
### Common code (shared across inspector and reconstructor)
This includes the following directories:
1. `platform`, with the platform layers for GNU + Linux (X11 and Wayland) and Windows (unimplemented)
2. `input`, input system
3. `renderer`, the source code for the software renderer
4. `ds`, the data structures used in the projects
5. `math`, the mathematical functions and objects used in the code
6. `lib`, third-party code used in the project, on windows the modified libraries are already avaliable
7. `test_files`, files used to test the correctness of the programs. For now this is empty as I didn't write any tests that directly require video data, but I recommend checking the sources for where to get some testing data.

The following are present in `lib/` (the `.h` files are in there):
- [nob](https://github.com/tsoding/nob.h), the build system (by TSoding)
- [flag](https://github.com/tsoding/flag.h), for parsing the build system arguments (by TSoding)
- [astf](https://github.com/bernardobrust/ASTF-V2), for automated testing (by me)
- "OS_NAME", the modified libraries ([FFmpeg](https://github.com/bernardobrust/FFmpeg/tree/dav1d-mv-integration) and [dav1d](https://github.com/bernardobrust/dav1d/tree/mv-export)) pre-compiled for your OS

### Inspector
The inspector is a tool to visualize compression data of the videos, such as color channels, chroma channels and inter prediction. See more [here](inspector/)

### Reconstructor (way latter)
The reconstructor is a tool to reconstruct 3d models from video data using computer vision. See more on the ideas file [here](../ideas.md)

# Profiling
In this project we use both `perf` and "Intel VTune Profiler" (check [sources](../sources.md)). Profiling data is not uploaded to GitHub as it's hardware dependent and can be extracted with ease.

Information relevat to performance can be found on the [TODO list](../todo.md)
