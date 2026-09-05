# Build Instructions
This project only uses C, the build system is a separate C file [`nob.c`](nob.c) (check the structure section).

### Requirements:
- A C11 compatible compiler, tested mainly with `gcc` on GNU + Linux, `cl` (msvc, make sure you are using a 64-bit version) on windows.
- FFmpeg development libraries: `avformat`, `avcodec`, `swscale`, and `avutil`. If running on Windows the environment variable `FFMPEG_DIR` must be set to the directory where the FFmpeg libraries are located.

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
- Gentoo:

**USE flags: dav1d (enabled by default)**
```bash
sudo emerge media-video/ffmpeg
```

If you are using a different distribution, please check the package manager for the FFmpeg development libraries.

On Windows the easiest way is with `win-get`, just run:
```powershell
winget install "FFmpeg (Shared)"
```

### Building:
The first time you compile you will have to generate the build tool, simply run (uses `gcc` by default):
`gcc nob.c -o nob` or `cl nob.c`

Now you can compile the project with:
`./nob(.exe) -target TARGET -platform PLATFORM -build_type BUILD_TYPE`

The parameters are as follows:
- TARGET: inspector | reconstructor | tests (see below)
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
6. `lib`, third-party code used in the project
7. `test_files`, files used to test the correctness of the programs. For now this is empty as I didn't write any tests that directly require video data, but I recommend checking the sources for where to get some testing data.

The following are present in `lib/` (the `.h` files are in there):
- [nob](https://github.com/tsoding/nob.h), the build system (by TSoding)
- [flag](https://github.com/tsoding/flag.h), for parsing the build system arguments (by TSoding)
- [astf](https://github.com/bernardobrust/ASTF-V2), for automated testing (by me)

### Inspector
The inspector is a tool to visualize compression data of the videos, such as color channels, chroma channels and inter prediction. See more [here](inspector/)

### Reconstructor
The reconstructor is a tool to reconstruct 3d models from video data using computer vision. See more [here](reconstructor/)

# Profiling
In this project we use both `perf` and "Intel VTune Profiler" (check [sources](../sources.md)). Profiling data is not uploaded to GitHub as it's hardware dependent and can be extracted with ease.

Information relevat to performance can be found on the [TODO list](../todo.md)