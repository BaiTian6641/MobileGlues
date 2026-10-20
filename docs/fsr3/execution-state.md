# FSR3 execution state

Updated: 2026-10-20 (S0/S1 start). This file is the continuation record for the FSR3 lane. It
does not replace `docs/mobileglues-googlebook/execution-state.md`; the older file remains the
source of truth for the pre-FSR3 renderer/pack history.

## Repository identity

| Item | Value |
| --- | --- |
| Integration worktree | `/home/polar/MobileGlues` |
| Branch | `googlebook/x1e-stage1-3` |
| HEAD | `d4479c1` (`[FSR3] (contract): freeze ABI v1 and record baseline provenance`) |
| Origin | `git@github.com:BaiTian6641/MobileGlues.git` |
| Upstream | `https://github.com/MobileGL-Dev/MobileGlues.git` |
| Untouched user changes | modified `.gitignore`, `docs/mobileglues-googlebook/validation/README.md`; untracked `.vscode/`, FSR3 plan, recent validation docs |
| Worktrees | `/tmp/mg-fsr-core` → `googlebook/fsr-core-s1`; `/tmp/mg-fsr-backend` → `googlebook/fsr-backend-s2`; `/tmp/mg-fsr-game` → `googlebook/fsr-game-audit` |
| Existing unrelated worktree | `/tmp/mg-depth-port` detached at `b1ff643`; not part of this lane |

### Provenance

- Installed renderer: `com.fcl.plugin.mobileglues` **2.0.0**, APK SHA-256
  `f032e6673dc8a626ef27775747b6cb0fdb4e5e48918ed3a62cc5d37a0a515a82`.
- Installed ARM64 core: `lib/arm64-v8a/libmobileglues.so`, SHA-256
  `dcb3493e240d82354ba7fc2db446ef25fd8d7c54f75d16bed164bd5e92c129a1`, Build ID
  `d99046abb48428ea108f9f7aafff7a325548db24`.
- Installed binary strings identify the plugin build tree
  `/home/polar/MobileGlues-plugin/MobileGlues`; that submodule is at `8ac7036`
  (2026-10-19 10:29:15 -0500). This is the actual installed-source lead, not the audited
  `97558a6` upstream snapshot and not this checkout's `355d797`.
- The installed header defines `MG_MOBILEGLUES_VERSION 2000`; it contains the FSR1 setting and
  the same single-pass FSR1 shader as the checkout. No FSR3 symbols or shaders are installed.

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
| S1 spatial foundation | **PARTIAL: host/build PASS, device NOT RUN** | Core branch `5f0c421` corrects dataflow/sizing; independent review running; adapter still needs to render the scene at the derived extent |
| S2 contract | **PASS (frozen)** | `docs/fsr3/frame-contract.md` ABI v1 |
| S2 backend feasibility | **PASS (compile/ops + one-pass/full-chain dispatch)** | X1E RCAS exact vs CPU; 8-pass chain executed at 2x/1.5x with host parity; independent Vulkan and multi-frame history NOT RUN |
| S2 game audit | **PASS (source)** | `docs/fsr3/audits/minecraft-iris-pass-input.md`: final TAA is `composite6`, jitter is partial, no velocity buffer in Iris 1.8.14 |
| S3 native temporal backend | **PARTIAL** | In-tree ABI/backend + optional bridge submit PASS; low-resolution scene and timing NOT RUN |
| S4 first game path | **PARTIAL / FAIL performance** | Config switch and optional Iris final-pass bridge work; scene remains native, 13 FPS vs 26 off |
| S5 Complementary | **BLOCKED** | waits for S4 |
| S6 qualification | **NOT RUN** | no FSR3 installation exists |

## Confirmed findings

- Host semantic suite passes at `355d797`: `sh MobileGlues-cpp/tests/run.sh` (pixel, framebuffer,
  shader source, depth filter, cache key, compat image, subgroup).
- FSR1 in this checkout and in the installed binary computes EASU into `color`, then calls RCAS
  on the original input texture and writes only `sharpenedColor`. The EASU result is dead.
- FSR1 takes EGL surface width/height as render width/height, then multiplies by the preset to
  create target dimensions. With FSR1 on, the application's framebuffer 0 redirect can therefore
  enlarge scene work instead of reducing it.
- FSR1 initializes render color as RGBA8, then recreates it as RGBA32F on resize.
- The installed plugin is not an FSR3 build. Any FSR3 claim requires a new exact binary identity.

## Worker state

| Worker | Worktree/branch | Status | Handoff |
| --- | --- | --- | --- |
| FsrCoreAudit | `/tmp/mg-fsr-core`, `googlebook/fsr-core-s1` | completed: host PASS/build PASS, device NOT RUN | `docs/fsr3/audits/core-fsr1.md` |
| FsrBackendSpike | `/tmp/mg-fsr-backend`, `googlebook/fsr-backend-s2` | completed: host + X1E compile/ops PASS, dispatch NOT RUN | `docs/fsr3/audits/backend-feasibility.md` |
| FsrGameAudit | `/tmp/mg-fsr-game`, `googlebook/fsr-game-audit` | completed: source audit PASS | `docs/fsr3/audits/minecraft-iris-pass-input.md` |
| FsrCoreReview | review task | running | independent FSR1 defect review |

## Next executable action

1. Review core FSR1 diff and evidence; integrate only after the off path and truthful failure
   checks are demonstrated.
2. Implement true low-resolution scene targets/viewports/depth for Minecraft/Iris; the current bridge does not reduce scene work.
3. Reconcile the game audit with the frozen ABI, then assign the first vertical slice.
4. Build the first FSR3-capable test binary and run the white UI/ACT/WSR baseline before any
   performance claim.
