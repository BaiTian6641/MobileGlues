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
| Scene extent | **NativeAA for final-pass-only packs**; scaled extents only for gbuffer packs | any |
| FSR3 history | **OFF** (`MG_FSR3_HISTORY` opt-in only) | ON |
| Pack TAA | if the pack exposes it; single temporal owner | OFF |
| RCAS sharpness | 0.4 | 0.2 |
| Direct post-HUD FSR | disabled when the bridge loads | developer diagnostic |
| UI/HUD | native target viewport | native |
| Hands | scene-owned, spatial only; softness accepted | ghost/reactive policy needed |

History-off removes double-temporal ghosting. The current AuroraLumen pack has
no TAA option, so history-off is the only temporal control. With history off,
motion vectors are zeroed; camera-attached hands and the sky are not
reprojected. FSR3 supplies spatial reconstruction and sharpening.

For a final-pass-only pack, scaling Iris `RenderTargets` changed hand/depth
ownership and produced a translucent hand/light-shadow overlay. NativeAA
leaves the targets native and removes it. NativeAA therefore has no scene
resolution gain on that pack; the faster scaled presets are not artifact-free
there. Scaled extents remain conditional for gbuffer packs, where the scene
target is actually reduced.

## Implemented routing

- `ApplyFSR3Present` is disabled as soon as the bridge mode is negotiated.
- History defaults off; `MG_FSR3_HISTORY=1` is developer-only.
- Native-target viewport reset keeps menus/HUD/hand composition out of the
  lower-left scene viewport.
- `ViewportUniformsMixin` reports the scene extent to world/composite programs
  and the native extent during the final pass.
- FSR3 quality preset remains user-selectable. NativeAA is the artifact-free
  point for the current final-pass-only pack; Quality/Balanced reproduced the
  hand/light-shadow overlay there.

## Resolved observation (2026-10-20)

The previously reported two-overlay hand was the scaled final-pass-pack hand
path, not a missing hand mesh. NativeAA + final-pass-scoped viewport uniforms
removed the translucent overlay and fixed world-locked stars. Evidence:
[`validation/2026-10-20-x1e-nativeaa-final-pass.md`](../validation/2026-10-20-x1e-nativeaa-final-pass.md).

A hand-sharpening or final-pass-pack performance profile still requires the
separate boundary work below; it is not implemented or qualified.
