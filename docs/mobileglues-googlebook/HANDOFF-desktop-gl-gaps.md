# Handoff: desktop-GL feature completion for MobileGlues

Continuation on 2026-10-21: see
[desktop GL, bridge and CI execution](2026-10-21-desktop-gl-bridge-ci.md) and
[independent review](2026-10-21-review.md). Later FSR3 findings are in
[universal rendering research](2026-10-21-fsr3-universal-rendering-research.md) and
[depth contract and runtime status](2026-10-21-fsr3-depth-contract-and-runtime-status.md),
which retain the `2.0.5-dev` / bridge `0.2.2` 26.2 rendering failure, later
2.0.6/bridge 0.2.3 CI, the 2.0.7 full-library error failure, interrupted control
and the verified 2.0.8 CI artifacts and physical buffer pass with remaining
game gates. The
[buffer mapping checkpoint](2026-10-21-buffer-mapping-and-rendering-checkpoint.md)
records the separately reviewed S4.9 corrections and their host/provider
limits. These records supersede the immediate-state table and next-action list below. The original handoff is retained as the
baseline; it is not a description of the latest branch or artifacts.

## Current continuation checkpoint

The delivery branch is `codex/desktop-gl-universal-fsr`. Current corrective
commits are buffer mapping `ed85188`, viewport/scissor `e3ab06c`, buffer errors
`9549931`, durable probe tooling `c02565e`, and 2.0.8-dev packaging `ab4d31d`.
The exact revisions and evidence are linked in the runtime and buffer records
above. The latest error correction passed independent review and 187 focused
ASan/UBSan checks. Its completed old-source comparison failed 38 assertions;
the corrected source failed none. Native CI [39578250534](https://github.com/BaiTian6641/MobileGlues/actions/runs/39578250534)
succeeded at exact `ab4d31d`; artifact `12335463401` and the locally signed
release derivative are independently verified. The runtime record keeps the
CI unstripped input, stripped payload, diagnostic APK and physically tested
local core hashes separate.

The permanent bridge checkout is clean on `codex/resource-pack-metadata` at
[`2f0e4faf57489c51418bfa9fe534d63f7623a769`](https://github.com/BaiTian6641/MobileGlues-FSR3-Bridge/commit/2f0e4faf57489c51418bfa9fe534d63f7623a769).
Advance it to the next reviewed bridge commit when ready, then pin the matching
0.2.4 profile JAR hashes and CI/source evidence before runtime qualification.
The pending version is not yet a verified artifact set.

The physical system provider passes the expanded five-case buffer suite.
MobileGlues 2.0.7 passed the three visibility cases but failed invalid-flag
reporting; that overall result remains FAIL. The corrected 2.0.8 full library
subsequently passed all five expanded cases with its exact core hash and
`src-beaba336ea4452da` marker. A separate run of the exact ARM64 core extracted
from the verified CI APK also passed all five cases, with its distinct core
hash and runner evidence retained. These results do not resolve the last recorded
26.2 terrain/HUD failure or supplies the interrupted FSR-disabled comparison.
Bridge 0.2.3 has verified CI artifacts for eight exact adapter profiles; physical acceptance is
still profile-specific. The device-owner's latest check still requires user
unlock before game work.

Next actions, using the current artifact records rather than the original
2.0.1 instructions retained below:

1. Finish independent verification of the pending bridge depth-generation
   correction and its exact profile JARs. Native 2.0.8 CI verification is
   complete; use the recorded artifact hashes and source marker. Preserve the
   completed five-case physical core pass, and rerun the isolated harness when
   a native change or artifact mismatch requires it.
2. After user unlock, verify the selected launcher profile and that no Minecraft
   process is running before changing a test artifact. Preserve world data,
   original pack archives and backed-up settings. Confirm the new marker from
   the game process before interpreting its rendering.
3. Complete the 26.2 FSR-disabled control and an enabled comparison using the
   same pack/settings. Bridge 0.2.3 can trace terrain phases with FSR disabled;
   absent `FINAL_DRAW`/`SUBMIT_RESULT` roles are expected in that control.
   Capture terrain and HUD behavior, then save and exit normally. The prior
   uncontrolled restart has no established cause; it is not a completed control.
4. Verify quality-switch and NativeAA depth lifetime for the corrected legacy
   bridge adapters. Prepare/verify the exact requested profile locks in the
   matrix below, then test each independently with the reviewed 0.2.4 bridge.
   Preserve completed profile results while adding new evidence; 1.18.2 stays
   at build/static evidence until it receives its own runtime session.
5. Continue the general desktop-GL gaps and effect/lifecycle gates from their
   evidence rows. Retain explicit provider limits and the separate unqualified
   FP16 experiment; do not infer full temporal FSR3, universal compatibility,
   sustained performance or X2E support from the bounded probe results.

| Requested runtime profile | Next qualification step after unlock and artifact verification |
| --- | --- |
| 1.20.1 Forge / Create Delight Remake | Retest the actual modpack and Oculus 1.8.0 with the new bridge, including the original Complementary and Steadfast archives and quality/NativeAA transitions. Preserve the earlier 0.2.1 visual results as their own evidence. |
| 1.20.1 Fabric | Prepare an isolated profile using the exact pinned Iris/Sodium/bridge tuple, then verify world, pack effects and quality transitions. Preparation is not runtime acceptance. |
| 1.21.1 NeoForge 21.1.262 | Retest the supported delivery tuple with its exact Iris 1.8.14-beta.1/Sodium 0.8.13 lock, including effects, quality transitions and save/exit. |
| 1.21.1 Fabric | Verify the isolated profile and exact adapter/mod lock, then run its own world/pack and lifecycle checks. |
| 26.2 NeoForge | Finish the FSR-disabled versus enabled terrain/HUD comparison with matching pack/settings and recorded phase diagnostics; retain the existing failure. |
| 26.2 Fabric | Verify isolated preparation and the exact pinned tuple, then run a separate terrain/HUD and FSR comparison. A NeoForge result does not qualify it. |
| 1.18.2 Forge and Fabric | Retain the verified build/static adapter checks; runtime qualification is NOT RUN until separately scheduled and executed. |

## Original handoff baseline

Date: 2026-10-21. Target: Minecraft 1.21.1 / NeoForge 21.1.262 / Iris
1.8.14-beta.1 / Sodium 0.8.13, HP Googlebook 14 (`C1L63708PZ`), Snapdragon
X1E80100 / Adreno X1-85, ANGLE 2.1 over Vulkan 1.4.295, GLES 3.2.

This handoff is for continuing the **general MobileGlues desktop-GL feature
completion**, not for adding more shaderpack-specific adaptations. The current
pack is accepted as the working regression target.

## Immediate state

| Item | Value |
|---|---|
| Main repo | `/home/polar/MobileGlues`, branch `googlebook/x1e-stage1-3`, HEAD `ad79230` |
| Main repo working tree | clean except user `.gitignore` and untracked `.vscode/` |
| Plugin source | `/home/polar/MobileGlues-plugin` (separate repo/worktree), dirty |
| Plugin APK | `/home/polar/MobileGlues-plugin/app/build/outputs/apk/debug/app-debug.apk`, SHA-256 `005958b8…d832` |
| Plugin package | `com.fcl.plugin.mobileglues`, version 2.0.1, debug-signed, arm64 only |
| Plugin packaging | `useLegacyPackaging = true`; native lib must extract to `/data/app/.../lib/arm64/libmobileglues.so` |
| Bridge JAR | `integrations/minecraft/fsr3-bridge/dist/mgfsr3bridge-0.1.0.jar`, SHA-256 `4d49e135…0684` |
| Device | `C1L63708PZ`; ComplementaryReimagined_r5.9.3 active; shaders enabled; debug options off; shadow distance 32 |
| Latest device run | Complementary, night, 44 FPS at 2880×1585, no compile errors |
| Capture | `/tmp/current-device.png`, SHA-256 `baa02e6c…fae25` |

## What is already implemented

| Commit | Change |
|---|---|
| `98058e3` | Explicit `r8ui`/`r16ui`→`r32ui`, `rg32ui`→`rgba32ui`; RGBA32UI backing |
| `0d3b890` | Host regression cases for those qualifiers |
| `2bcdf10` | Late global `uniform` hoisting in core shader source |
| `ad79230` | Validation record for host tests + current-pack device regression |

Host suite: `sh MobileGlues-cpp/tests/run.sh` passes, including
`compat_image`, `image_metadata`, subgroup, and FSR contract tests.

## Critical caveat: verify which core is loaded

FCL appears to cache the renderer in `/data/user/10/com.tungsten.fcl/app_runtime_mod`.
During the Bliss investigation the bridge ran, but no newly added core shader
marker appeared, and the driver error was unchanged. Reinstalling the plugin
and reselecting the renderer was attempted, but this was not conclusively
verified.

Before trusting any device result for the new image lowering:

1. Add a **core build marker** that is visible from the game process, e.g. a
   `GL_RENDERER`/extension suffix or a one-shot file/log from a known new code
   path.
2. Reselect MobileGlues in FCL, or clear the renderer cache, then relaunch.
3. Verify the marker, then run the format probe.

Do not claim device support for `98058e3` until this is resolved. The last
current-pack run only proves regression, not that the new image path executed.

## Missing work, in order

The full evidence and exact designs are in
`docs/mobileglues-googlebook/desktop-gl-gap-closure.md` (G0–G14). Summary:

### P0 — verify the current build and close the source-order failure

- **G0:** the late-uniform hoist has a host implementation but **no dedicated
  host regression test** and no device-qualified result. If Bliss is used only
  as a probe, add a host test for `mg_hoist_late_global_uniforms` and re-run
  its original default S0. Do **not** add another blind regex: dump the exact
  driver-bound source if it still fails.
- Verify FCL is loading the 2.0.1 core.

### P1 — legal image-format table

- Complete `r8/r16/r16f→r32f`, `r8i/r16i→r32i` qualifier lowering.
- Widen `RG32F` image storage to `RGBA32F`; add a test for storage/binding size
  compatibility.
- Add `RGBA8_SNORM` and the remaining legal integer formats to the immutable
  image allowlist.
- Fix `mg_image_unit_format` returning non-Table-8.27 `GL_RGBA16` for a
  data-backed RGBA16 image; null/storage RGBA16 already lowers to RGBA32F.
- Run a device wrapper probe that compiles, binds, writes, and reads each
  format; host qualifier tests are not device evidence.

### P1 — probes for existing advanced paths

- Image atomics: 2D/3D, fragment/compute/geometry, with readback and
  `glMemoryBarrier`. Expected to work through ES 3.2 / provider
  `GL_OES_shader_image_atomic`, but unverified.
- Subgroup lowering: device semantics for virtual 32-lane IDs, inclusive add,
  quad swaps, SPD and XOR-partition duplicate suppression. Host tests pass;
  residual constructs must fail loudly.
- Vertex/geometry multi-image capacity and atomics.

### P2 — optional missing desktop features

- **1D textures/images:** virtualize as 2D `width×1`; rewrite `image1D` to
  `image2D`, wrap image coordinates, and add clear/query/copy paths. Image-only
  phase first; `sampler1D` must fail loudly until implemented. Exact design is
  G2 in the closure doc.
- Mutable multi-level 2D/3D images: either report an explicit error for level>0
  on wrapper-created immutable storage, or allocate the full chain.
- Default tessellation levels: explicit `GL_INVALID_OPERATION` today; only
  synthesize a TCS if a concrete pack needs it.

### P3 — hard provider/API boundaries

Keep explicit failures and document, do not fake:
- SSBO `MAX_SHADER_STORAGE_BLOCK_SIZE=134,217,728`; WSR 256 blocked.
- No texture barriers or texture views; no bindless/SSO.
- `clip_cull_distance=0`; glslang limits should be gated to 0 so failure happens
  at translation, not later at the driver.
- Default tessellation levels unavailable.

## Files and docs

| Path | Purpose |
|---|---|
| `desktop-gl-vs-mobileglues-gap.md` | Initial comparison |
| `desktop-gl-gap-closure.md` | **Authoritative** row-by-row verification, G0–G14 designs, probes |
| `shaderpack-support/audits/2026-10-20-bliss-static-audit.md` | Bliss feature trace |
| `shaderpack-support/audits/2026-10-20-solas-static-audit.md` | Solas images/compute |
| `shaderpack-support/audits/2026-10-20-iterationrp-static-audit.md` | iterationRP subgroups/images/SSBO |
| `feature-matrix.md` | Historical probe/status matrix; some rows are stale per closure doc |
| `execution-state.md` | Named deviations and stage state |
| `2026-10-21-image-lowering-and-current-pack-regression.md` | Latest host/device validation |

## Build and test commands

```sh
# Host tests
sh MobileGlues-cpp/tests/run.sh

# Core release build
cmake --build build/android-arm64-release -j 24

# Plugin APK (separate repo)
cd /home/polar/MobileGlues-plugin
sh gradlew :app:assembleDebug --no-daemon
adb -s C1L63708PZ install -r app/build/outputs/apk/debug/app-debug.apk

# Verify extracted native lib
APK=$(adb -s C1L63708PZ shell pm path com.fcl.plugin.mobileglues | sed 's/package://' | tr -d '\r')
adb -s C1L63708PZ shell "ls -l $(dirname "$APK")/lib/arm64/libmobileglues.so"
```

Current Iris config is in
`/storage/emulated/10/Android/data/com.tungsten.fcl/files/.minecraft/config/iris.properties`
and is edited through FCL's content provider, not shell filesystem access.

## Engineering constraints

- Do not modify original shaderpack archives.
- Do not add shaderpack-specific behavior to core just to make one pack pass.
- Do not raise advertised limits or add extensions without backend semantics.
- Keep compile/link failures observable; never mask them as success.
- Keep wrapper lowering, bridge source transforms, provider limits, and
  unverified behavior separate in claims.
- Preserve the user's `.gitignore`, `.vscode/`, and unrelated work.
- Device evidence needs a readback or a visible frame; compile-only is not a
  device pass.
- The plugin submodule is dirty with deliberate local changes (hoist, image
  lowering, 2.0.1 packaging). Decide and record whether it is committed before
  another session rewrites it.

## Recommended first action

Add the core build marker, confirm the 2.0.1 library is actually loaded, then
run one explicit `r16ui`/`r8ui`/`rg32ui` device image probe with readback. That
closes the only uncertain link between the host tests and the installed
renderer.
