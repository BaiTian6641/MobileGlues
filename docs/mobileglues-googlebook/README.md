# Googlebook execution records

Evidence, decisions and handoff state for the specification in `docs/01`-`docs/03`. This
directory does not contain release claims. Machine-readable distribution inputs live in
`compat/`.

## Files

| File | Purpose |
| --- | --- |
| [FSR3 universal rendering research](2026-10-21-fsr3-universal-rendering-research.md) | Eight exact adapter locks, primary-source API families from 1.17 through 26.2, rendering contracts and future qualification gaps. |
| [Buffer mapping and rendering checkpoint](2026-10-21-buffer-mapping-and-rendering-checkpoint.md) | S4.9 coherent-storage versus mapping correction, exact Sodium comparison, mapping/error corrections, 187-check review, five-case host/provider evidence and unproven terrain causality. |
| [26.2 sampler, RGBA8 and draw-export regressions](validation/2026-10-21-262-sampler-and-rgba8-regressions.md) | Fork-only 26.2 regressions, upstream control, device bisect to `609ff8e`, missing draw exports and the zero-mod 26.2 pass; NeoForge/Iris and 1.21.1 regression gates remain pending. |
| [26.2 FSR3 performance, RGBA8 and bridge coverage](validation/2026-10-21-26.2-fsr3-performance-and-rgba8.md) | 36→66-70 FPS from installing the missing 26.2 bridge, 1.21.1 74 FPS, Create 76 FPS, RGBA8 null image-bind pass, and ten-profile 26.1/26.2-capability bridge coverage. |
| [FSR3 depth contract and runtime status](2026-10-21-fsr3-depth-contract-and-runtime-status.md) | Depth and viewport/buffer checkpoints, verified native 2.0.8 and bridge 0.2.4 CI, Create/Steadfast visual pass, retained 26.2 failure and interrupted control, 2.0.7 full-library error failure, 2.0.8 physical full-library pass, reviewed bridge 0.2.4 legacy depth correction, prepared Fabric 1.20.1 payload, and pending game/lifecycle gates. |
| `execution-state.md` | Current task, repository identity, confirmed progress, blockers and next action. |
| `feature-matrix.md` | Evidence rows for renderer builds, provider gates, pack/effect attempts and device gates. |
| `decisions.md` | Architecture decisions with evidence and consequences. |
| `stack-lock.json` | Reproduced local source/toolchain/game tuple and artifact identities. |
| `validation/` | Machine-readable probe and wrapper results plus capture hashes. |

## Historical-to-current stage mapping

Older records use the historical plan's `Stage 0`-`Stage 8`. The mapping is approximate
because the current plan separates runtime lanes and provider work that the old plan folded
into Stage 0.

| Historical | Current | Subject |
| --- | --- | --- |
| Stage 0 | S0 + S1 | Lock, build, provider and runtime startup |
| Stage 1 | S2 | Observable errors and truthful contract |
| Stage 2 | S3 + S4 | Shader/program/resource semantics |
| Stage 3 | S5 | Standard shaderpack fidelity |
| Stage 4 | S6 (ACT) | Advanced colored lighting |
| Stage 5 | S6 (WSR) | World-space reflections |
| Stage 6 | S7 + S9 | Optimization and desktop lifecycle |
| Stage 7 | S9 | Packaging and release |
| Stage 8 | S8 | X2E and additional-device qualification |

Do not copy a historical stage result into a current gate without rechecking its lane, provider
and pack tuple.

## Earlier stage checkpoint

The stage table below predates the desktop-GL continuation. Use the dated buffer
and runtime records above for the latest source, artifacts and physical results:
2.0.8 has passed independent CI artifact verification and the bounded physical
full-library buffer suite, including a separate run of the exact CI ARM64 payload.
The bridge 0.2.4 legacy depth correction has passed independent host review
and eight-profile CI/artifact verification. Game rendering and physical bridge
lifecycle checks remain pending. The historical absence/status statements below
are not a current feature inventory. New bounded fixes do not automatically
close their larger stage gates.

| Lane/stage | Status | What the evidence covers |
| --- | --- | --- |
| S0 | PARTIAL | Core `97558a6`, plugin `f5105e4`, NDK r28c ARM64 build and probe tooling reproduced. Launcher selection, delivery-lane native closure and exact asset lock remain open. |
| S1-legacy | NOT RUN | The pinned 1.20.1 / Fabric lane in `compat/runtime-lock.json` has not been booted. The 1.21.1 / NeoForge game-process smoke belongs to `S1-current`, not this lane. |
| S1-current | PARTIAL / STAGED WORKING | Delivery lane is Minecraft 1.21.1 / Java 21 / NeoForge 21.1.262 / Iris 1.8.14-beta.1 NeoForge / Sodium 0.8.13 NeoForge. The installed 21.1.262 tuple is staged/working: 1.21.1 world loaded and rendered, original pack active, save/quit to title. Effect gates, performance and lifecycle remain open. NeoForge 21.1.264 is an optional future patch, not a blocker. |
| S1-provider | PARTIAL | Below-wrapper and wrapper probes ran on the X1E HP unit through system ANGLE. App-local ANGLE and the bounded Zink/Turnip experiments were not run. |
| S2 | PARTIAL | Multi-string `glShaderSource`, frontend errors, compile/link/FBO status, limits, extension admission, per-program fragment-output state and SSBO block binding are repaired and probed. Cache identity, retained reflection and remaining stubs stay open. |
| S3 | PARTIAL | Compiler and resource-mapping work exists; the exact compiler corpus, source maps, reflection table and cache-key completeness are not closed. |
| S4 | PARTIAL | Formats, framebuffer routing, 3D image storage and SSBO mapping have staged device evidence. R8UI/R16UI lowering is absent; synchronization and persistent-map contracts are incomplete. |
| S5 | STAGED | Both original r5.9.2 styles reach High with no fallback in the supported 21.1.262 game process; r5.9.3 High also initializes. Named-effect fidelity is not tested. |
| S6 | STAGED | ACT 192 and WSR 128 pass staged scenes on r5.9.2; WSR 128 is the required advanced WSR gate. WSR 192 is optional staged headroom. Ultra ACT 256/WSR 256 is optional/future X2E and `BLOCKED` on X1E by the provider's 134,217,728-byte storage-block limit; it is not an X1E release blocker. Player reflections and the independent compute pack are not run. |
| S7 | NOT RUN | No controlled optimization or sustained performance result. |
| S8 | NOT RUN | No retail X2E device or independent profile. |
| S9 | NOT RUN | No signed release, fresh-install, update/rollback or desktop-lifecycle qualification. |

## Evidence levels

Keep these levels distinct in every record:

- Host semantic test: build or unit behavior, no GPU certification.
- Emulator: Android packaging/loading behavior only; not Adreno behavior.
- Target device, below wrapper: provider limits, formats and behavioral probes.
- Target device, wrapper level: MobileGlues translation and routing contract.
- Game process: launcher, JVM, native closure, Iris/Sodium and pack in one process.
- Desktop GL reference: correctness reference, not a Googlebook result.

A result at one level never certifies another. The XPS Googlebook specification is not a
substitute for an exact OEM device record; the connected unit is an HP Googlebook 14.

## Status rules

- Use `PASS`, `FAIL`, `BLOCKED`, `NOT RUN` or `NOT APPLICABLE` with evidence.
- A staged visual spot-check is not an effect-by-effect pass.
- A successful compile with an intended effect disabled is a failed advanced-feature test.
- Performance targets in the plan are objectives until measured on the named tuple.
- A missing device or unresolved native closure stays explicit; it is not converted into
  aggregate completion.
