# Video Reconstruction
This is a research repository aimed at studying the possibility of doing 3D reconstructions using computer vision along with AV1's video codec compression data (such as motion vectors). A proper paper is under way.

For the source code check [source code](src/), for the TODO list check [todo](todo.md), for the sources (articles, videos, tools, books, etc.) [sources](sources.md).

## Modified Libraries
In order to export compression data from libdav1d and FFmpeg I'm building modified versions of them at [libdav1d](https://github.com/bernardobrust/dav1d/tree/mv-export) [FFmpeg](https://github.com/bernardobrust/FFmpeg/tree/dav1d-mv-integration)
