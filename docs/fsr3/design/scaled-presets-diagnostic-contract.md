# Scaled preset diagnostic contract

The current phase removes the scale-dependent hand/light-shadow overlay while
preserving NativeAA's confirmed appearance. It extends Experiments A/B/C in
[artifact-free-scaled-final-pass.md](artifact-free-scaled-final-pass.md).
Use [scaled-presets-prompts.md](../scaled-presets-prompts.md) for agent
assignments. Status remains in the existing execution state and task ledger.

Reviewed on 20 October 2026 at `fb0bd162278847e00ba7a88d6d1fcd249ea1e034`.
The [NativeAA record](../validation/2026-10-20-x1e-nativeaa-final-pass.md)
and the user's confirmation establish no duplicated hand/light-shadow overlay
and world-locked stars for that run. They do not establish scaled draw
ownership or temporal FSR3 quality. This phase keeps shaders on, history off,
motion zero, RCAS 0.4, and the existing delivery runtime.

## Current source implications

| Observation | Consequence for this phase |
| --- | --- |
| `RenderTargetsMixin` scales dimensions; the native scale comes from the selected quality | Conditional sizing exists, but no complete per-profile failure gate/effective-preset negotiation is evident in this path |
| `ViewportUniformsMixin` uses `Fsr3Bridge.inFinalPass` | Inspect actual program uploads and cache lifetime; the boolean alone is not proof that every consumer sees the intended dimensions |
| Final HEAD/RETURN toggle the scope under separate `available()` checks | Ensure scope cleanup on every return, exception and availability transition; preserve nesting/context ownership |
| `MG_FSR3_HISTORY` is tested using `getenv(...) != nullptr`, once | Unset it for this phase. `"0"` and an empty value still enable the opt-in. A process restart is needed after changing a cached environment setting |
| `run_camera_motion` receives `allow_motion=false` when history is off | Preserve zero motion and reset behavior; do not revive camera-only reprojection to repair a scale mismatch |
| FSR still consumes the post-final image | Treat this as the temporary history-off path. An input downsample proves neither original scene resolution nor a useful speedup |
| `FSR3AdjustViewport` still overrides large color targets | Include shadow/offscreen/subviewport cases; the [F08 reproduction](../audits/2026-10-20-followup-review.md#f08-p1-the-new-native-viewport-heuristic-overwrites-legitimate-offscreen-viewports) remains relevant |

The saved records use “AuroraLumen” for different private variants. An older
private-port record describes gbuffer/composite/TAA work, while the latest
FSR record describes a final-pass-only pack without a TAA option. Capture the
actual archive SHA-256, profile/settings and compiled pass graph. Do not infer
one variant's behavior from the name, or equate “no TAA option” with proof
about every compiled history operation. The confirmed NativeAA observation
remains valid for its tested package; pack identification makes it repeatable.

## Source corrections that matter

The audited Iris revision is `eb7afb99f747cc8ed5ee4072119539035d33cefd`.
Check the delivered JAR descriptors before adding mixins.

- `currentDepthTexture` is supplied by Minecraft. Iris allocates its
  `noHand` and `noTranslucents` copies separately. Constructor scaling does
  not resize the external main depth.
  [Pinned RenderTargets source](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/targets/RenderTargets.java).
- `noHand` maps to **`depthtex2`**; `noTranslucents` maps to **`depthtex1`**.
  Their snapshots occur at different points in the frame.
  [Pinned sampler bindings](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/samplers/IrisSamplers.java),
  [pipeline boundaries](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/pipeline/IrisRenderingPipeline.java).
- The first/dirty depth copy and steady-state copy can use different routes.
  The selected strategy can affect read/draw bindings; do not assume a
  destination FBO was left active.
  [Pinned depth-copy strategies](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/gl/texture/DepthCopyStrategy.java).
- Hand rendering flushes batches and restores phase/projection state.
  Capture active draws and return state separately.
  [Pinned hand renderer](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/pathways/HandRenderer.java).
- Viewport uniforms are supplied per program with per-frame update frequency.
  Record actual uploaded values in the program that consumes them.
  [Pinned viewport uniforms](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/uniforms/ViewportUniforms.java).

Also inspect pinned Iris
[FallbackShader.java](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/pipeline/programs/FallbackShader.java)
and [ExtendedShader.java](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/pipeline/programs/ExtendedShader.java):
their `apply()` selects scene framebuffers, while
`clear()` can bind the main target. A final-pass-only pack may still use Iris
fallback world shaders. The actual draw trace, not the absence of pack-authored
gbuffer programs, must establish whether world work is scaled.

## Resource and coordinate invariants

| Role | Required domain and ownership |
| --- | --- |
| World and hand color | One scene-resolution ownership path, including opaque and translucent phases; preserve per-phase hand projection/depth behavior |
| Current scene depth / `depthtex0` | Matches the scene's coordinates and valid region; account for the existing external owner before replacing anything |
| `noHand` / `depthtex2` | Scene-sized pre-hand snapshot; never a hand depth-write target |
| `noTranslucents` / `depthtex1` | Scene-sized pre-translucent snapshot; preserve what is present at that boundary |
| Composite inputs | Follow their explicit scene/relative/fixed resource domains; do not assume every attachment equals color0 |
| Final output and HUD/UI | Display resolution and corresponding layout/uniforms |
| Shadow maps and fixed light resources | Retain the pack's existing allocations, viewport, projection and sampling conventions |
| Screen-space lighting | Follow the producing/consuming pass's domain; it is not automatically a fixed shadow map |
| Minecraft lightmap / overlay texture | Preserve existing bindings and semantics; these are not window-size render targets |

“Native shadow/light targets” means their original pack-defined extent.
It does not mean resizing a 4096² shadow target to a 2880×1585 window.
Ordinary read-only FBO binds, offscreen targets and intentional subviewports
must keep their GL semantics.

For this bounded profile, require matched scene color/depth extents and
explicit copy/sampling rectangles. A complete framebuffer alone is insufficient:
attachment dimensions can differ without exposing the intended ownership.
Preserve depth/stencil formats and masks if changing the depth resource.
Do not copy a native lower-left depth rectangle and call it a scaled full view.

## Experiment A capture events

Use a bounded capture window, identified by context/surface/resource
generation and frame/event sequence. Include enough first-use and steady-state
frames to observe both depth-copy routes. Log why a method skipped drawing.
Document the window/event budget and how the user can re-arm a short capture
after changing the held item within the same agreed Balanced session.

| Event | Purpose |
| --- | --- |
| Allocation/resize completion | Record actual resources after creation, not only requested dimensions |
| World draw samples and sky draw | Establish the original raster extent and program role |
| `beginHand` HEAD / depth copy HEAD+RETURN / `beginHand` RETURN | Compare source/destination copies and bindings across the pre-hand boundary |
| Solid hand HEAD / active draw-batch events / RETURN | Observe actual hand color/depth writes and later cleanup separately |
| `beginTranslucents` and its depth copy | Verify `depthtex1` snapshot timing and subsequent scene target |
| Translucent hand HEAD / active draw-batch events / RETURN | Verify the second hand phase or record that no such item was drawn |
| Final HEAD / actual program draw / RETURN | Prove output extent and the values consumed by the final shader |
| Native bridge submit before/after | Separate existing pack output from reconstruction and copies |
| HUD entry | Prove display viewport and absence of another scene/hand reconstruction |
| Actual shadow/light program entry/draw/exit | Verify fixed target roles and coordinate domains; mark absent passes N/A |

One returned Balanced capture is the initial gate. Prepare a short user run
sheet: same world/pack/settings, rotate toward stars, show opaque and
translucent held items if available, swing the hand, and open/close the pause
menu. Do not change time/weather/world state without the user's agreement.
If an item/effect is unavailable, record missing coverage and collect it in a
later agreed session; never invent a successful draw.

The user operates the game. Host instrumentation, builds and capture
instructions can be prepared independently. Installation, ADB collection,
launches and settings changes require an explicitly authorized device session.
Use serial `C1L63708PZ` when that session is authorized.

## Trace fields and neutrality

Record one run manifest: source/dirty-diff identity, APK/core/helper/JAR hashes,
pack SHA-256 and profile, runtime tuple, OS/provider/driver, requested/effective
preset, output extent, history-environment presence, RCAS, capture bounds and
trace schema version. Record unknown helper/pack identities as missing.

Each event needs:

- Frame/event number, phase, thread/context/surface, resource generation,
  allocation/copy/draw/return kind, and draw-executed or skipped status.
- Read and draw FBO identities; active draw/read buffers; every relevant
  color/depth/stencil attachment's object kind, name, generation, target,
  mip/layer, format, sample count, allocated extent and active rectangle.
- Viewport, enabled scissor and rectangle, relevant color/depth/stencil masks,
  blend/depth state, program identity and explicit target role.
- Copy strategy, source/destination identities, rectangles and first/dirty
  versus steady-state path.
- Actual `viewWidth/viewHeight`, aspect/projection convention and relevant
  depth/light sampler-to-unit/resource mapping at the consuming program.
  An inactive uniform is “inactive,” not zero.
- Pre/post FSR resource identities/extents, history reset, zero-motion state
  and failure/fallback reason.

Use wrapper allocation metadata or type-correct GL queries. Do not bind a
renderbuffer/array texture as `GL_TEXTURE_2D` to obtain a convenient size.
Preserve texture units, read/draw FBOs, samplers, renderbuffer bindings,
wrapper shadow state and application errors if diagnostic queries need state.
Do not call `glGetError` merely to erase failures.

Accumulate a bounded buffer and export at a safe boundary. Report dropped
events and overflow; an incomplete trace cannot prove an absent extra draw.
Avoid file writes per draw, GPU stalls and readbacks. Pixel captures, if later
needed, are a separate explicitly bounded diagnostic.
Compile trace code out of the release candidate. Confirm trace-on/off state
equivalence in host checks and repeat visual acceptance on the release tuple.

Illustrative record shape, **not a collected frame or an existing API**:

```json
{
  "schema_version": 1,
  "event": "HAND_SOLID_DRAW",
  "frame_id": null,
  "event_id": null,
  "context_id": null,
  "resource_generation": null,
  "requested_preset": "Balanced",
  "effective_preset": null,
  "draw_executed": null,
  "draw_fbo": null,
  "color0": {"name": null, "generation": null, "extent": null},
  "depth": {"name": null, "generation": null, "extent": null},
  "viewport": null,
  "uniforms": {"viewWidth": null, "viewHeight": null},
  "history_enabled": null,
  "motion_zero": null,
  "dropped_events": null
}
```

This abbreviated shape illustrates unknown-value handling. The implemented
schema must include the applicable fields above and reject incomplete records
for gates that depend on them.

## Decide the fix from the first divergence

| Trace result | Next action |
| --- | --- |
| Draws are scene-sized; hand return alone is native | Inspect cleanup; do not add a speculative rebind based only on RETURN |
| Actual hand draw is native while world is scaled | Correct the producing shader/batch target transition and its viewport/depth owner |
| Color is scaled but shared main depth stays native | Define coherent scene depth and update all producers, snapshots, readers and stencil dependencies |
| `noHand` changes during hand writes | Separate the snapshot from the scene depth target and verify copy timing |
| Color/depth are coherent but a light overlay remains | Trace the affected light/depth sampler and screen/shadow coordinates; investigate Experiment C |
| Large shadow/offscreen target gets display viewport | Replace size-based viewport inference with explicit role/transition control |
| Final program receives scene dimensions or stale values | Correct pass scope and actual uniform upload/cache behavior |
| Scene writes remain native; only FSR input is small | Implement a real scene boundary; do not mark scaled preset success |
| All sampled state agrees but artifacts remain | Extend the capture at the visible effect; consider shader coordinate math, sampling/format or state leakage without declaring an unsupported cause |

Experiment A succeeds when its evidence supports a decision, even if it exposes
broken ownership. Experiment B repairs that first divergence. Experiment C
is conditional on residual lighting artifacts; it is not permission to change
unrelated pack lighting.

Do not transplant Complementary's `composite6` route into this pack by name.
Keep the broader temporal roadmap separate. If the current post-final path
itself proves to be the remaining cause, propose and review a bounded earlier
handoff for this profile with explicit finishing/color semantics.

## Preset ladder and fallback

Derived from the current `fsr3_quality_scale`, Java `Math.round` and native
rounding/clamp policy. These are **expected dimensions**, not measured draws.

| Preset | Divisor | Scene extent for 2880×1585 output | Phase requirement |
| --- | --- | --- | --- |
| NativeAA | 1.0 | 2880×1585 | Preserve confirmed baseline and rollback |
| Balanced | 1.7 | 1694×932 | First trace and corrected scaled proof |
| Quality | 1.5 | 1920×1057 | Separate qualification |
| Performance | 2.0 | 1440×793 | Separate qualification |
| Ultra Performance | 3.0 | 960×528 | Separate qualification; do not infer acceptable quality from lower ratios |

Derive dimensions from the real output viewport, not the panel's 2880×1800
specification. Use one effective preset and rounding policy across allocations,
uniforms, copies, native dispatch and diagnostics. Test odd dimensions and
small-window clamping. Preserve the intended projection/aspect ratio and
document rounding rather than silently stretching a sample grid.

Automatic fallback is required behavior, not an already proven implementation.
Resolve eligibility before allocating/drawing. Unsupported profiles, missing
capabilities or failed invariants choose effective NativeAA consistently.
On resize/preset/resource changes, transition the complete graph and reset
identities together. On a late failure, do not present mismatched resources or
switch only one component's dimensions; invalidate the frame and arrange a
coherent fallback. Record requested/effective mode and reason.

Visual failure without a runtime-detectable invariant keeps the profile/preset
unqualified in the support policy. Do not invent automatic detection of
ghosting or overlays. A NativeAA fallback is a fallback pass, not a pass for
the requested Quality/Balanced/Performance/Ultra Performance mode.

## Completion gates

For **each** advertised preset, require:

1. Correct scene and actual hand-draw color/depth domains, snapshot semantics
   and observed allocations at the expected scale.
2. No duplicate translucent hand or light/shadow overlay; world-locked stars
   under rotation; native centered menus/HUD; preserved enabled effects.
3. Solid and translucent hand coverage, daytime lighting, night sky,
   scene/GUI transitions, resize/odd extents and shader reload. Mark absent
   effects N/A for that profile and missing tests pending.
4. Safe NativeAA fallback, restored state and no stale identities after
   failure, preset changes or return to title.
5. Visual confirmation on the trace-disabled paired release candidate, with
   exact identities and raw evidence retained.

Use the existing host suite and focused tests against real changed paths.
The pure viewport planner suite does not cover the wrapper's early large-target
branch. A fixture testing trace formatting alone cannot qualify draw ownership.
Validate first-use and steady copy, skipped-hand paths, exception-safe final
scope, actual target-role decisions and synchronized fallback.

Measure performance only after those gates, on matched routes/settings with
shaders on. NativeAA is the visual control; a validated reduced spatial path
can isolate savings from reconstruction overhead. Report frame intervals and
variance, not an unmatched FPS screenshot. This phase cannot close temporal
FSR3, Complementary/ACT/WSR or sustained release-performance gates.
Lower scene resolution mainly targets pixel work; it does not imply a
corresponding reduction in game simulation or geometry-submission cost.
