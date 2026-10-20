# FSR3 execution state

Updated: 2026-10-20 (S0/S1 start). This file is the continuation record for the FSR3 lane. It
does not replace `docs/mobileglues-googlebook/execution-state.md`; the older file remains the
source of truth for the pre-FSR3 renderer/pack history.

## Repository identity

| Item | Value |
| --- | --- |
| Integration worktree | `/home/polar/MobileGlues` |
| Branch | `googlebook/x1e-stage1-3` |
| HEAD | `0a1e062`; installed pair APK `538455b7…`, core `5733b7bb…`, JAR `1f759cfe…`; NativeAA final-pass hand/star PASS |
| Origin | `git@github.com:BaiTian6641/MobileGlues.git` |
| Upstream | `https://github.com/MobileGL-Dev/MobileGlues.git` |
| Untouched user changes | modified `.gitignore`, `docs/mobileglues-googlebook/validation/README.md`; untracked `.vscode/`, FSR3 plan, recent validation docs |
| Worktrees | `/tmp/mg-fsr-core` → `googlebook/fsr-core-s1`; `/tmp/mg-fsr-backend` → `googlebook/fsr-backend-s2`; `/tmp/mg-fsr-game` → `googlebook/fsr-game-audit` |
| Existing unrelated worktree | `/tmp/mg-depth-port` detached at `b1ff643`; not part of this lane |

### Provenance

- Installed renderer: `com.fcl.plugin.mobileglues` **2.0.0**, APK SHA-256
  `538455b765ec680f52919ecd4e296513353108a539352626ba4f45b9c43a0c39`.
- Installed ARM64 core: `lib/arm64-v8a/libmobileglues.so`, SHA-256
  `5733b7bb70ddd7cfb2eb6ae790b8969512f1fa23021fa96e5b7c835245278f72`
  (history off, zero motion when history off).
- Installed bridge JAR: `mgfsr3bridge-0.1.0.jar`, SHA-256
  `1f759cfeca1f3091bf74befe366121e7cf080fe4b940e2696d3c7006460c5cad`
  (conditional scene scaling, final-pass-scoped viewport uniforms).
- Fixture copies match these installed hashes:
  `/home/polar/MobileGlues-fixtures/MobileGlues-scene-arm64.apk`,
  `/home/polar/MobileGlues-fixtures/mgfsr3bridge-0.1.0.jar`.
- The APK uses `extractNativeLibs=true`; direct `Android/data` replacement was
  performed through the FCL file manager after a scoped-storage helper placed
  the JAR in Downloads.

## Target tuple

- Device: HP Googlebook 14 `quartz`, serial `C1L63708PZ`, Snapdragon X1E80100 / Adreno X1-85.
- OS: Android 17 / API 37, fingerprint
  `HP/quartz/quartz:17/CL3B.260622.276.R2/16560795:user/release-keys`.
- Provider: system ANGLE 2.1 over Vulkan 1.4.295, Adreno driver 512.886.6, GLES 3.2.
- Launcher: FCL 1.3.4.0. Supported modern game lane remains Minecraft 1.21.1 / NeoForge
  21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13. Minecraft 26.3 and 1.20.1 are separate,
  unqualified lanes.
- Device is available but serialized for this lane. Workers are host-only unless granted.

## Current gate status

| Gate | Status | Evidence |
| --- | --- | --- |
| S0 baseline/provenance | **PASS for identity; PARTIAL for regression baseline** | APK/core hashes, device tuple, host tests; white UI/ACT/WSR baseline not rerun in this lane |
| S1 spatial foundation | **PARTIAL: host PASS + R6 state device PASS** | Corrected FSR1; X1E texture/UBO/FBO/viewport preservation PASS for first use and resize; two contexts/off-path NOT RUN |
| S2 contract | **PASS (frozen)** | `docs/fsr3/frame-contract.md` ABI v1 |
| S2 backend feasibility | **PASS** | X1E RCAS exact vs CPU; 8-pass chain 2x/1.5x; independent X1E multi-frame evidence now present |
| S2 game audit | **PASS (source)** | `docs/fsr3/audits/minecraft-iris-pass-input.md`: final TAA is `composite6`, jitter is partial, no velocity buffer in Iris 1.8.14 |
| S3 native temporal backend | **PARTIAL** | Deterministic reset/history/reset probe PASS; per-pass Quality GPU timing ~17.7–23.6 ms total (PrepareReactivity ~6, Accumulate ~4–5); flat synthetic probe, game integration not qualified |
| S4 first game path | **PARTIAL: NativeAA artifact-free PASS; scaled final-pass path FAILS ownership** | NativeAA night run: hand/light overlay gone, stars world-locked; Balanced/Performance reproduced translucent hand/light-shadow overlay on the final-pass-only pack; `validation/2026-10-20-x1e-nativeaa-final-pass.md` |
| S5 Complementary | **BLOCKED** | source boundary mapped; ACT/WSR/r5.9.2 device qualification NOT RUN |
| S6 qualification | **NOT RUN** | no matched benchmark/thermal qualification |

## Confirmed findings

- The integration review (`audits/2026-10-20-integration-review.md`) found
  concrete defects. R1 (unsigned reconstructed-depth sampler), R6
  initialization state, R7 camera continuity and R9 damage-swap routing are
  fixed; R2 (final-image boundary), R3 (scene depth/screen grid), R4 (camera
  precision), R5 (jitter/depth parameters/TAA), R8 (ABI validation) and R10
  (steady-state cost) remain open.
- R1 host reproduction after the fix shows the unsigned sampler on its own unit
  with R32UI; all eight dispatch calls still succeed.
- R7/R4 device run with the paired JAR/core: `camera motion active at frame 3`,
  full frame/HUD, effective Balanced 1694x932 -> 2880x1585.
- The X1E backend reset/history/reset probe is deterministic across two runs.
- The first full-frame game run confirms the bridge loads, camera motion
  activates, output is native and HUD is correct at the tested tuple.
- NativeAA on the final-pass-only pack is the current artifact-free baseline.
  Scaling Iris targets in that pack reproduced a translucent hand/light-shadow
  overlay; scaling is conditional and must not be the default there.
- Source anchors for the hand/noHand boundary and the bounded scaled-path plan
  are in `design/artifact-free-scaled-final-pass.md`.
- No matched performance benchmark or Complementary/ACT/WSR qualification exists.

## Worker state

| Worker | Worktree/branch | Status | Handoff |
| --- | --- | --- | --- |
| FsrCoreAudit | `/tmp/mg-fsr-core`, `googlebook/fsr-core-s1` | completed: host PASS/build PASS, device NOT RUN | `docs/fsr3/audits/core-fsr1.md` |
| FsrBackendSpike | `/tmp/mg-fsr-backend`, `googlebook/fsr-backend-s2` | completed: host + X1E compile/ops PASS, dispatch NOT RUN | `docs/fsr3/audits/backend-feasibility.md` |
| FsrGameAudit | `/tmp/mg-fsr-game`, `googlebook/fsr-game-audit` | completed: source audit PASS | `docs/fsr3/audits/minecraft-iris-pass-input.md` |
| FsrCoreReview | review task | running | independent FSR1 defect review |

## Next executable action

1. Keep NativeAA as the artifact-free default for the current final-pass-only
   pack; do not claim scaled FSR3 performance there.
2. Implement Experiment A from `design/artifact-free-scaled-final-pass.md`
   (debug extent/bind trace at pre-hand, hand return and final return) and have
   the user operate one Balanced run.
3. Only if the hand color/depth targets are proven scene-extent and `noHand`
   remains pre-hand depth, implement Experiment B (one scene extent for hand).
4. Keep shadow/light targets unscaled; run Experiment C only if the overlay
   persists after B.
5. Do not enable temporal history or motion for this boundary.
