# Video Reconstruction
This is a research repository aimed at studiying the possibility of doing 3D reconstructions using computer vision along with AV1's video codec compression data (such as motion vectors). An overview can be found [here](ideas.md). A proper paper is under way.

For the source code check [source code](src/), for the todo list check [todo](todo.md), for the information sources [sources](sources.md).

## Note on AI Generated Code
For transparency, the following code in this project was generated using AI assistance:

- **Google Antigravity**: Generated the MIT-SHM shared memory implementation for rendering in the X11 platform layer ([`src/platform/platform_gnu_linux_x11.c`](src/platform/platform_gnu_linux_x11.c)).
  - **Commit**: `05bca471150adc089746bb843786238450589dbd` "Added shared memory to the X11 layer"

I opted for AI assistance in this case, as documentation for X11 without Xlib or Xcb is awful, and I didn't want to spend lots of time hunting for old and obscure sources on the internet for this.