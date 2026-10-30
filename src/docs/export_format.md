# Exporting data

**Work in progress, can (and will) change as the project advances. This is being written as the code is being dedveloped**

## Data Format
Exports as plain text:
```
src        := (integer, integer)
dst        := (integer, integer)
block_size  := (integer, integer)
source     := 1 or -1
magnitude  := float (3 decimal digits)
angle      := float (3 decimal digits, radians)
normalized := (float, float) (3 decimal digits)

Exmaple:
-----------------------------------------------------------------------------------------------------------------------------------------
FRAME 0
PTS 0
MV_COUNT 3
MV 0: src (100, 120) dst (115, 125) block_size (16, 16) source -1 magnitude 15.811 angle 0.322 normalized (0.949, 0.316)
MV 1: src (140, 120) dst (155, 130) block_size (16, 16) source 1 magnitude 18.028 angle 0.588 normalized (0.832, 0.555)
MV 2: src (180, 120) dst (180, 145) block_size (16, 16) source -1 magnitude 25.000 angle 1.571 normalized (0.000, 1.000)

FRAME 0
PTS 0
MV_COUNT 2
MV 0: src (100, 120) dst (115, 125) block_size (16, 16) source -1 magnitude 15.811 angle 0.322 normalized (0.949, 0.316)
MV 1: src (140, 120) dst (155, 130) block_size (16, 16) source 1 magnitude 18.028 angle 0.588 normalized (0.832, 0.555)
-----------------------------------------------------------------------------------------------------------------------------------------
```

The reason for 3 digits of precision is due to AV1 supporting up to 1/8 (0.125) pixel precision

The magnitude calculated here will not be the same as the one in the decoder, as there we make an approximation to meke the code faster