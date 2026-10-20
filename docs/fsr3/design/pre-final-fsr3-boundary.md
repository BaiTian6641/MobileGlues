# Pre-final FSR3 boundary for scaled Iris scenes

Status: **design. Current installed path is still post-final FSR3.**

## Why

With a scaled Iris scene, the pack final pass writes the 2880×1585 display
image from scaled colortex inputs. The current bridge then downsamples that
native final image to the scene extent and upscales it again. This does not
improve the already-upscaled image and can leave far-terrain edge artifacts.

TAA-off reduced the far-terrain residue, while TAA+WSR-off did not change it
further. The remaining owner is the double-resample boundary, not FSR3 history
(history remains off and motion zero).

## Target boundary

1. Composite/world/hand remain at the scene extent.
2. Final pass renders only the scene active rectangle into the native color
   target, using scene-sized `viewWidth/viewHeight` and no native final override.
3. FSR3 consumes:
   - color: scene active rectangle copied to the FSR3 scene color texture;
   - depth: the bridge-owned scaled scene depth, not native main depth;
   - motion: zero in the stable path.
4. FSR3 writes the **full native** color target before HUD.
5. HUD/UI then render at native resolution with native viewport/uniforms.
6. NativeAA scale 1 keeps the existing direct/native behavior.

## Required state ownership

| Role | Writer | Reader |
| --- | --- | --- |
| Scene color | world/hand/composite | final pass |
| Scene depth | world/hand | final depth-dependent effects and FSR3 |
| Final active rect | final pass | FSR3 input copy |
| FSR3 output | FSR3 bridge | HUD composition |
| Native HUD | vanilla Gui | display |

The final pass must not sample and write the same texture. The FSR3 input copy
is a separate texture. Scissor, color mask, read/draw FBOs, viewport and clear
state are restored around every bridge operation.

## Gates

- Trace: final active rect is scene-sized; final uniform dimensions are scene-
  sized; FSR3 output is full native; HUD is native.
- No double downsample of the final image.
- No far-terrain edge ghost during fast pan.
- No hand/light/shadow duplicate.
- NativeAA regression.
- Only then consider FSR3 history/motion.
