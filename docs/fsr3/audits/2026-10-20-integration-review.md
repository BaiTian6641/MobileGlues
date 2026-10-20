# FSR3 integration review: renderer-only and hybrid paths

Review date: 20 October 2026. Branch: `googlebook/x1e-stage1-3`.
Review began at `8f9df8d7eedd36e348156ed8f58e0905c45fe75b`; source changes
were reconciled through `51b3eaa9a0be6dc358458caa693e3238dc622fce`, with
execution records checked through `87e289d0b10d2c7368e5002992d9e8aeade19722`.
Scope includes the user's follow-up accepting review of a Minecraft bridge
for motion and other frame data.

Source line numbers below refer to `51b3eaa`. At final inspection, a concurrent
uncommitted edit removed the leading newline before the motion shader's
`#version`; it shifts later JNI line numbers by one and does not change these
findings. The saved host-suite result belongs to the stated source baseline.

**Recommendation: continue with the same-provider GLES backend and a hybrid
Minecraft/Iris integration, but do not qualify the current implementation as
performance upscaling.** The renderer-only path cannot save scene work as
implemented. The hybrid path has a useful starting structure, but submits the
wrong reconstruction boundary and has incomplete resolution and temporal-data
ownership. The backend also has concrete resource-binding and state defects.

This is a review record, not adoption of a replacement implementation plan.
This review did not edit product source, shaderpack archives, game data,
installed APKs or existing acceptance evidence. Its changes are this report,
the index link and isolated host reproduction evidence. No device commands
were run by this review.

Concurrent implementation commits added camera-position transport, smaller
FSR input extents, scene-depth extraction, restoration of image bindings,
pipeline-replacement invalidation and a no-context getter guard. Those changes
are accounted for below. Missing camera translation, equal input/output
extents and lost image-unit bindings are **not** reported as current defects.

## Scope and evidence

The delivery baseline remains Minecraft 1.21.1 / NeoForge 21.1.262 / Java 21 /
Iris 1.8.14-beta.1 / Sodium 0.8.13. NeoForge 21.1.264 is optional; 26.3 and
1.20.1 do not replace this lane.

The intended device is `C1L63708PZ`, HP Googlebook 14, Snapdragon X1E80100 /
Adreno X1-85. Existing project evidence names Android 17 / API 37, build
`HP/quartz/quartz:17/CL3B.260622.276.R2/16560795:user/release-keys`, with ANGLE
2.1 over Vulkan 1.4.295, Adreno driver 512.886.6. These are recorded target
identities, not a fresh device verification in this review.

Evidence checked in this turn:

- Read the current core, both native and Java bridge paths, the FSR3 plan,
  frame contract, decisions, execution records, and relevant existing probes.
- `sh MobileGlues-cpp/tests/run.sh`: **PASS**, all 11 test executables.
  These include pure viewport/matrix tests and ABI layout/value tests, not
  complete FSR3 image-quality or integration tests. Rerun after the concurrent
  source updates; the final raw log is retained with this report.
- Linked the unmodified `fsr3_backend.cpp` directly into a small host EGL
  harness, avoiding the full wrapper's previously recorded host initialization
  problem. Actual provider: **llvmpipe (LLVM 20.1.8, 256 bits)**,
  **OpenGL ES 3.2 Mesa 26.0.0-devel**, host **Linux 7.0.0-38-generic**.
  This is software GLES evidence, not X1E evidence.
- The host harness confirmed state loss on initialization and the incorrect
  integer-sampler binding described below. All eight dispatch calls nevertheless
  returned success. The harness does not establish image quality.
- Verified local AMD SDK HEAD:
  `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`, v1.1.4 / FSR3.1.4.
- Verified local Iris source HEAD:
  `eb7afb99f747cc8ed5ee4072119539035d33cefd`. The pinned Iris JAR and
  `/tmp/iris-1.8.14.jar` both hash to
  `60d5f8bf52f25e9986440f4a4270a6bd986d9cff994510e1108ac1547063b314`.
  JAR bytecode inspection confirms that `viewWidth` and `viewHeight` read
  Minecraft's main render target. This is not a complete source/JAR equivalence
  proof.

Reproduction source, raw log, commands, and hashes are in
[`2026-10-20-integration-review-evidence/`](2026-10-20-integration-review-evidence/).

## Post-review implementation status (2026-10-20)

This is the adoption record for the findings above, added after the review:

| Finding | Status | Evidence |
| --- | --- | --- |
| R1 unsigned reconstructed-depth sampler/filter | **FIXED** | `backend_review_after_fix.log`: unsigned sampler on unit 2, R32UI, integer filtering NEAREST; 8/8 dispatch PASS |
| R2 final-image reconstruction boundary | **OPEN** | bridge still submits after the pack final pass |
| R3 scene depth/screen-unit grid | **OPEN** | main depth and view/screen uniforms remain native-sized |
| R4 camera float precision | **FIXED (source); not packaged** | positions now cross JNI as double arrays and subtract before float delta; coordinated APK+JAR run pending |
| R5 jitter/depth parameters/TAA ownership | **OPEN** | jitter zero; near/far/FOV hardcoded; pack final TAA still active |
| R6 initialization state | **FIXED (source)** | `SessionStateGuard` in `mg_fsr.cpp`; wrapper-level device test NOT RUN |
| R7 camera continuity | **FIXED (source)** | `keep_previous` no longer overwritten; no dedicated device run |
| R8 ABI field/capability validation | **OPEN** | motion scale/validity/generation and caps remain narrower than ABI |
| R9 damage-swap route | **FIXED** | `presentSurfaceWithDamage` treats FSR3 as an upscaling route |
| R10 steady-state cost | **PARTIAL** | per-frame bridge file logging removed in source; state queries/clears/barriers remain; not packaged/benchmarked |

Do not use this review to claim performance upscaling. `docs/fsr3/audits/2026-10-20-review-fix-evidence.md` contains the fix evidence.

## Path comparison

| Path | Actual current behavior | Review conclusion |
| --- | --- | --- |
| Renderer only, `FSR3Direct.cpp` | Processes framebuffer 0 at swap, downsamples its completed image, uses constant depth and zero motion, resets history every frame | Diagnostic execution of the algorithm. No scene-work reduction; includes any already-composed HUD |
| Current Iris bridge | Scales selected Iris targets, runs the original native final pass, downsamples that finished color to the quality extent, extracts depth and computes camera motion, then reconstructs to native | Experimental integration. Smaller FSR inputs now exist, but the pass boundary, pack coordinates and temporal ownership remain incomplete |
| Refined hybrid | Bridge supplies explicit scene inputs, stage identity, resolution policy and temporal metadata; MobileGlues reconstructs through its existing GLES provider | Recommended delivery route, subject to the corrections and gates below |
| Strictly no game integration | Launcher or renderer can potentially establish a smaller whole-game framebuffer and spatially upscale it; native HUD and reliable temporal semantics need separate solutions | A bounded spatial compatibility path is credible. General FSR3 temporal support cannot be promised from the final image alone |

A bridge delivered as a mod can avoid maintaining a fork of Minecraft or Iris.
Its mixins still alter the game rendering pipeline at runtime. These are
different requirements from a strictly renderer-only installation.

The shader port, source pin, native ABI, persistent history resources, truthful
shader compile/link checks and separation from frame generation are useful work
to retain. Current FSR1 source also contains the corrected two-pass EASU-to-RCAS
flow and output-first sizing; the old plan's FSR1 findings describe its audited
baseline, not all of current HEAD. FSR1's complete reduced-scene integration
still needs qualification.

## Findings

### R1 — P1: the reconstructed-depth integer sampler is never assigned

`MobileGlues-cpp/upscaling/fsr3/fsr3_backend.cpp:504-519` registers only
`GL_SAMPLER_2D`. PrepareReactivity declares an active `usampler2D` for
`r_reconstructed_previous_nearest_depth`
(`fsr3_shaders.h:2470,2599-2601`). It therefore retains sampler unit 0 instead
of receiving the R32UI reconstructed-depth texture.

The live host trace confirms that both the floating-point motion sampler and
the unsigned reconstructed-depth sampler use unit 0, whose texture is
**RGBA16F motion**. PrepareReactivity consequently lacks its correctly typed
depth input for disocclusion handling. Mesa still reports successful dispatch:
absence of a GL error is insufficient evidence here.

Handle floating, signed and unsigned sampler types explicitly, assign distinct
units as required, and validate each reflected resource against its actual
format. Also correct integer texture sampling policy: `create_textures`
currently gives every texture `GL_LINEAR` filtering, including R32UI
(`fsr3_backend.cpp:294-299,335`). Integer sampled textures need a compatible
nearest-filter policy. Verify depth-edge/disocclusion patterns, not just flat
color output. The filtering constraint is specified in the
[OpenGL ES 3.2 texture-completeness rules](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf).

### R2 — P1: the bridge reconstructs a resampled final image

`FinalPassRendererMixin.java:30-35` submits at `renderFinalPass` RETURN.
Iris' final renderer writes the main color target and restores the main FBO.
The updated `mg_fsr_jni.cpp:454-459` now derives smaller input dimensions
from the quality scale, and lines 483-488 downsample the completed native
image. On the successful depth/motion path, lines 491-518 submit those
smaller inputs with independent native output dimensions.

This corrects the earlier native-sized FSR dispatch, but the handoff still
comes after the pack's final AA and finishing passes. Low-resolution scene
samples have already been filtered into the native image and are then filtered
back down. This adds a native finishing pass and copy, loses the original
samples and can misalign final color with scene depth when finishing applies
warping or distortion. It is not the required temporal-reconstruction boundary.

Submit the selected pre-reconstruction scene texture directly, with its actual
render extent and an independently obtained output extent. Install the output
as the input to the remaining finishing passes. For the already-audited
Complementary profile, review the proposed `composite6` replacement and its
`colortex2`/FXAA dependencies. Other packs require their own proven boundary.

The renderer-only alternative has the related performance problem:
`FSR3Direct.cpp:258-264` reduces the completed native frame, after the expensive
rendering has occurred. Changing that path's quality preset cannot reduce the
work already performed by Minecraft.

### R3 — P1: scene scale does not own depth and screen coordinates coherently

`RenderTargetsMixin.java:21-40` changes dimensions but not the external main
depth texture supplied to Iris. Iris' `RenderTargets` retains that texture as
`currentDepthTexture`; it creates its own pre-hand and pre-translucent depth
textures at the modified dimensions. The main target and its depth allocation
remain at the original size.

Meanwhile `ViewportUniforms` still obtains `viewWidth`/`viewHeight` from the
main target, confirmed in the pinned JAR. Shrinking the viewport therefore
does not fix shader pixel/UV calculations. Pack `texelFetch`, depth sampling,
screen-space effects and compute dispatches can refer to different grids.
The new motion/depth pass extracts the lower-left render-sized depth rectangle
into R32F for FSR. That improves the handoff if the actual scene rasterization
uses that rectangle, but does not correct the pack's earlier depth consumers
or screen-size uniforms. The active rectangle remains inferred from dimensions
rather than supplied by the producer.

Define explicit render-sized scene color/depth, auxiliary depth copies,
uniforms, compute extents and active rectangles. Keep output-sized finishing
and HUD targets separate. Audit all consumers of each target, including
ping-pong histories, blits, mipmaps and copies. Preserve independent shadow
maps, voxel volumes and fixed-size resources.

`FSR3AdjustViewport` recognizes targets by size and color attachment 0.
It does not establish that a target is a world target, does not cover depth-only
FBOs, and leaves partial viewports/scissors unchanged. Replace this heuristic
as the authority with explicit target roles from the hybrid contract.

### R4 — P2: camera translation loses precision before the delta is computed

Camera translation is now included. However, `Fsr3Bridge.java:84-94` reads
Iris' unshifted double-precision world position and casts each component to
float. `mg_fsr_jni.cpp:285-287` then subtracts those float positions. At world
coordinate 1,048,576, float spacing is 0.125 blocks: a movement of 0.03 blocks
can become zero, followed by a jump in a later frame. Static-world motion
becomes quantized even when the game itself retains precise camera motion.
This finding follows from the data types and has not been reproduced on X1E.

Subtract current/previous double positions before conversion, or use a bounded
origin-relative representation with a documented shift convention. Iris'
own camera tracker already maintains origin shifts. Validate pure translation
at large coordinates separately from rotation and origin changes.

Motion coverage is also still camera-only. Previous object transforms and
deformation data are absent for independently moving entities, particles,
foliage, water and Create geometry. Hands and sky need their own conventions.
The matrix tests do not establish that these content categories reproject
correctly.

### R5 — P1: jitter, depth parameters and the pack's TAA remain uncoordinated

The hybrid descriptor is zero-initialized and never sets `jitter_x/y`.
It hardcodes near/far/FOV to `0.1`, `1000`, and 60 degrees
(`mg_fsr_jni.cpp:504-534`), even though actual projection/FOV can differ.
The bridge leaves the pack's final TAA and any pack-specific partial jitter
active. The existing Complementary audit already distinguishes its partial
vertex jitter from scene-wide jitter.

Choose one scene-wide jitter owner before rendering, supply the exact applied
offset, derive the real projection/depth parameters, and integrate at the
profile's final temporal-resolve boundary. Preserve independently required
reflection/lighting histories. Color encoding, exposure, hand/sky treatment,
alpha, reactive and transparency-mask policies also need explicit ownership.
Masks are optional API inputs; their omission still needs a quality assessment
for translucent and animated content.

AMD's documented temporal inputs and jitter/TAA guidance support these
requirements; the exact Minecraft integration follows from the inspected
pipeline. [AMD upscaler guide](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/)

### R6 — P1: initialization and the surrounding bridge do not preserve GL state

`mg_fsr.cpp:223-235` initializes/resizes before entering the backend dispatch
guard. `Backend::initialize` allocates textures, creates programs and uploads
constants without an outer guard. The host test seeded a program, texture and
UBO binding; initialization changed **2 → 0**, **1 → 0**, and **1 → 0**
respectively. A subsequent dispatch guard can only preserve those already
altered values.

There are additional holes around the same injected operation:

- `FSR3Direct.cpp:242-256` constructs `PresentGuard` after allocation.
- `mg_fsr_jni.cpp:312-346` alters texture bindings and uploads CPU buffers
  without isolating pixel-unpack state.
- `MotionGuard` now restores image units 0 and 1 and invalidates the wrapper
  image-binding shadow, but the motion operation still inherits the caller's
  sampler on texture unit 0 (`mg_fsr_jni.cpp:223-308`).
- Presentation blits do not disable/restore a caller's scissor test.

Guard the entire entry/initialization/resize/dispatch/presentation operation,
including failure exits, and coordinate raw GLES state with wrapper shadows.
Establish safe unpack-buffer/row-layout state for CPU uploads. Add adversarial
caller-state tests covering first use, resize, masks, sampler objects, image
bindings, unpack buffers and injected failure.

### R7 — P1: history and resources are not owned by the complete render stream

Pipeline construction/destruction now calls camera invalidation, and reflection
failure invalidates the camera too. These are useful corrections.

The new time-gap and teleport checks nevertheless do not work when the camera
is valid: `mg_fsr_jni.cpp:405-413` sets `has_previous = false`, but the same
capture unconditionally restores it to `true` at line 417. Consequently a gap
over 250 ms or a displacement over 64 blocks retains the previous frame and
does not request the intended history reset. Compute one continuity decision
and preserve it through capture and dispatch.

The remaining ownership gaps are constant surface/resource-generation IDs
(`mg_fsr_jni.cpp:510-511`), process-global camera history (`:69-81`) and
context key 1 when the wrapper does not track the context (`:430`). Pipeline
replacement alone does not identify every perspective/FOV, surface or context
transition.

`mg_fsr3_forget_context` cleans the direct-present map, not the separate JNI
map. `destroy_state` also omits `motion_program` deletion. The direct-present
skip flag is process-global, so one stream can consume another stream's flag.

Use provider/context/surface generations and per-stream camera/frame state.
Connect lifecycle callbacks to both native paths; retire GL objects in their
owning context and clear CPU bookkeeping after context loss. On input or
backend failure, coordinate an explicit fallback with resolution and pack-AA
ownership. Do not merely mark the bridge failed and let swap-time synthetic
FSR process the HUD.

### R8 — P2: accepted ABI fields are not consistently implemented

The ABI exposes `motion_scale_x/y`, validity flags, alpha policy, resource
formats/generations and jitter inclusion. Current behavior is narrower:

- `fsr3_backend.cpp:902-905` ignores the supplied motion scale factors.
- It computes jitter-cancellation constants, but the checked-in shader's
  `LoadInputMotionVector` does not use them (`fsr3_shaders.h:148-154`).
- Validation checks texture existence/target but not actual sampled extent,
  format, validity flags or every declared generation (`mg_fsr.cpp:138-179`).
- `query_caps` advertises FSR3 from function-pointer presence, while programs
  require ESSL 3.20, specific formats and image units through binding 29.
  Limits and successful compilation are not established by that mask.
- A raw pass error can return false without a useful pass-specific error string.

Reject unsupported combinations or implement them explicitly. Publish a
qualified capability subset, validate finite camera/time/exposure data, and
test actual descriptor-to-shader behavior. The current ABI test checks data
layout/value preservation, including a negative motion scale, without
dispatching it.

AMD's dispatch structure distinguishes render/output extents, motion scale,
jitter and camera parameters.
[AMD dispatch description](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/)

### R9 — P2: damage swaps bypass the FSR3 present route

`egl.cpp:431-435` tests only `fsr1_setting`. Selecting FSR3 disables FSR1,
so an available KHR/EXT damage-swap function goes directly to the provider.
It bypasses `ApplyFSR3Present` and does not consume the bridge-present flag.
This makes behavior depend on which legal EGL presentation entry point the
launcher uses.

Route every supported swap variant through the same FSR3 presentation policy,
with appropriate full-surface damage whenever the upscaler writes the whole
surface. Verify ordinary, KHR and EXT paths.

### R10 — P2: avoidable steady-state cost undermines the performance goal

`Fsr3Bridge.mark` opens/appends a file and writes stderr. The final hook calls
it every frame even when FSR3 is disabled; an enabled submit adds another
per-frame call (`Fsr3Bridge.java:112-117,194-198`;
`FinalPassRendererMixin.java:30-35`). Camera capture also performs reflection
lookups and allocates arrays each frame.

The backend queries/restores 32 texture units, 32 image units and 16 indexed
UBO bindings per dispatch, independently of the bindings actually used.
Every pass uses `GL_ALL_BARRIER_BITS`; numerous intermediates are cleared
through FBOs each frame. The current bridge also incurs a native-to-render
color blit before reconstruction and a render-to-native reconstruction output.

Make logging opt-in and bounded, cache bridge accessors, use reusable frame
storage, and make Off exit before per-frame work. After correctness, measure
and reduce state-query traffic, copies, clears and barriers using resource
dependencies. Do not weaken synchronization by assumption.

Calculated raw texture storage from current allocation code, excluding driver
padding, game resources, programs and allocation overhead:

| FSR extents at a 2880 × 1800 output | Backend textures | Current-style bridge auxiliaries |
| --- | ---: | ---: |
| NativeAA input/output | about 483 MiB | about 79 MiB |
| Quality input 1920 × 1200 | about 281 MiB | about 35 MiB |
| Performance input 1440 × 900 | about 210 MiB | about 20 MiB |

These are allocation arithmetic, not measured physical memory or bandwidth.
The corrected input sizing reduces backend storage at Quality and Performance.
Use the actual EGL/game output extent; the recorded 2880 × 1585 window differs
from the panel's 2880 × 1800. Format widening is a significant optimization
constraint to measure alongside reconstruction cost.

### R11 — P1: motion/depth failure creates a mismatched input packet

If `run_camera_motion` fails, `mg_fsr_jni.cpp:491-518` switches color, depth
and the descriptor's render dimensions back to the native main target. It
continues to submit `s.motion`, which was allocated at `rw × rh`. At any
upscaling preset those dimensions differ. Backend motion reads can therefore
address outside the provided motion image. Resetting temporal history does
not make this resource packet valid.

There is a second failure detail at lines 307-308: `motion_active` is set from
the previous-frame flag before the final GL-error check returns false. A GPU
error can therefore both select the mismatched packet and incorrectly retain
the temporal history.

Treat failed motion/depth preparation as a coordinated fallback, or provide
valid, matching inputs with a forced reset. Do not continue dispatch with a
different color/depth grid and an unchanged motion texture. These are source
findings; this review did not inject these failures on the target device.

## Recommended hybrid boundary

The bridge should provide semantic ownership of a frame, while MobileGlues
retains implementation of the algorithm and its GPU resources:

1. **Before scene rendering:** negotiate mode and actual render/output extents,
   establish the qualified profile and one jitter owner, and process reset
   events. If the backend is unavailable, restore a coherent native/spatial
   configuration before rendering that frame.
2. **During scene rendering:** maintain matched color/depth/auxiliary targets,
   screen-space uniforms and dispatch regions. Preserve independent fixed-size
   effects. Supply or produce current-to-previous motion with explicit coverage.
   Apply the qualified scene-texture LOD bias without changing unrelated
   shadow, lookup, volume or UI samplers.
3. **At the selected temporal boundary:** submit actual color/depth/motion
   textures, camera position/projection, jitter, exposure/masks where used,
   timestamps, context/surface/resource identities and reset reason.
4. **Inside MobileGlues:** validate the contract, execute the GLES backend,
   preserve caller state, return the output texture and actual mode/status.
5. **After reconstruction:** run compatible finishing passes at display
   resolution and compose native-resolution HUD. Record CPU and GPU timings
   without steady-state readback or device-wide waits.

For static-world bring-up, camera rotation **and translation** are a bounded
first gate. Independently moving objects need a separate gate and either
previous transforms/deformation or a qualified motion buffer. Optical-flow
estimates, global reactive masks and zero motion should not silently be treated
as complete object-motion evidence.

Keep original ZIP archives unchanged. A versioned runtime pack transformation
or integration profile is still a distinct configuration whose effects must
be qualified. Avoid matching all packs through a single final-pass heuristic.
Do not globally disable TAA-related defines when they also select effect
histories or other rendering behavior.

## Refinements to the research and execution records

The attached plan already contains many of the right requirements: real scene
scaling, explicit temporal inputs, native HUD, same-provider compute and matched
performance comparisons. The larger issue is that the implementation does not
yet satisfy them.

- Foreground the 1.21.1 NeoForge delivery lane in the plan's retained scope.
  Its opening still emphasizes 26.3 and 1.20.1.
- Define renderer-only, camera-only hybrid and fully qualified hybrid as
  different availability levels. A bridge must own resolution and pass
  placement as well as transport motion data.
- Reconcile current state with dated evidence. `execution-state.md` says no
  FSR3 installation exists while `hybrid-bridge.md` records a device run.
  Decisions D-FSR-014/015 describe the earlier no-scaling/downsample path;
  D-FSR-016 still describes a 250 ms suppression window, while code uses a
  consumed flag. Preserve old observations with their revisions instead of
  silently treating them as current.
- Make generated shaders reproducible from a clean checkout. The advertised
  `make_shaders.sh` generates GLSL files, but does not regenerate the embedded
  header and all its variants. Pin generator tools and verify embedded hashes.
- Retain the actual AMD copyright/MIT notice with generated derivatives and
  distribution artifacts. The shader header says a notice exists in
  `docs/fsr3/audits`, but this review found a description of the license there,
  not the full notice.
- Narrow the bridge's advertised Iris version range. Its mixins depend on
  specific method signatures, but metadata currently permits `[1.8.8,)`.
- Distinguish same-port host/device parity from an independent algorithm
  reference. Shared shader/host mistakes can pass both sides.

Existing `hybrid-bridge.md` reports **13 FPS enabled versus 26 FPS disabled**
for an earlier native-scene build. This is a recorded adverse observation,
not a new benchmark or a result for current HEAD. Preserve its exact package,
pack, settings and capture identities.

## Suggested order and acceptance gates

These are review recommendations for the existing ledger, not completed work:

| Order | Work | Evidence required before advancing |
| --- | --- | --- |
| 1 | Fix typed resource binding, first-use/resize state preservation and precise backend errors | Adversarial GL-state tests; nonuniform depth/disocclusion inputs; resource-binding inspection |
| 2 | Establish a real reduced-scene hybrid contract using bilinear/FSR1 first | Per-pass extent/uniform/depth trace; uncropped world; native HUD; all requested effects preserved |
| 3 | Move FSR3 to the qualified boundary and establish jitter/projection/motion | Motion visualizations for static camera, translation, rotation and depth edges; actual low input → native output |
| 4 | Add moving-object/mask coverage and lifecycle/fallback behavior | Entity/hand/particle/foliage/water/Create sequences; world switch, teleport, reload, resize, context/surface loss |
| 5 | Compare against the pinned independent reference and target device | Multi-frame image comparisons with justified tolerances, resets and odd extents; exact core/JAR/provider identity |
| 6 | Optimize and qualify performance | Native, same-input bilinear, FSR1 and FSR3; median/p95/p99 frame time, CPU/GPU breakdown, memory, power/thermal sustained runs |

At Quality, scene pixel count is approximately 44% of native; this does not
predict a 2.25× frame-rate gain. CPU work, shadows/voxel work, reconstruction,
copies and presentation remain. Retain the plan's provisional 20% median
frame-time improvement target and tail-time guard as acceptance goals, then
calibrate against repeated-run variance.

The most useful next implementation milestone is a **correct, measured
low-resolution hybrid scene path** with explicit inputs. More quality presets
or successful dispatch counters do not close that gate.

## Primary references checked

- [AMD SDK v1.1.4 release](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4)
  identifies the pinned FSR3.1.4 source generation.
- [Iris viewport uniforms at the inspected revision](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/uniforms/ViewportUniforms.java)
  show the main-target size ownership; relevant behavior was also checked in
  the pinned JAR.
- [Iris final renderer at the inspected revision](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/pipeline/FinalPassRenderer.java)
  shows final composition and restoration of the main target.
- [Iris target allocation at the inspected revision](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/targets/RenderTargets.java)
  distinguishes externally supplied main depth from owned depth copies.
- [Iris camera uniforms at the inspected revision](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/uniforms/CameraUniforms.java)
  expose current/previous camera position separately from the matrices.
