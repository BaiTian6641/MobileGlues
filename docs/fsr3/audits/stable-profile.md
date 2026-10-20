# FSR3 stable profile (temporary product policy)

Status: **stability fallback, not temporal FSR3 qualification.**

The reviewed pass/input audit establishes that hands are scene-owned: hand
solid/translucent write the same gbuffer color and main depth as the world,
are not TAA-jittered, and carry no velocity. Camera-only reprojection therefore
either ghosts them or requires a reactive mask that sacrifices accumulation.
Excluding hands and compositing them later is a separate profile decision that
changes pack lighting/depth; it is not a free hand fix.

## Policy

| Item | Stable profile | Research profile |
| --- | --- | --- |
| Scene extent | Quality 1.5x (or Performance if needed) | any |
| FSR3 history | **OFF** (`MG_FSR3_HISTORY` opt-in only) | ON |
| Pack TAA | **ON** (single temporal owner) | OFF |
| RCAS sharpness | 0.4 | 0.2 |
| Direct post-HUD FSR | disabled when the bridge loads | developer diagnostic |
| UI/HUD | native target viewport | native |
| Hands | scene-owned, spatial only; softness accepted | ghost/reactive policy needed |

History-off removes double-temporal ghosting. Pack TAA supplies smoothing and
anti-flicker; FSR3 supplies scene-extent reconstruction and sharpening. The
hand remains part of the scene and can be softer than native because it is
spatially upscaled. No hand temporal/motion claim is made.

## Implemented routing

- `ApplyFSR3Present` is disabled as soon as the bridge mode is negotiated.
- History defaults off; `MG_FSR3_HISTORY=1` is developer-only.
- Native-target viewport reset keeps menus/HUD/hand composition out of the
  lower-left scene viewport.
- FSR3 quality preset remains user-selectable; Quality is the recommended
  first stable point.

## Open hand decision

If hand sharpness is unacceptable, the next profile must either:

1. accept hand ghosting with history on and a profile-qualified reactive mask;
2. render hands after reconstruction at native resolution while proving pack
   lighting/occlusion; or
3. use NativeAA/no scene reduction for hand-critical scenes.

None is implemented or qualified.
