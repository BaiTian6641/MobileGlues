# Artifact-free scaled path for final-pass-only packs

Status: **design only. No scaled device pass yet.**

Target: AuroraLumen private port on X1E/Adreno X1-85 through MobileGlues,
Iris 1.8.14, ANGLE ES 3.2. The pack exposes a final pass but no gbuffer/deferred
world pipeline that MobileGlues can use as a real reduced-resolution scene.
NativeAA is the current artifact-free fallback ([evidence](../validation/2026-10-20-x1e-nativeaa-final-pass.md)).

## Failure being solved

When Iris `RenderTargets` was scaled to 1694x932 while the pack still rendered
hand/depth at native ownership, the user saw a translucent hand/light-shadow
overlay. With the scene adapter disabled and NativeAA selected, the overlay
disappeared and the star layer stayed world-locked. The overlay is therefore a
target-ownership defect at the hand/light boundary, not a shader math defect.

Iris details that constrain the fix:

- `RenderTargets` creates scaled `colortex` targets, `depthtex0`, and the
  `noHand`/`noTranslucents` depth copies at the constructor extent.
- `copyPreHandDepth()` copies pre-hand depth into `noHandDestFb`; the FBO has
  color attachment 0 plus the `noHand` depth texture. Whether Iris leaves that
  FBO bound for the hand draw is a measured question, not an assumption: the
  `noHand` texture must retain pre-hand depth semantics.
- `HandRenderer.renderSolid()` and `renderTranslucent()` draw into the current
  FBO; they do not create their own target.
- `finalizeLevelRendering()` runs the composite and final pass at native output.
- Shadow/light targets are not the same `RenderTargets` object and must not be
  scaled by the scene adapter.
- Iris viewport uniforms are used by world and composite shaders; the final
  pass and UI must see native dimensions.

Source anchors: Iris `RenderTargets.java:122-128` (target allocation/scale),
`:209-230` (depth copies), `IrisRenderingPipeline.java:1045-1067`
(beginHand/beginTranslucents/finalize), `HandRenderer.java:73-148` (hand
phases), and the existing [pass-input audit](../audits/minecraft-iris-pass-input.md).

## Required ownership matrix

| Phase | Color | Depth | Viewport | Motion |
| --- | --- | --- | --- | --- |
| World solid/cutout/entity | scene color0 (scaled) | scene depth (scaled) | scene extent | zero in current profile |
| Copy pre-hand | scene color0 (scaled) | `noHand` copy (scaled) | n/a | n/a |
| Hand solid | **scene color0 (scaled)** | **single hand depth target at scene extent; measure which** | **scene extent** | **zero** |
| Translucents / hand translucent | scene color0 (scaled) | same hand depth target at scene extent | scene extent | zero |
| Composite | scene targets (scaled) | scene depth (scaled) | scene extent | zero |
| Final pass | native output | reads scene depth | **native extent** | zero |
| HUD/UI | native output | native | native | n/a |
| Shadow/light maps | fixed shadow extent | fixed shadow extent | fixed | n/a |

Invariant: no hand or world color write may bind a native-sized color/depth
target while the rest of the scene is scaled. No final/UI write may bind a
scaled target. The pre-hand copy must not leave a depth-only FBO bound, and
the `noHand` texture must remain a pre-hand depth copy.

## Implementation plan (bounded, no behavior expansion)

### Experiment A — prove the extents and bind

Add a debug-only trace at:

1. `RenderTargets` constructor/resize: scene color, depth, `noHand` size.
2. `IrisRenderingPipeline.beginHand()` return: current FBO, color attachment
   type/name, attachment texture size, viewport.
3. `HandRenderer.renderSolid()` and `renderTranslucent()` return: same fields.
4. `FinalPassRenderer.renderFinalPass()` HEAD/RETURN: current target and extent.

Gate: hand FBO color/depth must equal the scaled scene extent. If they are
native, the duplicate is a bind ownership bug, not a viewport bug. Remove the
trace before any product build.

### Experiment B — one scene extent for hand

- Keep `RenderTargets` sizing at the quality extent.
- Ensure the pre-hand copy restores the scaled scene color FBO used by the
  hand; do not leave a depth-only copy FBO or a native main target bound.
- Do not force the hand into the `noHand` copy: that would invert `depthtex1`
  semantics and can create the light/shadow overlay this design is eliminating.
- Apply the scene viewport at bind time; keep the native viewport only for the
  final pass and FBO 0.
- Keep `noHand`/`noTranslucents` at the same scaled extent as the scene.
- Keep the pack's shadow/light render targets untouched.
- Keep viewport uniforms scene-scaled for world/composite and native during the
  final pass.
- Keep motion disabled with history off; hands and sky are not reprojected.

Gate: Balanced 1.7x shows no translucent hand/light overlay, stars stay
world-locked, and hand/menu alignment matches the NativeAA baseline.

### Activation policy

The scene adapter stays conditional on a scaled quality preset. The current
final-pass-only pack defaults to NativeAA; selecting Balanced/Performance is
the opt-in for this experiment. If any gate fails, the adapter must force
NativeAA rather than ship a scaled artifact.

### Experiment C — isolate light/shadow if B still overlays

If B removes the hand duplicate but leaves a light/shadow overlay:

- Verify the pack's shadow/light programs do not sample a scene-scaled
  `viewWidth/viewHeight`; if they do, scope the viewport-uniform override by
  program/pass rather than globally.
- Verify no `ShadowRenderTargets` object passes through `RenderTargetsMixin`.
- Verify `depthtex1`/`depthtex2` are sampled as textures at scene extent, not
  as native window extents.
- Do not mask the overlay with a blend or clear; that would hide an ownership
  error.

Gate: overlay absent with scaled scene and final pass at native.

## Explicit non-solutions

- Do not render a native hand copy after FSR3 while leaving the scaled hand in
  the final image; that creates two hands.
- Do not scale shadow/light render targets to the scene extent.
- Do not advertise motion vectors for camera-attached hands or the sky.
- Do not make Balanced the default until these gates pass. NativeAA remains
  the artifact-free default.
- Do not treat the final-pass downsample alone as scene-resolution scaling;
  it cannot reduce world geometry cost.

## Acceptance

| Check | Required |
| --- | --- |
| Scaled scene extents | 1694x932 at Balanced from 2880x1585 |
| Hand duplicate | absent |
| Light/shadow overlay | absent |
| Stars while rotating | world-locked |
| Menus/HUD | native, centered, no lower-left crop |
| Hand/depth alignment | matches NativeAA baseline |
| FSR3 history | off; motion zero |
| Pack bytes | unchanged |
| Fallback | NativeAA on any gate failure |

Performance is recorded separately; no FPS claim is made by this design.
