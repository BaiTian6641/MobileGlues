# FSR3 bridge implementation notes and acceptance checks

The immediate phase is now
[scaled preset overlay correction](scaled-presets-prompts.md), with NativeAA
as the confirmed visual baseline, history off and motion zero. Use its
[diagnostic contract](design/scaled-presets-diagnostic-contract.md) first.
The temporal examples and long-term acceptance work below remain future work.

These are implementation instructions and original design examples for the
next agent. They are **not implemented features or device acceptance evidence**.
Use the [handoff](agent-handoff.md) for sequencing and the existing
[task ledger](task-ledger.md) for status. Source findings are in the
[follow-up review](audits/2026-10-20-followup-review.md).

## 1. Integrate a world render path, not just a data callback

The hybrid bridge has two responsibilities: supply temporal inputs and
control where reconstruction occurs. Adding camera matrices to a completed
frame cannot recover original subpixel scene samples or independent object
movement. The current final-pass downsample is useful for bring-up but needs
replacement as the product path.

AMD's integration guide calls for jittered scene input, depth and motion at
the temporal reconstruction boundary; its described shader interfaces do
not supply an OpenGL ES backend. MobileGlues owns qualification of its port.
[AMD integration guide](https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/).

| Responsibility | Bridge / Minecraft / Iris | MobileGlues |
| --- | --- | --- |
| Frame eligibility | World present, pack profile, stage, requested mode, output extent | Capabilities and accepted ABI/protocol |
| Resolution | Allocate and render the coherent scene graph; distinguish scene and display consumers | Validate actual resource metadata and supported views |
| Temporal inputs | Apply jitter; produce depth/motion/masks; identify projection, cuts, time and exposure | Validate and convert conventions exactly once; select compatible shader permutation |
| Reconstruction | Submit the original scene resources before finishing/UI | Run FSR3 and own GPU history/output |
| Finishing | Route the output to the remaining pack passes and native UI | Preserve application state and frontend GL semantics |
| Lifecycle | Notify world/profile/size/surface changes | Invalidate and retire resources in the correct context |

Do not choose world targets by size alone. A scene texture, menu blur,
reflection target and shadow attachment may share a size. Declare their roles.
Keep the main/UI target at presentation resolution. Reduced scene depth,
pre-hand/pre-translucent copies, screen uniforms, viewports, scissors and
compute extents must describe the same grid.

The pinned Iris implementation derives viewport uniforms from the main target,
while `RenderTargets` accepts externally owned depth. Changing only its
constructor dimensions cannot establish a coherent render graph.
[ViewportUniforms at the audited Iris revision](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/uniforms/ViewportUniforms.java),
[RenderTargets](https://github.com/IrisShaders/Iris/blob/eb7afb99f747cc8ed5ee4072119539035d33cefd/common/src/main/java/net/irisshaders/iris/targets/RenderTargets.java).

For every allocation/generation change, record role, producer/consumer pass,
frontend name, generation, logical/physical format, allocation extent,
active rectangle, viewport and screen-dimension uniforms. Log after resources
actually exist; a constructor request or a one-time call before allocation is
not proof. Trace compute group counts and rounded edges separately.

## 2. Route selection, native UI and fallback

Choose the route before changing the scene projection or targets. The following
excerpt is from the [standalone example](examples/bridge_contract_examples.cpp).
It assumes that protocol, profile and resources have already been validated:

```cpp
Route choose_route(const Eligibility& e) {
    return e.world && e.shaders && e.profile_qualified && e.protocol_ready &&
                   e.resources_ready && e.temporal_requested
               ? Route::SceneTemporal
               : Route::Native;
}
```

Native means the game's native render path with its selected shaderpack
preserved. It does not mean disabling shaders. A title/loading frame should
not accidentally inherit a world viewport or a stale bridge-present flag.
An in-world menu can retain the world reconstruction behind it while its own
GUI is composed at native size. Identify launcher-owned overlays separately:
they are outside the game render graph and need their own observation.

A failure after the reduced scene has rendered needs a prepared fallback.
Use a compatible spatial resolve of that frame, including a defined treatment
of the applied jitter, or rerender the native scene if the architecture allows
it. Select a valid native/spatial AA policy for the next frame. Reusing an old
FSR output or switching to post-HUD synthetic FSR is not an acceptable fallback.
Do not call a jittered, unresolved input “FSR1 baseline.”

Use a frame state machine such as `native`, `scene-prepared`,
`scene-submitted`, `resolved`, `fallback`, `presented`. Enforce one submission
at the negotiated stage and one presentation. Reset/retire state on stream
changes. An availability query should describe capability; a separate
negotiated lifecycle owns routing.

## 3. Close the native and JNI contracts together

Before extending [mg_fsr_abi.h](../../MobileGlues-cpp/upscaling/mg_fsr_abi.h),
reconcile its behavior with [frame-contract.md](frame-contract.md):

| Contract issue observed | Required decision |
| --- | --- |
| Masks/exposure described as optional but zero handles described as invalid | State which omissions are accepted, which neutral resources the backend creates, and their actual extents/formats |
| Motion called optional in prose but required by temporal dispatch | Require a valid motion field and report its coverage; zero motion is only valid where the scene is static |
| Caller-owned output documented but currently rejected | Advertise backend-owned output until caller-owned views are implemented and tested |
| Fields for motion scale, origin and jitter exist without complete implementation | Define conversion once and test the actual generated shader permutation |
| Nonzero jitter required on every temporal frame in prose | Validate the applied sequence and metadata; a valid sequence can contain a zero component |
| LDR described ambiguously as perceptual and sRGB-decoded | Define transfer function, primaries, sampling decode, tone-map stage, exposure and output encoding explicitly |
| Generic statement that all API functions return zero on success | Document pointer, void, string and integer-returning functions individually |
| Declared identities/generations versus constant JNI identities | Give every submitted name a real owning stream and allocation generation |

Freeze a supported subset only after descriptor validation rejects unsupported
cases. Version incompatible changes; do not reinterpret ABI v1 silently.
Document frontend-to-provider name translation and output-handle ownership.
The ABI's seconds and the upstream dispatch's milliseconds also need an
explicit conversion at their boundary.
[AMD dispatch fields](https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/).

The JNI bridge requires its own handshake. A same-named `dlsym` result does
not validate an argument change from `float[]` to `double[]`.
[JNI array-access functions](https://docs.oracle.com/en/java/javase/21/docs/specs/jni/functions.html).

Proposed interface sketch, **not an existing exported API**:

```cpp
// Use a new symbol whose signature is stable across future helpers.
struct BridgeInfo {
    uint32_t struct_size;
    uint32_t protocol_version; // Separate from MG_FSR_ABI_VERSION.
    uint64_t capabilities;     // e.g. double camera + explicit resources.
};
// Returns an error for unsupported versions/sizes; never partially enables.
extern "C" int mg_fsr_bridge_query_v2(BridgeInfo* info);
```

Resolve the query first. Missing symbol, unsupported version, missing required
entry point or failed capability check leaves the native shader-enabled route
active. Only then resolve/call versioned typed functions. The Java-to-helper
boundary must negotiate as well: an old helper cannot safely answer a newly
added Java native method unless the adapter catches the missing-method case
and falls back before camera submission. Test old/new core, helper and JAR
combinations, including failed initialization followed by another query.

Give the bridge build a protocol version and paired artifact manifest.
The current generic `0.1.0` artifact name and broad Iris dependency range do
not establish compatibility. Pin tested method descriptors and mixin injection
points; a successful class load is insufficient.

## 4. Camera history, motion and jitter

Track camera and GPU histories separately. A new valid camera sample can
become the predecessor of the next frame even when the current frame had to
reset. Do not gate this transition on an already-true `has_previous`.
Commit GPU history only after successful dispatch/output publication.

The example `can_reproject` checks the prior sample, stream generation,
monotonic time, finite values, explicit cuts and camera displacement:

```cpp
// Excerpt; see the standalone file for the complete function and checks.
if (!previous || !finite_sample(*previous) || !finite_sample(current) ||
    current.reset || !(previous->stream == current.stream))
    return false;
const double dt = current.seconds - previous->seconds;
if (!(dt > 0.0 && dt <= maximum_gap_seconds)) return false;
```

The example's 250 ms gap and 64-block teleport thresholds are illustrative
policies, not AMD requirements. The real producer also validates all matrices,
projection changes, frame identity and world/dimension changes. Keep camera
positions in doubles through subtraction. Avoid prematurely converting
billion-block positions to floats. Handle a singular inverse and a point
behind the previous camera without generating NaNs.

For a surface point with **unjittered** clip positions in both frames:

```glsl
// Mathematical example. Assumes GL bottom-left UV origin and +Y up.
// Producing prevClip correctly for the same moving surface is separate work.
vec2 currentNdc = currClip.xy / currClip.w;
vec2 previousNdc = prevClip.xy / prevClip.w;
vec2 motionPixels = (previousNdc - currentNdc) * 0.5 * renderExtent;
```

An object that moved two render pixels right has motion `(-2, 0)` in this
convention. Three pixels up gives `(0, -3)`. With a top-left texture convention
the Y conversion differs. Convert the active rectangle and sampling origin
consistently; do not copy a D3D Y flip into GLES by habit. The ABI currently
specifies top-left active rectangles, so the producer/backend conversion must
be explicit. Validate vertical asymmetric patterns and nonzero subrect origins.

Depth reprojection supplies motion only for geometry consistent with the
camera transform. Independently moving objects require previous object/model
transforms; articulated entities require previous poses; deforming geometry
requires previous deformation/time. Decide how hands, sky, clouds and particles
are represented at the selected boundary. Use masks for legitimately
unreprojectable changes, not to hide a missing motion producer.

Use one jitter sequence tied to accepted scene frames and the actual render/
output sizes. Apply it to the scene projection before drawing, and give
MobileGlues the exact applied offset. Keep UI and shadow projections outside
that policy. Remove duplicate or partial pack jitter at the profile boundary.
The pinned SDK supplies phase/offset helpers and derives jitter cancellation;
the GLSL callback applies cancellation only for the matching permutation.
[Pinned SDK host implementation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/sdk/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp),
[pinned GLSL callback](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/sdk/include/FidelityFX/gpu/fsr3upscaler/ffx_fsr3upscaler_callbacks_glsl.h).

Prefer unjittered motion with cancellation disabled for the first supported
profile. If a producer includes jitter, enable and verify cancellation once.
Record a permutation matrix for depth inversion/infinite far, motion
resolution, jitter inclusion, HDR/LDR, exposure and precision. Reject
unsupported combinations instead of accepting fields the shader ignores.
Scope scene texture LOD changes to the qualified profile; avoid changing
shadow, UI, LUT or voxel sampling globally.

## 5. Shaderpack profiles and the finishing boundary

Use the local [pass/input audit](audits/minecraft-iris-pass-input.md) as the
source of pack-specific findings. For the first Complementary r5.9.3/r5.9.2
LDR profile, the audited boundary is `composite6`, using the scene before its
final TAA resolve and writing display-resolution input for `composite7/final`.
Confirm the actual color encoding; an RGBA8 attachment alone does not identify
the transfer function. Do not disable all uses of a `TAA` macro: it can also
control effect behavior and jitter elsewhere.

The replaced pass previously updated `colortex2`, and the following FXAA
interaction can read it. Either supply a compatible replacement or explicitly
select and qualify the profile's nondependent `FXAA_TAA_INTERACTION` route.
Audit every reader of a removed history before deleting its writer. Preserve
lighting/reflection histories that are independent of the final AA history.

AuroraLumen needs its own resource, temporal and color audit. The reported game
run is not evidence that it shares Complementary's pass names. ACT voxel/
floodfill targets and WSR relative-resolution reflection histories must keep
their intended grids and effect semantics.

A versioned profile should record original ZIP SHA-256, pack version, settings
that affect the pass graph, matching source hashes, transformation version,
temporal boundary, expected inputs/outputs and capability requirements.
Apply transformations in memory or to a derived cache, preserving original
ZIPs byte-for-byte. Unknown source signatures should use native rendering
with a clear diagnostic, not a guessed shader rewrite.

SuperResolution is a useful example of explicit pack integration. Its current
version-3 documentation places `at_velocity` on a newer Minecraft lane; this
is not proof that Iris 1.8.14 on 1.21.1 exposes it. Its documented platform
support also excludes Android. Borrow design ideas after checking compatibility,
not an assumption that its mod can be dropped into this installation.
[Upstream interface guide](https://sr.187j3x1-114514.org/docs/Shaderpack-Interface-documentation/Version-3/),
[upstream platform documentation](https://github.com/IReallyWantToSleep/superresolution/blob/dev/docs/README_EN.md).

## 6. State isolation, error ownership and transactional failure

Construct the operation guard **before** allocation, shader compilation,
uploads, depth/motion preparation or framebuffer changes. Cover initialization,
resize, success and every error return. Restore provider state and MobileGlues'
tracked frontend state coherently.

This is a partial setup example inside that guard, not a complete guard:

```cpp
// Illustrative GLES setup before private CPU-address texture uploads.
GLES.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
GLES.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
GLES.glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
GLES.glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
GLES.glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
GLES.glActiveTexture(GL_TEXTURE0);
GLES.glBindSampler(0, 0); // Then set the private texture's required sampling.
// Disable/replace scissor deliberately before private full-image blits.
// Restore all saved state when leaving the complete operation.
```

Saving/restoring unpack state does not make it safe to use a CPU pointer while
the caller's unpack buffer is bound. A caller sampler can override texture
sampling parameters; integer textures require compatible sampling. These
are API semantics, not pack-specific exceptions.
[OpenGL ES 3.2 specification](https://registry.khronos.org/OpenGL/specs/es/3.2/es_spec_3.2.pdf).

Exercise hostile caller state: nonzero unpack PBO, row/skip settings,
comparison sampler, texture/image units, generic and indexed UBOs, unusual
viewport, enabled scissor, read/draw FBOs and pending application error.
Keep the wrapper's application error queue separate from internal diagnostic
errors. Draining provider errors without preservation violates that boundary.

If depth/motion preparation fails, reject the packet or prepare a complete
fallback packet. Never submit native color/depth with smaller motion under
native dimensions. Validate actual mip/layer/format/extents before dispatch.
Publish `motion_active` and the output only after successful validation.
Clear pending history on failure and make subsequent recovery deliberate.

The new first-use/resize device probes cover four state categories. Extend
them to the entire JNI/presentation operation and to context changes,
failure injection and Off. Those probe successes do not establish all the
state cases above.

## 7. Reliable measurement and a bounded optimization loop

The saved three dispatch rows sum to 17.667, 23.597 and 18.636 ms. They lack
disjoint/counter-width validity and omit bridge/copy/state cost. Do not use
them as a qualified game-speed prediction. Start with a timer implementation
that respects `EXT_disjoint_timer_query`.
[Khronos timer specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt).

Conceptual collection algorithm; implementation and tests are still required:

```text
initialize:
  verify extension, entry points and nonzero counter width for chosen target
  allocate a bounded query pool; centralize ownership of disjoint checks
submit:
  if a slot is free, timestamp operation/pass boundaries and tag its epoch
  if none is free, skip this measurement without stalling the render thread
poll on a later frame:
  read disjoint status; invalidate every affected pending interval/epoch
  poll result availability; read only completed queries
  accept only a valid ordered interval in one uninterrupted epoch
  recycle completed or invalidated slots safely
```

Do not nest incompatible elapsed queries with the application's own queries.
Handle counter wrap, context loss and partial dispatches. Keep `glFinish`,
busy waiting and CPU readback out of game benchmark instrumentation. Measure
CPU submission time separately from GPU timestamps and real frame intervals.
Record unavailable counters as unavailable, not zero.

Measure the full operation from bridge preparation through output publication
alongside pass times. State guards, copies, clears, query creation and JNI/file
overhead can dominate a nominal shader improvement. AGI system tracing can
help separate CPU scheduling, GPU work and presentation; first verify the
features available on this exact device/provider.
[Android GPU Inspector system profiler](https://developer.android.com/agi/sys-trace/system-profiler).

Use one controlled experiment per patch: remove a redundant copy, narrow a
verified format, reuse an allocation, reduce a proven redundant barrier, or
replace broad state queries with correct tracking. Re-run nonuniform image
reference tests for each change. Do not infer FP16/image/subgroup support
from a function pointer or a desktop extension. Preserve integer reconstructed
depth and its typed binding.

For algorithm validation, build an independent harness from the pinned AMD
upscaler on a supported reference API, outside the GLES port. Feed identical
nonuniform multi-frame inputs, normalize origin/encoding, capture intermediate
and final outputs, and define justified tolerances. A desktop reference is
reference evidence; it is not Android driver evidence. A second run of the
same GLES shaders cannot reveal errors shared by that port.

## 8. Acceptance matrix

Use shaders enabled for every game case, including comparison routes.
Keep failed/unrun cells open in the canonical ledger.

| Case | Stimulus | Evidence needed to pass |
| --- | --- | --- |
| Title/loading/UI-only | Launch, resize, load/leave world, change GUI scale | Native layout and hitboxes; zero scene submits; no cached world viewport |
| In-world GUI | HUD, F3, chat, inventory, pause, tooltip, container preview | Correct size/clipping and sharp text over the reconstructed world |
| Scene grid | Quality/Balanced/Performance, odd extents, resize | Actual color/depth/motion extents and uniforms agree; full uncropped scene |
| Static convergence | Fine fences, wires, foliage, text in world; static camera | Stable temporal sequence; no unexplained periodic resets or shimmer |
| Camera movement | Horizontal/vertical pan, rotation, large coordinates | Correct sign/scale; no depth-edge tearing; finite debug buffers |
| Independent movement | Static camera with entity, articulated model, hand, moving Create geometry | Correct local motion or a documented qualified coverage policy; no persistent trails |
| Deformation/transparency | Water, foliage, particles, texture animation, emissives | Profile-specific motion/masks; no broad full-frame reactivity workaround |
| Disocclusion/cuts | Occluder removal, teleport, FOV/perspective, dimension switch | History rejection/reset at the event and recovery on following valid frames |
| Pack profiles | AuroraLumen reproduction; original Complementary required versions/effects | Profile/hash match, preserved effects and independently audited temporal boundary |
| Protocol/failure | Mixed artifacts, shader failure, wrong size, stale handle, allocation failure | Safe native/prepared fallback, intact GL state, specific diagnostics, no crash |
| Lifecycle | Shader reload, mode change, fullscreen, background/resume, surface/context loss | No stale names/history; resources retired correctly; Off stays inactive |
| Performance | Repeated matched native/bilinear/FSR1/FSR3 routes | Valid timing, median/p95/p99, CPU/GPU costs, thermal and memory records |

For visual acceptance save motion clips and input debug views, not only a
single F3 screenshot. Compare selected regions against native and the pinned
reference; a global similarity score can hide HUD scaling or disocclusion
failures. Identify any launcher overlay separately from the game HUD.

Use a fixed world/save, camera route, pack/settings, render distance, simulation
distance, frame cap/VSync, output size and power mode. Warm shader caches and
reach a recorded thermal state. An initial protocol is at least three repeated
60-second captures per mode after warmup, rotating mode order; extend it if
variance/thermal drift prevents a useful comparison. This is a proposed
measurement protocol, not a completed run. Retain the project targets of
20% lower median frame time on a GPU-limited Quality route and at most 5%
tail degradation, subject to the baseline plan's variance calibration.
Report CPU-limited and sustained runs separately.

## 9. Build, reproduce and hand off

The existing host suite is:

```sh
sh MobileGlues-cpp/tests/run.sh
```

Build/run the standalone examples from the repository root:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic \
  docs/fsr3/examples/bridge_contract_examples.cpp \
  -o /tmp/mg-fsr3-contract-examples
/tmp/mg-fsr3-contract-examples
```

The examples check routing, continuity recovery and motion math. They omit
real render hooks, matrix capture, protocol transport, GL state, backend
dispatch and GPU history. Their PASS cannot advance a product gate.

For the authorized implementation phase, use the pinned ARM64 build:

```sh
MG_NDK="$HOME/Android/Sdk/ndk/28.2.13676358"
cmake -S MobileGlues-cpp -B build/android-arm64-release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$MG_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_static -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-arm64-release -j 24
```

Before running `integrations/minecraft/fsr3-bridge/build.sh`, resolve and
hash-pin the correct Iris and Mixin JARs, then pass `IRIS_JAR`, `MIXIN_JAR`
and `ANDROID_NDK_HOME` explicitly. Its defaults refer to temporary/external
workspace files and do not make a clean checkout reproducible. Its Java 17
bytecode target can run on the required Java 21 runtime; do not confuse the
compiler target with the game runtime requirement.

Make shader regeneration reproducible too: record AMD commit, compiler and
SPIRV-Cross versions, full commands/permutations, reviewed GLES changes and
hashes of both source and embedded header. Reproduce
`fsr3_shaders.h`, not only intermediate GLSL files. Preserve upstream notices.
[Pinned SDK release](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4),
[license](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/c6efa6bf7f2027b3ec94f28578bb5965eabb9e55/LICENSE.txt).

Qualify ELF alignment for both core and helper and the actual APK/JAR load
path. This project's recorded FCL extraction requirement still applies.
Android's 16 KB guidance explains compatibility requirements; it does not
replace launcher-specific loading validation.
[Android page-size documentation](https://developer.android.com/guide/practices/page-sizes).

Each device record needs source revisions, dirty diff hash, APK/core/helper/JAR
hashes, original pack hash/profile/settings, output and render extents, actual
OS/provider/driver, exact commands, raw capture hashes and stated gate result.
Keep host, reference, emulator and X1E evidence distinct. The review turn only
creates documentation and host evidence; deployment belongs to the subsequent
implementation task.
