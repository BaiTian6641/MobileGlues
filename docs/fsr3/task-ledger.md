# FSR3 task ledger

Updated: 2026-10-20. Owner: coordinator. Gate labels follow
`docs/MobileGlues_FSR3_Research_and_Implementation_Plan.md`.

| ID | Stage | Task | Owner | Paths | Status | Evidence / blocker |
| --- | --- | --- | --- | --- | --- | --- |
| FSR-S0-A | S0 | Lock actual core/plugin/device provenance | coordinator | `docs/fsr3/` | DONE | plugin 2.0.0 APK `f032e667…`; installed `libmobileglues.so` `dcb3493e…`; core source `8ac7036`; checkout `355d797` |
| FSR-S0-B | S0 | Host baseline and existing regressions | coordinator | `docs/fsr3/` | PARTIAL | `sh MobileGlues-cpp/tests/run.sh` passes; game baseline captures exist; white UI/ACT/WSR regression rerun pending |
| FSR-S0-C | S0 | Reproduce FSR1 static findings | FsrCoreAudit | `/tmp/mg-fsr-core` | PASS (host) | all five defects reproduced; report `core-fsr1.md`; device NOT RUN |
| FSR-S1-A | S1 | Output-first FSR1 dimensions and true reduced render | FsrCoreAudit | `gl/FSR1/`, focused tests | PARTIAL (host/build PASS; device NOT RUN) | target == EGL surface, render derived; adapter still needed for app viewport/uniforms |
| FSR-S1-B | S1 | Correct EASU→RCAS two-pass dataflow and format policy | FsrCoreAudit | `gl/FSR1/` | PARTIAL (host/build PASS; device NOT RUN) | RCAS reads EASU intermediate; one RGBA8 policy; shader validates GLSL 450 |
| FSR-S1-C | S1 | Injected-pass state/lifecycle tests | FsrCoreAudit | `gl/FSR1/`, tests | PARTIAL (host PASS; device NOT RUN) | expanded state mask and tests; independent review running |
| FSR-S2-A | S2 | Freeze ABI v1 frame contract | coordinator | `docs/fsr3/frame-contract.md` | DONE | frozen; requires version bump for changes |
| FSR-S2-B | S2 | Pin AMD SDK v1.1.4 / FSR3.1.4 and audit licenses | FsrBackendSpike | `docs/fsr3/audits/backend-feasibility.md` | PASS | commit `c6efa6b`; MIT upscaler path; 38-file manifest |
| FSR-S2-C | S2 | GLES operation/format feasibility spike | FsrBackendSpike | `tools/fsr3_spike/` | PASS (compile/ops; dispatch NOT RUN) | X1E probe 14/22 with expected format/subgroup failures; 12/12 ESSL compile; 10/10 host SPIRV-Cross |
| FSR-S2-D | S2 | Minecraft/Iris pass graph, jitter, motion audit | FsrGameAudit | `/tmp/mg-fsr-game` | PASS (source audit) | report `minecraft-iris-pass-input.md`; final TAA is pack `composite6`; jitter is partial; no engine motion/velocity in 1.8.14 |
| FSR-S2-E | S2 | Execute one adapted pass, then the full 8-pass chain, against reference | FsrBackendDispatch | `tools/fsr3_spike/dispatch/` | PASS (executed; parity not Vulkan) | RCAS exact vs CPU; full chain 2x/1.5x; host/device hashes equal; multi-frame/ABI/timing NOT RUN |
| FSR-S3-A | S3 | Minimal in-tree GLES backend and shader adaptations | backend | `MobileGlues-cpp/upscaling/` | READY | S2 dispatch PASS; needs controller/ABI, persistent resources and production submission |
| FSR-S4-A | S4 | Adapter reduced-resolution scene + semantic inputs | game | game integration module | BLOCKED | waits for S1/S2 |
| FSR-S4-B | S4 | Camera-only prototype, then moving objects | game | adapter/debug | BLOCKED | waits for S3 |
| FSR-S5-A | S5 | Complementary r5.9.3 profile; r5.9.2 regression | game/QA | integration metadata | BLOCKED | waits for S4 |
| FSR-S6-A | S6 | Native/bilinear/FSR1/FSR3 matched benchmark matrix | QA | `bench/upscaling/` | NOT RUN | device serialized; thresholds provisional |
| FSR-S6-B | S6 | Sustained thermal, lifecycle, packaging | QA | release | NOT RUN | waits for S5 |

## Ownership locks

- Coordinator: shared ABI, CMake, `egl/`, `gl/framebuffer.cpp`, `gl/texture.cpp`, integration
  merge order, final status.
- Core worker: `MobileGlues-cpp/gl/FSR1/` and focused tests only.
- Backend worker: `MobileGlues-cpp/upscaling/fsr3/` after S2 decision, plus benchmark harness;
  no shared file edits without a patch proposal.
- Game worker: integration module and versioned profile metadata; no edits to original packs.
- QA: validation/benchmark evidence; no implementation edits.

## Stop conditions

Do not advance a gate on a compile-only result. Do not mark FSR3 available if the backend
falls back to spatial scaling. Do not improve FPS by disabling ACT, WSR, player reflections,
or requested pack effects.
