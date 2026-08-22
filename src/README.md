# Build Instructions
This project only uses C (no Make, CMake or SCons), with the external dependencies being included by the build system itself in header libraries.

### Requirements:
- A C11 compatible compiler (tested mainly with `gcc`).
- FFmpeg development libraries: `avformat`, `avcodec`, `swscale`, and `avutil`.

The libraries can be installed in GNU + Linux systems as follows (if I added anything wrong here please open an issue):
- Debian / Ubuntu:
```bash
sudo apt install libavformat-dev libavcodec-dev libswscale-dev libavutil-dev
```
- Fedora / RHEL / CentOS / Rocky Linux / AlmaLinux (FFmpeg packages are generally provided through additional repositories such as RPM Fusion):
```bash
sudo dnf install ffmpeg-devel
```
- Arch Linux (and adjacent like Atrix) / Manjaro / CachyOS:
```bash
sudo pacman -S ffmpeg
```
- openSUSE (Package names may vary with the FFmpeg version available in the configured repositories, I don't know the exact versions that will work as I don't use open SUSE nor anyone that does, but I'm using FFmpeg version 9 and from what I read the repos use this version as well):
```bash
sudo zypper install ffmpeg-9-libavformat-devel ffmpeg-9-libavcodec-devel ffmpeg-9-libswscale-devel ffmpeg-9-libavutil-devel
```
- Alpine Linux:
```bash
sudo apk add ffmpeg-dev
```
- Gentoo (The best distro):

**USE flags: dav1d (enabled by default)**
```bash
sudo emerge media-video/ffmpeg
```

### Building:
The first time you compile you will have to generate the build tool, simply run (uses `gcc` by default):
`gcc nob.c -o nob`

Now you can compile the project with:
`./nob -target TARGET -platform PLATFORM -build_type BUILD_TYPE`

The parameters are as follows:
- TARGET: inspector | reconstructor (see below)
- PLATFORM: gnu_linux_x11 | gnu_linux_wayland | windows (windows does not exist for now)
- BUILD_TYPE: debug | release | test

For example, to build the inspector for GNU + Linux with X11:
`./nob -target inspector -platform gnu_linux_x11 -build_type release`

Both GNU + Linux back-ends speak their display protocols directly, so no X11 or Wayland development packages are required.

### Running:
So far there is no usage instructions here, you just run the executable generated under `build/`. It will be in the format "target_buildtype_platform".

### Testing:
Testing can be performed with either a whitelist or a blacklist:
`./build/TEST_EXECUTABLE -enable TESTS_TO_RUN` or `./build/TEST_EXECUTABLE -disable TESTS_TO_EXCLUDE`

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
6. `lib`, third-party code used in the project
7. `test_files`, files used to test the correctness of the programs

For now the following are present in `lib/` (we still don't have the video processing integration):
- [nob](https://github.com/tsoding/nob.h), the build system
- [flag](https://github.com/tsoding/flag.h), for parsing the build system arguments
- [astf](https://github.com/bernardobrust/ASTF-V2), for automated testing

### Inspector
The inspector is a tool to visualize compression data of the videos, such as color channels, chroma channels and inter prediction. See more [here](inspector/)

### Reconstructor
The reconstructor is a tool to reconstruct 3d models from video data using computer vision. See more [here](reconstructor/)
