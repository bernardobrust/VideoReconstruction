# Video Reconstruction
This is a research repository aimed at studying the possibility of doing 3D reconstructions using computer vision along with AV1's video codec compression data (such as motion vectors). An overview can be found [here](ideas.md). A proper paper is under way.

For the source code check [source code](src/), for the TODO list check [todo](todo.md), for the online sources [sources](sources.md).

## Modified Libraries
In order to export compression data from libdav1d and FFmpeg I'm building modified versions of them at [libdav1d](https://github.com/bernardobrust/dav1d/tree/mv-export) [FFmpeg](https://github.com/bernardobrust/FFmpeg/tree/dav1d-mv-integration) as they don't export them. 

## Note on AI Generated Code
A list of AI-generated code is available [here](ai_code.md), along with the reasoning for using it. All code, after generated was reviewed by me with care.
