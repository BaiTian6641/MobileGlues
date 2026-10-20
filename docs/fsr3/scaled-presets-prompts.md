# Next phase prompts for scaled preset overlay correction

Use these prompts in order with the implementation agent. The immediate goal
is to remove the hand/light-shadow overlay from **Quality, Balanced,
Performance and Ultra Performance**, preserving the confirmed NativeAA visual
baseline. Shaders stay enabled. FSR history stays off and motion stays zero.
Temporal reconstruction is a later phase.

These prompts continue the existing
[scaled-path design](design/artifact-free-scaled-final-pass.md) and
[execution state](execution-state.md). The
[diagnostic contract](design/scaled-presets-diagnostic-contract.md) supplies
the trace fields, source corrections, decision rules and preset gates.
Use the existing task ledger; do not create another live status table.

The reviewed checkpoint is `fb0bd162278847e00ba7a88d6d1fcd249ea1e034`.
The recorded NativeAA package is APK
`538455b765ec680f52919ecd4e296513353108a539352626ba4f45b9c43a0c39`,
core `5733b7bb70ddd7cfb2eb6ae790b8969512f1fa23021fa96e5b7c835245278f72`,
bridge `1f759cfeca1f3091bf74befe366121e7cf080fe4b940e2696d3c7006460c5cad`.
Those identities come from the saved record, not a new device inspection.

## Prompt 1 Prepare Experiment A

Paste this first. It authorizes host implementation/build work for a bounded
diagnostic package. It does not authorize installing or operating the device.

```text
Work in /home/polar/MobileGlues. Prepare Experiment A for the scaled-preset
hand/light-shadow overlay, starting from the latest source and evidence.

Read AGENTS.md, docs/fsr3/execution-state.md,
docs/fsr3/validation/2026-10-20-x1e-nativeaa-final-pass.md,
docs/fsr3/design/artifact-free-scaled-final-pass.md and
docs/fsr3/design/scaled-presets-diagnostic-contract.md.
Read the existing follow-up review for the still-relevant viewport/state
risks, but use the newer NativeAA record for the current baseline.

The user confirms NativeAA has no translucent hand/light-shadow overlay and
stars stay world-locked. Keep that baseline. This phase covers the remaining
presets with shaders ON, history OFF, motion ZERO and RCAS 0.4 unchanged.
MG_FSR3_HISTORY must be absent, not set to "0": the current code tests presence
and caches the result. Do not enable jitter or camera/hand/sky reprojection.
Do not assume this AuroraLumen variant has the older pack's TAA or pass graph.

First record HEAD, dirty files and the exact loaded-pack identity available
in the records. Identify missing pack/hash/profile facts for the capture
manifest. Review the pinned Iris source and verify hook descriptors against
the delivery JAR. Preserve unrelated edits and original shaderpack archives.

Implement an opt-in, bounded debug trace without changing target routing:
- allocation/resize identities and observed attachment extents;
- pre-hand depth-copy entry/exit, selected copy strategy and dirty-copy path;
- solid/translucent hand entry, actual draw/batch state, and return;
- pre-translucent depth copy and actual world/sky draw state;
- final pass entry, actual program/draw and return; pre/post bridge submit;
- HUD entry and any actual shadow/light passes used by this profile.
Record both read/draw FBOs, attachment kinds/names/generations/mips/layers/
formats/extents, viewport/scissor, draw buffers, program/pass role and actual
uploaded dimension/sampler values. Keep an ordered frame/event identifier.
Unavailable data must be marked unknown, never synthesized.

Hand return is required but insufficient: shader clear() can rebind main
after drawing. Capture where the hand actually writes. An early return with
no hand draw must be recorded as skipped.
noHand is depthtex2; noTranslucents is depthtex1. Main depth is externally
supplied. A complete FBO or requested allocation size does not prove matching
scene/depth ownership.

The trace must preserve GL/provider and wrapper state and application errors.
Do not add per-draw file I/O, glFinish, or pixel readbacks. Use bounded capture,
explicit overflow reporting and a debug-only export path. Trace-disabled
builds must have no diagnostic traversal/output.

Run the appropriate host suite and focused tests on the real trace/hooks.
Build a paired core/helper/JAR/APK diagnostic tuple, inspect mixin targets,
record hashes and keep the confirmed NativeAA artifacts recoverable.
Do not install, launch, change settings, or issue device commands in this step.

Deliver the concrete host diff, build/test results, package manifest,
instructions for one user-operated Balanced capture, expected events and
rollback instructions. Stop at that device gate. The trace may reveal a
mismatch; Experiment A passes when the capture is trustworthy, not when the
render path is already correct. Do not implement speculative ownership fixes
before inspecting the run.
```

## Prompt 2 Diagnose the captured run

Use after the user returns the Experiment A log and observations.

```text
Analyze the returned Experiment A capture using
docs/fsr3/design/scaled-presets-diagnostic-contract.md.
Read current source first and match the capture's exact binary, bridge, pack,
profile, requested/effective preset and output dimensions. Do not operate the
device or assume the recorded run authorizes a second run.

Verify history off, zero motion, shaders on, no lost events, real Balanced
scene dimensions and the expected solid/translucent hand draw coverage.
If a hand path was skipped or a field is unknown, limit the conclusion.
Distinguish actual draw bindings from bindings left behind by shader cleanup.

Find the first producer/consumer mismatch in the ordered frame:
world/hand color and depth; depthtex2 pre-hand snapshot; depthtex1
pre-translucent snapshot; copy rectangles; sampling units/coordinates;
viewport/scissor; final/display uniform domain; shadow/light fixed resources.
Check the FSR3AdjustViewport large-target branch against the known regression.
Separate Minecraft lightmap/overlay textures, screen-space light effects and
actual shadow maps. Do not assume the pack creates a shadow pass.

Return a compact before/after event table, the narrowest supported causal
hypothesis, any alternatives the evidence does not exclude, and a concrete
Experiment B patch plan with exact hooks, resource lifetimes and regression
tests. If only a return-time FBO was captured, do not call that draw ownership.

NativeAA working is evidence of the baseline and a scale-dependent defect;
it does not alone prove the exact failing bind or exclude shader coordinate
math. If capture is insufficient, identify the single missing observation and
prepare the bounded host trace refinement. Keep device collection pending.
```

## Prompt 3 Implement Experiment B and any justified Experiment C

Use once the trace supports a specific correction. This authorizes host
implementation and packaging; device execution remains separately scheduled.

```text
Implement the trace-supported scaled-scene correction from Experiment A.
Use the latest source and the two scaled-path design documents. Preserve the
confirmed NativeAA baseline and keep shaders ON, history OFF, motion ZERO.

Make world and hand write one explicitly owned scene color/depth extent.
Preserve the pack's existing hand projection and depth-range semantics.
Do not draw a second native hand after FSR and do not draw into the noHand
snapshot. Keep depthtex2 pre-hand and depthtex1 pre-translucent. If the scene
depth owner changes, update all depth readers, copies, framebuffer attachments,
stencil requirements and allocation-generation paths together.

Fix the first proven ownership/coordinate error, not the last visible symptom.
Use explicit pass/target roles. Preserve unrelated native/offscreen targets,
fixed shadow/light resources and intentional subviewports. Do not force every
target larger than the window to the window viewport.
Make scene viewports/scissors/uniforms match real draw extents; keep final
and HUD at display resolution. Scope final-pass state with exception-safe,
balanced lifetime and correct per-program uniform updates.

For the final-pass-only profile, trace any Iris fallback shaders used for real
world/hand draws. Their existence prevents assuming that no pack gbuffer
program means no scene FBO. Prove the actual reduced work.
Keep the current post-final, history-off dispatch as a temporary baseline
unless the trace demonstrates that this boundary itself must change.
A necessary boundary change requires an explicit color/depth/finishing
contract and before/after capture; do not silently start temporal integration.

If hand ownership is corrected but light/shadow overlay remains, perform
Experiment C: follow the exact light/depth producer and sampler into the
affected draw. Inspect coordinate domains, depth snapshots, sampler bindings,
blend/depth/stencil state and shadow viewport overrides. Do not suppress the
overlay with clears, disabled lighting, hidden hands, fully reactive images,
or extra history resets.

Implement NativeAA fallback as a synchronized effective preset used by Java
allocation/uniform logic and native dispatch. The current default is not proof
of automatic per-profile failure fallback. Check invariants before drawing;
on a late failure invalidate the frame/resources and transition coherently.
Never change only the native scale while Java still uses scaled resources.
Keep unqualified presets opt-in and expose requested versus effective mode
and the fallback reason in diagnostics.

Test real wrapper/bridge paths for scene and shadow targets, subviewports,
read-only binds, first-use/steady depth copies, final-scope failure, resize,
resource-name reuse and fallback. Run required host/build checks.
Deliver a paired candidate, manifest, NativeAA regression checklist and the
next user-operated Balanced test. Do not install or interact with the device
without explicit authorization for that session. Keep each causal change
reviewable and preserve unrelated work.
```

## Prompt 4 Qualify every remaining preset

Use after the corrected Balanced run meets the structural and visual gates.

```text
Qualify the corrected implementation across all remaining FSR preset levels,
using docs/fsr3/design/scaled-presets-diagnostic-contract.md. Preserve NativeAA
as the known visual baseline. Use one exact paired package, pack and profile.

Prepare the host validation tools and run sheets first. Coordinate each
device session with the user; do not treat earlier authorization as permission
for unscheduled installs/settings changes. The user operates the game unless
they explicitly delegate that operation.

Recheck NativeAA, then corrected Balanced, Quality, Performance and Ultra
Performance. Balanced is the first diagnosis case, not proof of other ratios.
Use actual output dimensions, shared rounding rules and observed allocations.
At 2880x1585 the current arithmetic predicts:
Quality 1920x1057; Balanced 1694x932; Performance 1440x793;
Ultra Performance 960x528. These are expected sizes, not acceptance evidence.

For each preset require: one scene/hand color path; correct depth snapshots;
no translucent duplicate hand; no light/shadow overlay; world-locked stars
during rotation; native centered HUD/menus; unchanged enabled shader effects.
Exercise empty/opaque/translucent held items, hand swing, night sky, daylight
and actual lighting effects, GUI scale, odd window size, resize, shader reload,
return to title, and preset transitions. Keep N/A/untested cases explicit.

Validate requested/effective preset and safe NativeAA fallback for failed
invariants or unsupported profiles. A visually clean native fallback does not
pass the requested scaled preset. An input-size log after final downsampling
does not prove world draws ran at reduced resolution.

Use bounded debug captures for structure, then the trace-disabled release
candidate for visual confirmation and matched frame-time measurements.
Keep shaders, RCAS, scene/settings and thermal conditions comparable.
Report real performance separately; do not label history-off reconstruction
as temporal FSR3 qualification or count an FPS overlay as a benchmark.

Record each preset's exact evidence in the existing execution state and task
ledger. Keep failed presets experimental/unavailable with NativeAA fallback.
Do not promote every preset because Balanced passed. Provide a remaining
defect list and a separate future handoff for temporal integration.
```

## Prompt 5 Independent checkpoint review

```text
Review this scaled-overlay checkpoint without implementing fixes.
Use the current source, exact diff, artifact/pack identities and captured
events, plus docs/fsr3/design/scaled-presets-diagnostic-contract.md.

Check that the real world and hand draws share the intended scene extent,
depthtex2 stays pre-hand, depthtex1 stays pre-translucent, and final/UI and
fixed shadow/light targets retain their own domains. Verify trace neutrality,
actual uploaded uniforms, per-program caching, fallback consistency and
release-build trace removal. Check the wrapper's large-target viewport rule.

Reject a PASS obtained by falling back silently to NativeAA, disabling
shaders/effects, adding another hand, clearing away lighting, or enabling
temporal history/motion. Separate host, debug-device and release-device
evidence, and distinguish each preset from the others.

Return prioritized findings with exact locations, the gate supported by each
record, and the next executable correction. NativeAA remains the rollback
baseline. The full temporal FSR3 roadmap stays deferred until this phase is
qualified and explicitly resumed.
```
