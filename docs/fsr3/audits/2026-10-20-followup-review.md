# FSR3 follow-up review after the first in-game test

Reviewed on 20 October 2026, starting from
`d72acc0fc83589515b398fb454e54a5941ce7101`. This review prepares the next
implementation agent; it does not implement or install product changes.
The source was being edited concurrently. Corrections and evidence through
`5d7ac39643e68b85dc77c35515cdea70485a19d2` were reconciled below; subsequent
edits need a fresh check. Baseline line references are to `d72acc0` unless
another revision is stated.
Evidence identities are retained in
[`2026-10-20-followup-evidence/`](2026-10-20-followup-evidence/).

The next milestone is a shader-enabled world rendered at a real reduced
resolution, reconstructed before finishing/UI, with consistent depth, motion,
jitter and resource lifetimes. The current post-final bridge does not meet
that contract. Fixing isolated artifacts without establishing that boundary
will leave related failures in other menus, packs and camera motions.

## User observations and required behavior

The user reports incorrect scaling of the main menu, HUD and other overlays,
plus flickering/glitching of the world. **Shaders must remain enabled.**
Treat these as open integration regressions. Their exact package, pack,
settings and reproduction sequence still need to be attached to a device
record; the request does not identify a new exact binary tuple.

The saved successful run used `AuroraLumen-Mobile-v2.zip`, not Complementary.
It showed a full world image and native-size HUD in one capture, before the
review fixes. That observation does not close the broader UI regression.
Use that pack first to reproduce the reported behavior, then qualify the
original Complementary assets required by the project.

## What the existing evidence establishes

- The native backend and optional Java/JNI helper exist and can execute through
  the same GLES provider. Keep this architecture.
- The recorded game path submits 1920 × 1057 and outputs 2880 × 1585. This
  establishes bridge input/output dimensions, not independently measured
  dimensions of every original world draw or shaderpack resource.
- The integer reconstructed-depth sampler and integer filtering have a
  source correction and a host trace showing the right R32UI binding.
- Camera positions now remain double precision until the relative delta is
  calculated. This needs a coordinated core/helper/JAR package.
- Damage swaps now enter the common upscaling presentation route.
- A wrapper initialization-state guard exists, but it does not cover the
  entire injected operation or establish every safe upload/sampling state.
- Later records verify first-use/resize preservation of texture, UBO, FBO and
  viewport state, and activation of the paired double-position camera path.
  These are narrower than full bridge-state and temporal-quality acceptance.
- All 11 host semantic executables passed again. They do not execute the
  complete Java/JNI path or test shader-enabled GUI composition.

The X1E reset/history/reset probe is a flat-input smoke test. Repeating the
same implementation and obtaining identical PASS/status JSON is not an
independent FSR algorithm reference or proof of temporal image convergence.
The existing `.json` capture also includes initialization text before its
JSON object. Preserve the raw capture and produce a separate machine-readable
result when improving the harness.

## Findings that determine the next work

### F01 P1: menu and fallback frames can enter the diagnostic post-HUD path

At `d72acc0`, `MobileGlues-cpp/gl/FSR3Direct.cpp:247-287` runs whenever FSR3 Experimental
is enabled and the consumed bridge-present flag is absent. A menu frame,
loading transition or failed bridge submission need not have a bridge
submission, so it can enter this diagnostic whole-frame reconstruction.
The path has no explicit world/profile/UI eligibility and consumes the
already-composed framebuffer.

This is a concrete routing defect and a candidate contributor to the user's
UI report, not proof of the cause of every overlay artifact. The viewport
heuristic, global cached native extent and missing state isolation are other
paths to investigate with a trace.

**Concurrent mitigation:** `5d7ac39` adds a global bridge-mode flag, set by
`nativeAvailable`, that suppresses direct present after bridge detection.
It also restores a native viewport at framebuffer binds. The new
[overlay record](../validation/2026-10-20-x1e-ui-overlay.md) observes a centered
pause menu, native HUD and aligned hand, but explicitly does not revalidate
the FSR setting. This is useful limited evidence, not closure of the full
shader-enabled UI matrix. The global flag is not a per-frame/per-stream route
contract, and the new viewport heuristic has the F08 defect below.

**Required:** choose the render route before the frame. Menus and unsupported
frames stay native. A valid shader-enabled world may use the negotiated scene
route. A failed temporal submission must use the frame's prepared fallback,
never silently activate synthetic FSR over the final UI. Make the experimental
direct path an explicit developer diagnostic, separate from product FSR3.

### F02 P1: scene resolution and the reconstruction point remain inconsistent

`RenderTargetsMixin.java` changes Iris dimensions, but the externally supplied
main depth and screen-size uniforms still have separate ownership.
`FinalPassRendererMixin.java` submits after `renderFinalPass`.
`mg_fsr_jni.cpp:487-524` downsamples that completed image and infers the
depth rectangle.

**Concurrent work:** `5d7ac39` adds `ViewportUniformsMixin` and a final-pass
flag; its record says that JAR was not installed for the overlay capture.
A `SceneDepthMixin` was also being drafted in the working tree. Recheck their
integration before editing them. Neither observation establishes a complete
scene resource graph or changes the post-final FSR boundary.

**Required:** explicitly identify scene color/depth, auxiliary depth copies,
history resources, per-pass viewport/scissor/uniforms and compute extents.
Consume the original scene texture at the selected temporal-resolve pass.
Bind native-size output to later finishing/UI. A logged requested width is
insufficient; record actual allocations and active rectangles after creation
and each generation change. Keep shadow maps, LUTs and voxel volumes on their
own grids.

### F03 P1: temporal semantics remain incomplete

The bridge still supplies zero jitter and hardcoded near/far/FOV, while the
pack's final TAA remains active. Current motion covers camera reprojection,
not independent object transforms/deformation. The backend ignores
`motion_scale_x/y`, and the generated motion-load permutation does not apply
the cancellation constant computed by the host.

**Required:** a single jitter owner, real projection metadata, explicit
current-to-previous motion convention, a compatible shader permutation and
qualified content coverage. Diagnose stationary-camera moving objects as
well as moving-camera terrain. Do not use a fully reactive image or perpetual
history resets to claim that ghosting is solved.

See the original review's R2/R3/R5/R8 and the
[implementation notes](../implementation-notes.md).

### F04 P1: state isolation and failure packets are still incomplete

`SessionStateGuard` in `mg_fsr.cpp` saves/restores unpack state but does not
unbind a caller's pixel-unpack buffer or establish safe row/skip state before
CPU-address uploads. `make_aux` is outside that guard. `MotionGuard` inherits
the caller's sampler. Presentation blits retain the caller's scissor state.
The direct-present guard is still constructed after its resource allocation.

`mg_fsr_jni.cpp:495-520` also falls back to native color/depth dimensions on
motion/depth preparation failure while still providing the smaller motion
texture. `motion_active` can be set before the GL-error result is known.
This is the original R11 and remains open.

**Required:** guard the complete operation and normalize its private state;
validate resource dimensions/formats before dispatch; retain application GL
errors through the wrapper's error mechanism. The backend's raw
`while (GLES.glGetError() != GL_NO_ERROR) {}` consumes existing provider errors
without preserving them for the caller. Internal diagnostics and application
errors need distinct handling.

### F05 P1: JNI compatibility is not negotiated

The precision fix changed the camera argument from `jfloatArray` to
`jdoubleArray`, while the exported `nativeCamera` symbol name stayed the same.
The helper resolves that symbol with `dlsym` and `init` only requires the
available/submit pointers. A mixed old-core/new-helper or new-core/old-helper
installation can call the wrong JNI array accessor through a function pointer.

**Required:** a versioned bridge handshake before camera or submit calls,
explicit required capabilities and a paired core/helper/JAR manifest. Preserve
the C frame ABI's declared versioning; the JNI protocol needs its own version.
A mismatch must leave shaders operating in the native route. Narrow the
advertised Iris compatibility range to tested signatures.

### F06 P1: a continuity fix regressed startup, then received a concurrent correction

At committed `d72acc0`, `keep_previous = valid && has_previous` can never
change the initial false `has_previous` to true. A host test compiling the
exact extracted camera-state/capture source observed:

```text
capture=1 valid=1 has_previous=0 expected_previous=0
capture=2 valid=1 has_previous=0 expected_previous=1
capture=3 valid=1 has_previous=0 expected_previous=1
continuity_contract=FAIL
```

A concurrent edit changed that decision to `valid && !time_gap`. Repeating
the same host check then produced `0,1,1` and PASS. This corrects the startup
case at source level. It was committed in `41a58f7`; a later
[paired-package device record](../validation/2026-10-20-x1e-r7-camera.md)
reports camera motion active at frame 3. The host harness itself substitutes
JNI array transport and does not run a JVM, Android or the GPU. The device
record does not prove large-coordinate motion quality or the complete reset
matrix.

Keep a regression test for first/second capture, cut/reset recovery,
teleport, pause/resume, invalid input, matrix changes and large coordinates.
Camera, sessions, native extents and bridge-present status still need ownership
per context/surface/profile generation. Pipeline replacement alone does not
provide that lifecycle model.

### F07 P2: timing needs validity checks and a nonblocking collection path

Recalculating the three complete eight-pass rows in the saved raw log gives
**17.667, 23.597 and 18.636 ms**, with median **18.636 ms**. The original
17–18 ms summary understated the second row; a concurrent documentation
correction now records the rounded totals.

These are sums of recorded timestamps, not validated total upscaler cost.
The timing code at `fsr3_backend.cpp:963-1010` checks function pointers but
does not record extension support, timestamp counter width or
`GL_GPU_DISJOINT_EXT`. It allocates queries and calls `glFinish` every timed
dispatch. Timestamp boundaries also omit initialization, earlier clears,
bridge preparation, copies and state restoration. Three dispatches with
reset/history/reset are not a warmed steady-state sample.

**Required:** a reusable asynchronous query ring, valid disjoint intervals,
whole-operation timing as well as per-pass timing, and separate CPU/frame
interval measurements. Keep blocking diagnostic timing out of game benchmark
builds. These requirements follow the
[Khronos timer-query specification](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt).
PrepareReactivity and Accumulate are useful initial investigation targets,
but their apparent cost must be remeasured under that protocol.

### F08 P1: the new native-viewport heuristic overwrites legitimate offscreen viewports

In `5d7ac39`, `FSR3Direct.cpp:217-230` treats any color texture at least as
large as the native surface as a native display target. It then replaces
the requested viewport with the full native extent. This includes a 4096²
color shadow target on a 2880 × 1585 surface, and deliberate subviewports
on a native-size texture. The rule has no producer/pass identity.

Compiling the exact committed `FSR3AdjustViewport` function with substituted
GL query results reproduces both failures:

```text
4096 color shadow target: adjusted=1 viewport=0,0,2880,1585 preserve=FAIL
native target deliberate subviewport: adjusted=1 viewport=0,0,2880,1585 preserve=FAIL
```

This is a source-level host reproduction, not an observed device shadow
failure. The existing `FSR3PlanViewport` tests exercise the pure planner,
not this new early-return branch, so their PASS does not cover it.

**Required:** identify scene/presentation transitions explicitly and restore
the intended viewport there. Preserve arbitrary application viewports on
other targets. Add tests through the real adjustment/bind path for shadow
targets, same-size offscreen targets and intentional subviewports. The GUI
fix must not change shaderpack rendering semantics.

## What should be preserved

Retain pinned AMD SDK v1.1.4 / FSR3.1.4, the same-provider GLES approach,
the native ABI, corrected typed binding, existing capability probes, and the
source audit of Complementary's `composite6` boundary. Preserve every original
pack ZIP byte-for-byte. Implement versioned runtime profiles in the adapter
and qualify their enabled effects.

The already-recorded final-pass downsample path is useful as a bring-up
diagnostic, not the destination architecture. Shader-enabled native fallback
must remain usable throughout development.

## Immediate agent assignment

Start with the main prompt in [agent-prompts.md](../agent-prompts.md).
The first implementation checkpoint is: negotiated bridge package, explicit
world/UI routing, tested continuity/failure/state behavior, and an observable
scene resource graph. Then establish the reduced scene and temporal boundary,
add motion/jitter/profile support, and qualify performance.

Use [task-ledger.md](../task-ledger.md) for live status. Reconcile its existing
R6/R7 and “independent” evidence wording against this review; do not create
another competing status ledger. The user's new UI/world reports remain open
until replayed with the exact new package and shaders enabled.
