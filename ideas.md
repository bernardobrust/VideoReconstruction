# Ideas for the Paper
## Introduction
### 1. Overview of the Proposed Approach
Using a video encoded with the AV1 codec, we investigate how the motion vectors generated during video compression can be leveraged for computer vision tasks. Specifically, we explore whether these vectors can provide useful motion information for applications such as camera motion estimation, object tracking, and 3D scene reconstruction.

### 2. Motivation and Applications
The ability to reconstruct 3D geometry from video data has numerous applications, including:

- Robotics and autonomous navigation
- 3D modeling and digital asset creation
- Augmented and virtual reality systems
- Film production and visual effects
- Video game development
- Industrial inspection and measurement

Traditional approaches often require computationally expensive feature extraction and optical flow estimation. Motion information already available within compressed video streams may offer a more efficient alternative.

### 3. Why AV1?
AV1 is an open, royalty-free video codec designed for modern internet video delivery and is experiencing rapid industry adoption.

Several characteristics make AV1 attractive for this work:

- Wide and growing deployment across streaming platforms
- Open standard with accessible tooling and documentation
- Advanced motion compensation mechanisms
- Availability of motion vector information generated during encoding

While motion vectors are not unique to AV1, the codec provides a rich source of motion information that may be exploited for computer vision applications without requiring full optical flow computation.

### 4. Research Questions
- Can AV1 motion vectors serve as a useful approximation of scene motion?
- How do they compare to traditional optical flow methods?
- Can they provide sufficient information for Structure-from-Motion (SfM) pipelines?
- What are the limitations introduced by video compression?

***

## Background
### 1. Fundamentals of AV1 Compression
- Inter-frame prediction
- Block-based motion compensation
- Motion vector generation
- Reference frames and temporal prediction

### 2. Motion Analysis in Computer Vision
- Optical flow
- Feature detection and tracking
- Camera motion estimation
- Structure-from-Motion (SfM)

### 3. Related Work
- Use of compressed-domain information in computer vision
- Motion-vector-based object tracking
- Previous attempts at compressed-domain 3D reconstruction
- Locating camera position from images

***

## Methodology
### 1. Extracting AV1 Motion Vectors
- Accessing motion vector information from encoded streams
- Converting block-based vectors into a dense motion field
- Handling missing or inconsistent vector regions

### 2. Motion Estimation and Object Tracking
- Building dense movement maps
- Tracking points of interest across frames
- Evaluating motion vector accuracy

### 3. Structure-from-Motion Pipeline
- Feature selection and correspondence generation
- Camera pose estimation
- Triangulation of scene points
- Point cloud generation and refinement

### 4. Reconstruction of Low-Texture Scenes
- Challenges posed by feature-poor surfaces
- Leveraging motion information from shadows
- Using illumination changes as additional cues
- Potential benefits for otherwise difficult reconstruction scenarios

### 5. Implementation
- Performance oriented C11 with use of data-oriented-design
- Use of already existing efficient decoders (FFmpeg)
- Data analysis using GNU Octave (or python)

***

## Evaluation
### 1. Experimental Setup
- Test videos and datasets
- Encoding parameters
- Hardware and software environment

### 2. Metrics
- Motion estimation accuracy
- Tracking robustness
- Reconstruction quality
- Point cloud density
- Line and curve accuracy
- Computational efficiency

### 3. Comparison Against Baselines
- Traditional optical flow methods
- Feature-based SfM pipelines
- Hybrid approaches

***

## Discussion
### 1. Advantages of Compressed-Domain Motion Analysis
- Reduced computational cost
- Availability in already-encoded video streams
- Scalability for large datasets

### 2. Limitations
- Compression artifacts
- Block-level precision constraints
- Sensitivity to encoding settings
- Occlusions and scene complexity
- Handling low detail / texture objects

### 3. Future Directions
- Hybrid motion vector and optical flow approaches
- Real-time reconstruction systems
- Use alongside images
- Construction of usable 3D models

***

## Conclusion
### 1. Summary of Contributions
- Demonstrate the use of AV1 motion vectors for motion analysis
- Explore their application to 3D reconstruction pipelines
- Evaluate benefits and limitations relative to traditional approaches

### 2. Broader Impact
Compressed-domain computer vision techniques could enable more efficient processing of the rapidly growing volume of video data while reducing computational requirements for 3D reconstruction and scene understanding.
