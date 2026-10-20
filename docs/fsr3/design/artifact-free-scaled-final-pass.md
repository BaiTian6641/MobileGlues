# Artifact-free scaled path for final-pass-only packs

Status: **design only. No scaled device pass yet.**

Target: AuroraLumen private port on X1E/Adreno X1-85 through MobileGlues,
Iris 1.8.14, ANGLE ES 3.2. The latest FSR record describes a final-pass-only
pack. Verify its exact archive and compiled passes; Iris fallback shaders may
still produce world/hand draws into scene FBOs.
NativeAA is the current artifact-free fallback ([evidence](../validation/2026-10-20-x1e-nativeaa-final-pass.md)).

Use the [next-phase prompts](../scaled-presets-prompts.md) and
[diagnostic contract](scaled-presets-diagnostic-contract.md) for execution.
They refine the trace and extend qualification to all four scaled presets.

## Failure being solved

When Iris `RenderTargets` was scaled to 1694x932, the user saw a translucent
hand/light-shadow overlay. Main depth still has separate external ownership.
With the scene adapter disabled and NativeAA selected, the overlay
disappeared and the star layer stayed world-locked. This supports investigating
target ownership and coordinate domains at the hand/light boundary. The exact
failing draw/copy or shader-coordinate operation still needs a trace.

Iris details that constrain the fix:

- `RenderTargets` creates `colortex` targets and the `noHand`/`noTranslucents`
  depth copies from its scene extent and pack overrides. `depthtex0` refers
  to the externally supplied main depth; it is not resized by changing the
  constructor width/height.
- `copyPreHandDepth()` copies into the `noHand` texture. `noHandDestFb` has
  color attachment 0 plus that depth texture, but not every copy strategy uses
  the destination FBO. Whether any FBO remains bound for the hand draw is a
  measured question; `noHand` must retain pre-hand depth semantics.
- `HandRenderer.renderSolid()` and `renderTranslucent()` draw into the current
  FBO; they do not create their own target.
- `finalizeLevelRendering()` runs composites on their configured targets,
  followed by the final pass at native output.
- Fixed shadow/light-map resources retain their existing owners and extents.
  Distinguish them from screen-space lighting targets and the Minecraft lightmap.
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
scaled target. A depth-copy destination must not become the subsequent hand
depth-write target, and the `noHand` texture must remain a pre-hand depth copy.
Fixed shadow/light resources retain their pack-defined extents, not necessarily
the window extent.

## Implementation plan (bounded, no behavior expansion)

### Experiment A — prove the extents and bind

Add a debug-only trace at:

1. `RenderTargets` constructor/resize: scene color, depth, `noHand` size.
2. `IrisRenderingPipeline.beginHand()` and the depth-copy operation HEAD/RETURN:
   read/draw FBOs, color/depth attachment identity and extent, copy rectangles,
   selected strategy, viewport/scissor.
3. `HandRenderer.renderSolid()` and `renderTranslucent()` entry, actual
   draw/batch execution, and return: the same fields plus program identity.
   A shader's cleanup can rebind the main FBO; RETURN alone is insufficient.
4. `FinalPassRenderer.renderFinalPass()` HEAD/draw/RETURN: current target,
   extent and actual uploaded uniforms; distinguish bridge submit before/after.
5. Actual world/sky and shadow/light draws used by the pack, plus HUD entry.

Experiment A's gate is a trustworthy ordered trace that identifies actual
draw ownership and the first mismatch, including skipped draws/unknown fields.
A mismatch is useful diagnostic evidence; it does not prevent Experiment B
from repairing that mismatch. The eventual product gate requires matched
scene/hand color and depth. Compile trace code out of product builds.

### Experiment B — one scene extent for hand

- Keep `RenderTargets` sizing at the quality extent.
- Correct the producer-stage transition identified by the trace so actual
  hand draws use the scene target. Do not infer that the depth copy left its
  destination bound, or override legitimate post-draw cleanup.
- Do not force the hand into the `noHand` copy: that would invert `depthtex2`
  semantics and can create the light/shadow overlay this design is eliminating.
- Apply the scene viewport at explicit scene transitions and the native
  viewport at display transitions. Preserve arbitrary offscreen/subviewports;
  a generic framebuffer bind is not a sufficient target-role signal.
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
the opt-in for this experiment. Quality and Ultra Performance are also
unqualified until separately tested. If any gate fails, the adapter must use
NativeAA coherently across allocation, uniforms and dispatch rather than ship
a scaled artifact. This is required fallback behavior; the current default
setting alone does not establish a complete automatic fallback implementation.

### Experiment C — isolate light/shadow if B still overlays

If B removes the hand duplicate but leaves a light/shadow overlay:

- Verify the pack's shadow/light programs do not sample a scene-scaled
  `viewWidth/viewHeight`; if they do, scope the viewport-uniform override by
  program/pass rather than globally.
- Verify no `ShadowRenderTargets` object passes through `RenderTargetsMixin`.
- Check wrapper viewport adjustments as well: they can affect a shadow FBO
  even when its Java target object never passes through that mixin.
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
  it does not reduce the original world draw's pixel work.

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

The size above is the Balanced example at the recorded output extent.
Quality, Performance and Ultra Performance require their own draw and visual
evidence, using the ladder in the diagnostic contract. Performance is recorded
separately; no FPS claim is made by this design.
