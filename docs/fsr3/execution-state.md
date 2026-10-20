# FSR3 execution state

Updated: 2026-10-20 (S0/S1 start). This file is the continuation record for the FSR3 lane. It
does not replace `docs/mobileglues-googlebook/execution-state.md`; the older file remains the
source of truth for the pre-FSR3 renderer/pack history.

## Repository identity

| Item | Value |
| --- | --- |
| Integration worktree | `/home/polar/MobileGlues` |
| Branch | `googlebook/x1e-stage1-3` |
| HEAD | `f793190`; R1/R6/R7/R9 review fixes committed in `ae29d70`/`c65de12`; package `025850…` (see validation) |
| Origin | `git@github.com:BaiTian6641/MobileGlues.git` |
| Upstream | `https://github.com/MobileGL-Dev/MobileGlues.git` |
| Untouched user changes | modified `.gitignore`, `docs/mobileglues-googlebook/validation/README.md`; untracked `.vscode/`, FSR3 plan, recent validation docs |
| Worktrees | `/tmp/mg-fsr-core` → `googlebook/fsr-core-s1`; `/tmp/mg-fsr-backend` → `googlebook/fsr-backend-s2`; `/tmp/mg-fsr-game` → `googlebook/fsr-game-audit` |
| Existing unrelated worktree | `/tmp/mg-depth-port` detached at `b1ff643`; not part of this lane |

### Provenance

- Installed renderer: `com.fcl.plugin.mobileglues` **2.0.0**, review-fix APK
  SHA-256 `025850a0180eead1d4434d35cf9f9d5ac5bfd09503572b7bde8a927f3df64ce9`.
- Installed ARM64 core: `lib/arm64-v8a/libmobileglues.so`, SHA-256
  `472576b7fe303d06362277a4e9f2864dd1077177bebfe54e615dc1a38816024f`.
- Installed bridge JAR: `mgfsr3bridge-0.1.0.jar`, SHA-256
  `7e171274b9e86619e1455ec8849a297ca244615c5d1244dfa6794babff9724dc`
  (diagnostic scene-extent logging on top of the functional bridge).
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
| S1 spatial foundation | **PARTIAL: host/build PASS; FSR1 device NOT RUN** | Corrected two-pass EASU/RCAS and output-first sizing; host tests PASS; FSR1 not device-qualified |
| S2 contract | **PASS (frozen)** | `docs/fsr3/frame-contract.md` ABI v1 |
| S2 backend feasibility | **PASS** | X1E RCAS exact vs CPU; 8-pass chain 2x/1.5x; independent X1E multi-frame evidence now present |
| S2 game audit | **PASS (source)** | `docs/fsr3/audits/minecraft-iris-pass-input.md`: final TAA is `composite6`, jitter is partial, no velocity buffer in Iris 1.8.14 |
| S3 native temporal backend | **PARTIAL** | X1E three-frame reset/history/reset probe deterministic (`cf71d4b5…`); R1 unsigned-sampler and R6 init-state defects fixed; per-pass timing NOT RUN |
| S4 first game path | **PARTIAL: rendering PASS; performance NOT QUALIFIED** | Quality 1920x1057 -> 2880x1585, camera_motion=1, full world + native HUD; spot ~18 off/~20 Quality/~20 Performance, not frame-locked; R2/R3/R5 boundary/jitter defects remain; `validation/2026-10-20-scene-adapter-quality.md` |
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
- The X1E backend reset/history/reset probe is deterministic across two runs.
- The first full-frame game run confirms the bridge loads, camera motion
  activates, output is native and HUD is correct at the tested tuple.
- No matched performance benchmark or Complementary/ACT/WSR qualification exists.

## Worker state

| Worker | Worktree/branch | Status | Handoff |
| --- | --- | --- | --- |
| FsrCoreAudit | `/tmp/mg-fsr-core`, `googlebook/fsr-core-s1` | completed: host PASS/build PASS, device NOT RUN | `docs/fsr3/audits/core-fsr1.md` |
| FsrBackendSpike | `/tmp/mg-fsr-backend`, `googlebook/fsr-backend-s2` | completed: host + X1E compile/ops PASS, dispatch NOT RUN | `docs/fsr3/audits/backend-feasibility.md` |
| FsrGameAudit | `/tmp/mg-fsr-game`, `googlebook/fsr-game-audit` | completed: source audit PASS | `docs/fsr3/audits/minecraft-iris-pass-input.md` |
| FsrCoreReview | review task | running | independent FSR1 defect review |

## Next executable action

1. Install the quality-slider APK and capture F3 at NativeAA, Quality, Balanced,
   Performance, and Ultra Performance with FSR3 on, plus FSR3 off.
2. Device-run the scene-adapter APK + bridge pair. Verify scaled target extents, native HUD, no crop, and the direct-present skip before any performance claim.
3. Reconcile the game audit with the frozen ABI, then assign the first vertical slice.
4. Build the first FSR3-capable test binary and run the white UI/ACT/WSR baseline before any
   performance claim.
