# Prompts for completing MobileGlues FSR3

**Current phase:** use [scaled-presets-prompts.md](scaled-presets-prompts.md)
first. The confirmed NativeAA baseline and the remaining scaled hand/light
overlay require Experiments A/B/C with history off and zero motion. The prompts
below are the broader roadmap; their temporal steps are deferred during that
bounded phase.

Paste the main prompt into the implementation agent's chat. The later prompts
are focused continuations for that same agent, or assignments for agents the
user has explicitly chosen. They do not require automatic delegation.

The current task is implementation after review. The documents and examples
describe work to perform; they are not evidence that it has been performed.

## Main implementation prompt

```text
Work in /home/polar/MobileGlues and complete the shader-enabled FSR3 integration,
including its Minecraft/Iris bridge. Start by reviewing current source and
evidence, then implement and validate the required changes. Continue through
the dependency-ordered checkpoints; do not stop at a plan or a successful build.

Current prerequisite: read docs/fsr3/scaled-presets-prompts.md and the latest
execution state. While the scaled-overlay phase is open, execute its scoped
prompts first, preserving NativeAA, history off and zero motion. The temporal
and motion work below is a later roadmap, not authorization to bypass that
phase or its user-operated device gates.

The user reports:
1. Main menu, HUD and other overlays are not consistently scaled correctly.
2. World rendering flickers or glitches.
3. Shaders must stay enabled. Use a standard temporal-upscaler integration,
   including the necessary game-renderer changes, rather than final-image
   filtering as the production path.

Read these first:
- /home/polar/MobileGlues/AGENTS.md
- docs/01_MobileGlues_Googlebook_Architecture.md
- docs/02_MobileGlues_Googlebook_Implementation_Plan.md
- docs/03_MobileGlues_Googlebook_Validation_and_Sources.md
- docs/MobileGlues_FSR3_Research_and_Implementation_Plan.md
- docs/fsr3/agent-handoff.md
- docs/fsr3/audits/2026-10-20-followup-review.md
- docs/fsr3/implementation-notes.md and docs/fsr3/references.md
- docs/fsr3/execution-state.md, task-ledger.md, frame-contract.md and decisions.md
- docs/fsr3/audits/minecraft-iris-pass-input.md
- The game, backend-smoke and timing evidence under docs/fsr3/validation/.

Keep the delivery tuple: Minecraft 1.21.1, NeoForge 21.1.262, Java 21,
Iris 1.8.14-beta.1 and Sodium 0.8.13. Keep FSR3.1.4/SDK v1.1.4 pinned unless
a separately justified source update is necessary. Keep the existing GLES
provider/backend; no general Vulkan rewrite or frame generation in this task.

Re-read HEAD and dirty files. The follow-up review began at d72acc0 and
reconciled concurrent changes through 5d7ac39. Reproduce or verify findings before fixing
them. Do not reintroduce old sampler, camera precision or damage-swap defects.
Preserve unrelated changes. Keep the existing task ledger as the status source.

The production frame path must be:
negotiated shader-enabled reduced scene with matched color/depth/motion/jitter
-> FSR3 at the profile's temporal-resolve boundary
-> display-resolution finishing
-> native HUD/UI
-> one presentation.
Menus and unsupported profiles remain native with shaders preserved wherever
the game normally uses them. Missing/failed bridge submission must never
silently trigger synthetic post-HUD FSR. Keep diagnostic direct-present code
explicitly separate from product mode.

Implement in order:
1. Safe world/UI routing, bridge version handshake, coherent fallback,
   complete state/error isolation and tested camera continuity.
2. Real reduced world targets, matching depth copies, uniforms, viewports,
   scissors and compute extents. Prove this using bilinear/FSR1 first.
3. Explicit scene-resource handoff before the final temporal resolve; one
   jitter owner; real projection/depth/exposure metadata; correct motion units
   and shader permutations.
4. Independently moving/deforming content and versioned shaderpack profiles.
   Use AuroraLumen to reproduce the user's current symptoms, then qualify
   original Complementary Reimagined/Unbound r5.9.3 and r5.9.2 regression.
5. Valid nonblocking CPU/GPU measurement, targeted optimization, matched
   native/bilinear/FSR1/FSR3 benchmarks and sustained thermal/lifecycle tests.
6. A compatible, hash-pinned core/plugin/helper/JAR release candidate.

Do not disable shader effects, force every pixel reactive, keep resetting
history, or substitute spatial scaling to obtain an FSR3 quality/performance
pass. Camera-only remains experimental. A flat reset/history/reset probe and
identical PASS JSON from two runs are not independent algorithm validation.
Fix the frozen contract's documentation/implementation inconsistencies with
explicit compatibility handling; version any incompatible contract change.

Use the code examples as design references, not production-ready patches.
Create regression tests against real changed paths. Capture actual resource
extents/generations and debug motion/depth/jitter inputs before image tuning.
Preserve original shaderpack ZIPs byte-for-byte.

Device tests use adb -s C1L63708PZ and one device owner at a time. Do not
reboot, wipe, clear launcher data, change the graphics provider, or modify
system/vendor. Preserve worlds/settings and verify the loaded package tuple.
If device ownership/access is unavailable, complete independent host work
and report the exact pending device gate.

After each checkpoint, update the canonical records with source/build/host/
device evidence separately. Report changed files, revisions, commands, results,
remaining defects and the next executable step. Keep semantic, toolchain and
performance changes in separate reviewable commits and inspect the staged diff.
Finish only when the stated product gates pass; otherwise identify the concrete
remaining blocker without promoting partial evidence to completion.
```

## Continuation A: native UI and scene routing

```text
Implement checkpoint 1 of docs/fsr3/agent-handoff.md.
Read the latest follow-up review and current code before editing.

Own the world/UI route selection, bridge protocol handshake and coherent
failure policy. Trace FSR3Direct.cpp, FinalPassRendererMixin,
IrisRenderingPipelineMixin, RenderTargetsMixin, mg_fsr_jni.cpp and EGL swaps.
Identify whether the reported overlays belong to Minecraft or the launcher.

Remove product reliance on the absence of a bridge-present flag. UI-only and
transition frames must stay native, with zero FSR scene submissions. In-world
HUD, chat, inventory, pause menus, debug text, tooltips and container previews
must preserve GUI scale, clipping and input hitboxes. Test resize/DPI/window
changes and returning to title after a world.

Recheck the 5d7ac39 overlay mitigation and F08 reproduction: large color
targets and intentional subviewports must not be forced to display size.
Use explicit target/pass roles and test the real wrapper entry points.

Negotiate the JNI camera-array protocol and all required functions before
camera calls. Reject old/new core-helper mixtures without a JVM crash.
Make the render route a per-frame, per-context/surface/profile decision.
Do not infer world targets solely from texture size.

Add exact routing/state tests, event-driven traces and shader-enabled device
captures. Keep the production shaders active and the original pack unchanged.
Record remaining world/temporal failures separately.
```

## Continuation B: coherent reduced-resolution rendering

```text
Implement checkpoint 2 of docs/fsr3/agent-handoff.md after routing is stable.
Own the explicit world resource graph and its integration with Iris.

Replace size-only inference with target roles and resource generations.
Make color, main scene depth, pre-hand/pre-translucent depth copies, screen
uniforms, viewports/scissors, texture consumers and compute dispatches agree.
Keep the presentation target and UI native. Preserve shadow maps, voxel
volumes, lookup tables and pack-relative resolutions without double scaling.

Record actual GL allocations and active rectangles after creation and resize,
not just requested widths. Exercise 2880x1585, 1920x1057 input, odd extents,
scale changes and shader reloads. Acknowledge that render/output aspect ratios
may differ slightly from integer rounding and derive projection consistently.

Use bilinear/FSR1 as a diagnostic baseline with a valid spatial AA policy.
Preserve all enabled shader effects. Show that scene work really decreases,
that the image is not cropped, and that UI layout is unchanged. Update S4-B
only with that evidence. Then expose explicit resources to the temporal handoff.
```

## Continuation C: temporal bridge and shaderpack profiles

```text
Implement checkpoints 3 and 4 of docs/fsr3/agent-handoff.md.
Start from explicit reduced-scene resources; do not append FSR3 after final.

For the first Complementary LDR profile, use the audited composite6 boundary.
Replace final TAA, account for colortex2 and composite7 FXAA dependencies,
and feed display-size output into finishing. Preserve effect histories and
original archives. AuroraLumen needs its own audited boundary; do not assume
its pass names or color conventions match Complementary.

Supply actual projection/depth metadata and the applied jitter. Verify
current-to-previous motion signs, units, origin and jitter cancellation with
nonuniform fixtures. Use doubles for camera positions and compatible JNI
packaging. Test first/second frame, cuts, teleports and recovery.

Produce rigid object motion, then articulated/deforming geometry coverage,
including hands, entities, particles, water, foliage and compatible Create
content. The upstream at_velocity documentation describes newer game
versions and is not an existing 1.21.1 facility. Implement/qualify this lane's
producer explicitly. Add reactive/transparency policies where needed.

Capture moving clips and input debug views. Distinguish camera-only,
mixed-coverage and fully qualified profiles. Do not advertise full temporal
support based on a camera-only prototype.
```

## Continuation D: backend contract, correctness and optimization

```text
Own the MobileGlues backend and native API work described in the handoff.
Coordinate shared ABI/JNI changes with the bridge owner if separate.

Complete capability and descriptor validation, motion scaling/permutations,
GL state/error isolation, resource generation tracking, lifecycle cleanup and
failure recovery. Keep integer formats, typed samplers and barriers correct.
Fix the mismatched native-color/small-motion failure packet. Do not consume
application errors to make internal work appear successful.

Create a reproducible shader-generation pipeline pinned to AMD SDK v1.1.4,
compiler tools, permutations, port patches and generated hashes. Preserve
license notices. Compare nonuniform multi-frame sequences with an independent
pinned upstream reference, including nonzero motion, jitter, resets and odd
extents. Comparing the same port on host and device cannot exclude shared bugs.

Replace blocking timing with a reusable query ring and disjoint/counter-width
validation. Measure the entire operation plus individual passes. Then reduce
copies, unnecessary format widening, state queries, clears and barriers one
controlled experiment at a time. Do not trade correctness for unqualified
speed or introduce a second provider/backend without a demonstrated constraint.
```

## Continuation E: independent acceptance and performance

```text
Review and validate the exact packaged implementation using
docs/fsr3/agent-handoff.md and the acceptance matrix in implementation-notes.md.
Keep implementation claims and observations separate.

Verify loaded core/helper/JAR/pack hashes and actual provider/OS identity.
Reproduce the user's menu/HUD/overlay scaling and world flicker with shaders
enabled. Test static and moving cameras, independent objects, disocclusion,
hands, transparency, reloads, resizes and lifecycle transitions.

Reject performance comparisons made from unmatched F3 screenshots.
Run repeated warmed native, bilinear, FSR1 and FSR3 routes with controlled
settings/camera/content. Report median/p95/p99 real frame intervals, CPU/GPU
components, query validity, memory, power mode and thermal behavior.
Keep timing instrumentation overhead out of the claimed game performance.

An earlier recorded shader-enabled frame or flat backend smoke does not pass
these gates. Keep failed/unrun cases visible, preserve raw captures with hashes,
and update the canonical ledger only when each criterion is satisfied.
```

## Review prompt before accepting a checkpoint

```text
Review the current checkpoint without implementing fixes first. Read the
stated acceptance criteria, exact diff and evidence. Reproduce material
failures where practical. Check shader-enabled behavior, native UI, resource
identity, temporal correctness, fallback and benchmark comparability.

Return prioritized actionable findings with exact source locations, distinguish
source defects from device observations, and state which gate can or cannot
advance. Do not treat a successful build, flat-frame probe, log message or
illustrative example as product acceptance.
```
