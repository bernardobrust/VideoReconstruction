# Exporting data

**Work in progress, can (and will) change as the project advances. This is being written as the code is being dedveloped**

## Data Format
Exports as plain text, as the following example shows:
```
FRAME 0
PTS 0
MV_COUNT 3
MV 0: src (100, 120) dst (115, 125) block_size (16, 16) magnitude 15.811 angle 0.322 normalized (0.949, 0.316)
MV 1: src (140, 120) dst (155, 130) block_size (16, 16) magnitude 18.028 angle 0.588 normalized (0.832, 0.555)
MV 2: src (180, 120) dst (180, 145) block_size (16, 16) magnitude 25.000 angle 1.571 normalized (0.000, 1.000)

FRAME 1
PTS 40
MV_COUNT 2
MV 0: src (105, 120) dst (120, 125) block_size (16, 16) magnitude 15.811 angle 0.322 normalized (0.949, 0.316)
MV 1: src (145, 120) dst (160, 130) block_size (16, 16) magnitude 18.028 angle 0.588 normalized (0.832, 0.555)
```

The magnitude calculated here will not be the same as the one in the decoder, as there we make an approximation to meke the code faster