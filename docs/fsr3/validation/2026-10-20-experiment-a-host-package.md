# Experiment A host package and capture gate

Status: **host package ready. Not installed, not run on device.**
Scope: bounded diagnostic for the scaled-preset hand/light-shadow overlay.
History remains off; motion remains zero; shaders remain enabled; RCAS 0.4.

## Host tuple

| Artifact | SHA-256 |
| --- | --- |
| Diagnostic APK | `42dabf4d…` |
| Diagnostic core | `6811d7f4…` |
| Diagnostic bridge JAR | `9d8b90ad…` |
| Embedded helper | `6e6f79fb…` |

Fixture paths:

- `/home/polar/MobileGlues-fixtures/experiment-a/MobileGlues-expA-diag.apk`
- `/home/polar/MobileGlues-fixtures/experiment-a/mgfsr3bridge-0.1.0.jar`

The confirmed NativeAA rollback pair is unchanged:

- APK `538455b7…`, core `5733b7bb…`, bridge `1f759cfe…`.
- Fixtures: `MobileGlues-scene-arm64.apk`, `mgfsr3bridge-0.1.0.jar`.

Source: `b22f5e5` plus user-authored dirty docs at receipt time. The JAR's
`trace-build.properties` records the source/status/diff receipt and the Iris
JAR hash.

## Native diagnostic

- Separate core build: `MG_FSR3_DIAGNOSTICS=ON`; release build remains `OFF`.
- `mg_diag_on_draw()` is inline no-op when the diagnostic macro is off. The
  release core has no `nativeTrace` or `mg_diag` symbols.
- Trace accumulates in memory; file write occurs once at a bounded boundary.
  No per-draw file I/O, `glFinish`, pixel readback, target routing, or shader
  changes.
- Snapshot fields include driver/tracked read and draw FBOs, draw buffers,
  viewport/scissor, blend/depth/stencil masks, program, color/depth/stencil
  attachment kind/name/target/level/layer/format/extent (unknown fields are
  `null`), `viewWidth`/`viewHeight`, and `depthtex0/1/2`, `colortex0`, `gcolor`
  sampler unit/texture/sampler bindings.
- The first actual top-level draw of each role is captured through
  `prepareForDraw`; later draws update a lightweight last-draw summary and the
  draw count. A shader `clear()` rebind therefore cannot hide the actual draw
  target, and an early hand return records zero draws as skipped.
- Bounds: 120 level frames, 3000 events, 12 MiB. Overflow is recorded and
  stops the capture; it is not a silent success.

## Mixin/version policy

- The 1.21.1 NeoForge tuple (Iris 1.8.14-beta.1) is the verification target.
- New trace hooks are optional (`require=0`). No Minecraft type is referenced
  by compiled diagnostic hooks except the HUD string descriptor.
- HUD hook uses the 1.21.1 descriptor
  `render(Lnet/minecraft/client/gui/GuiGraphics;Lnet/minecraft/client/DeltaTracker;)V`;
  it is unverified on host because no Minecraft client JAR is present. Iris
  `finalizeGameRendering` supplies the always-available `PRE_HUD` event.
- Shadow events exist only when the pack actually calls `ShadowRenderer`;
  absence is not proof that the pack has no shadow feature.
- Descriptor checks against the exact Iris JAR: PASS
  (`tools/verify_fsr3_hooks.sh`). Host checks: PASS. The HUD limitation is
  recorded in the manifest `hooks` list and missing HUD event.

## User-operated Balanced capture (separate authorization)

Do not install or change settings during this host step. When the user
authorizes a device session:

1. Retain the NativeAA rollback pair above.
2. Install the diagnostic APK and replace the mod JAR with the diagnostic JAR.
3. In the game working directory, create `mgfsr3-trace.arm` before launching.
   Trace output is `mgfsr3-trace.jsonl` in the same directory. The marker is
   deleted after a completed capture; recreate it to re-arm.
4. Keep shaders ON, FSR3 **Balanced**, resolution 2880×1585, RCAS 0.4, and
   ensure `MG_FSR3_HISTORY` is absent (not `"0"` or empty).
5. Launch the world and, within the 120-frame window:
   - show an opaque held item and swing it;
   - show a translucent held item if available, otherwise record missing;
   - rotate the camera across the night sky/stars;
   - open and close the pause menu.
6. Return `mgfsr3-trace.jsonl`, observations, crash/logcat text if any, and the
   active pack archive SHA-256 plus Iris profile name. Do not edit the JSONL.

Expected ordered events include `ALLOCATION`, `RESIZE` (if any),
`PRE_HAND_COPY_ENTRY/EXIT`, `BEGIN_HAND`, `HAND_SOLID_RETURN`,
`PRE_TRANSLUCENT_COPY_ENTRY/EXIT`, `BEGIN_TRANSLUCENTS`,
`HAND_TRANSLUCENT_RETURN` (only for translucent items), selected
`PHASE_*_END` world/sky samples, `COMPOSITE_RETURN`/`SHADOW*` where present, `FINAL_ENTRY`, `FINAL_DRAW_RETURN`,
`FINAL_POST_SUBMIT`, `PRE_HUD`, and `HUD_ENTRY` if the optional hook matched.
`SHADOW*` appears only if the pack actually runs that pass.

Experiment A passes only when the capture is complete enough to identify the
first producer/consumer divergence; a visually clean or broken frame alone is
not a gate.

## Rollback

Restore the baseline APK/core and bridge JAR hashes above, remove
`mgfsr3-trace.arm`, force-stop and relaunch the launcher. No shaderpack bytes
are modified.
