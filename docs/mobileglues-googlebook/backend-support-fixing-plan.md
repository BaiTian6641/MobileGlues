# Shaderpack backend support fixing plan

Status: **plan only. No new backend.** Evidence date: 2026-10-20.
Target tuple: HP Googlebook 14, X1E80100 / Adreno X1-85, Android 17,
system ANGLE over Vulkan 1.4.295, GLES 3.2, Iris 1.8.14-beta.1.

The boundary that can be qualified is **(MobileGlues core, Iris bridge, original
archive bytes)**. A pass that requires a modified pack, an unreported companion
mod, or a hidden fallback is not an original-pack claim.

## Why this plan

The remaining original-pack failures are concentrated in wrapper/bridge seams,
not in a missing renderer:

1. desktop image qualifiers that ANGLE rejects;
2. image-unit assignment that can alias;
3. graphics-stage image rewriting;
4. SSBO clear reachability under the advertised GL profile;
5. pack lifecycle state;
6. Iris custom-uniform metadata;
7. advanced ACT/WSR evidence on r5.9.3 and Unbound;
8. BSL and effect-by-effect qualification.

Do not add a parallel backend, Zink, a second translator, or a Vulkan rewrite.
Do not edit original shaderpacks. Keep compile/link/FBO failures truthful.

## Ranked gaps

| ID | Gap | Evidence | Class | Smallest fix | Acceptance gate |
| --- | --- | --- | --- | --- | --- |
| G1 | `rg16f`, `rg32f`, target-aware `rgba16`/`rg16` image qualifiers: iterationRP `img_pixelData2D`, `FBIMG_ALT_OUTPUT`, `img_rtwWarp1D` | `validation/2026-10-19-x1e-iterationrp-caps.json`, `validation/2026-10-19-iterationrp-port.md`, `gl/image_metadata.cpp`, `gl/glsl/compat_image.cpp`, bridge `ImageBindingRegistry.java` | wrapper+bridge defect / provider limit | One shared legal-format table: widen null backing to RGBA16F/RGBA32F, make `rgba16` target-aware, extend RGBA16/RG16 attachments. Core must own the mapping; bridge must not emit host-specific qualifiers. | Original iterationRP compiles and renders every previously rejected program; unmappable formats report an error. |
| G2 | Compute image-unit determinism: `floodfill_img`, `voxel_img`, `wsr_img`, `wsr_lod_img` | `decisions.md` D7, `execution-state.md` ACT alias logs, bridge `ImageBindingTransformer.java` / `ProgramImagesBuilderMixin`, `gl/image_bindings.cpp` | wrapper+bridge defect | Prove core `mg_bind_image_uniforms` + remap with the bridge removed. If insufficient, port deterministic registry into core. Never fall back to unit 0. | Original r5.9.2 ACT 192 + WSR 128 with no bridge: distinct unit values and no flicker/one-color output. |
| G3 | WSR 128 SSBO zero-fill reachability: Iris will not call the implemented clear under the advertised GL 4.0 profile | `decisions.md` D8, `gl/buffer.cpp:1088-1138`, `gl/getter.cpp`, bridge `IrisRenderSystemMixin` | wrapper/version-contract defect | Expose the implemented zero/immutable-storage clear through the call path pinned Iris actually uses; narrow ARB path or small compatibility hook. Do not globally inflate GL version. Retire R8-only fallback. | Original WSR 128 without bridge zero-fills correctly; no fallback path. |
| G4 | Graphics-stage image rewriting: iterationRP `PixelData_VS`, Complementary shadow vertex images | `decisions.md` D7, bridge `ImageBindingTransformer.java` unused `compute` flag, player-ref report | wrapper+bridge defect / ES stage limit | Enforce stage-scoped rewriting; layout/format only where legal; explicit vertex multi-image probe. | Vertex multi-image programs compile; shadow vertex stage has no `ERROR: 1:1`; player reflections remain staged. |
| G5 | Iris custom uniforms: `BIOME_PALE_GARDEN`, `BIOME_SULFUR_CAVES`, `endFlashIntensity` | `feature-matrix.md:95-101`, `execution-state.md`, Iris pass-input audit | tuple/metadata (`c`) | Qualify a compatible Iris/pack tuple or document the suppression table. Do not patch original ZIPs. | Matched log has no unresolved-uniform warnings, or affected effects are explicitly NOT TESTED. |
| G6 | Pack lifecycle state: bridge unit/format maps and core fallback map persist across reload/switch; 32-name cap reuses unit 0 | bridge `ImageBindingRegistry.java`, `gl/image_bindings.cpp`, S9 NOT RUN | wrapper+bridge defect | Clear maps on pack load/unload/reload/context loss; fail loudly on unit exhaustion. | A→B→A pack switch and same-pack reload preserve distinct units; no silent reuse. |
| G7 | ACT/WSR staging beyond r5.9.2: r5.9.3 and Unbound; residual half-rate floodfill circle | `feature-matrix.md:116-121`, `execution-state.md`, AuroraLumen audit | qualification (`d`) | Same profile matrix on r5.9.3 and Unbound; 8 s clips; compare floodfill behavior to desktop reference. | Staged ACT 192 / WSR 128 on r5.9.2, r5.9.3, Unbound; no fallback; player refs off. |
| G8 | BSL and effect fidelity; AuroraLumen private boundary | `feature-matrix.md:88-93,122-124`, AuroraLumen validations | qualification (`d`) | Import original BSL default; run named-effect matrix on original packs. Keep AuroraLumen private-only. | BSL pipeline active with named effects; per-effect matrix recorded; private derivative never used as original-pack evidence. |
| G10 | scene-scaled no-final pack copy: iterationRP finalizes through composites, Iris copies scaled colortex0 into native main | `validation/2026-10-20-g10-no-final-guard.md` | bridge defect | detect `ProgramId.Final` before RenderTargets; disable bridge for no-final packs | **PASS on device** |
| G9 | RGB8/RGB10_A2 render-target completeness for Sildur: Vibrant colortex1/3 `RGB8`, Enhanced `gnormalFormat=RGB10_A2` | `validation/2026-10-20-x1e-sildur-format-probe.md`; both FBO complete | **PASS (provider FBO)** | No core widening needed for these formats; keep RGB8_SNORM handling separate | FBO complete; source compile/link and S0/S1 remain |

### G1 implementation status

- Host: RG16F → RGBA16F and RG32F → RGBA32F lowering/backing implemented with
  `compat_image` and `image_metadata` tests passing
  (`validation/2026-10-20-g1-rg16f-rg32f-lowering.md`).
- Still open: target-aware RGBA16/RG16 image-bound attachments, stage-scoped
  rewriting, core-only unit proof, and original-pack host sweep/device gate.
- The G1 build is not the currently installed stable core; do not treat the
  host test as a device pass.

### iterationRP refinement (from the 2026-10-20 static audit)

- **G1 is the first hard blocker.** The original default graph has 15 active
  illegal qualifier sites: 9x `rgba16`, 1x `rg16`, 5x `rg16f`. Current core
  already partially handles `rgba16`/`rg16`; `rg16f`/`rg32f`, `RG16F` storage
  allowlisting, and target-aware attachment policy remain. The first gate is
  the five `img_pixelData2D` programs, not a full-pack rewrite.
- **G4 is the second hard blocker.** `PixelData_VS` declares a vertex
  `image2D`; `shadow.gsh` declares three `uimage3D`s and uses
  `imageAtomicMax`. Rewriting must be stage-scoped; do not apply the
  compute-only bridge to graphics stages.
- **G2 is a proof gap, not yet a demonstrated failure.** Prove the core
  resolver on the original pack with the bridge removed, especially the
  six-image `Composite_Copy_CS` and three-image `shadow.gsh`.
- **G3 is not a default-profile iterationRP blocker.** Its 4 KiB SSBO is fully
  written by `SSBOUpdate_CS`; keep G3 on the WSR/shared path.
- **G6 is S3 work.** The first import does not need pack switching, but the
  32-name fallback must never silently reuse unit 0.

The private X1E ports widen formats and rewrite subgroup/BackwardAnalysis/SH
semantics. They are not original-pack evidence and must not be used as the
correctness reference. See
`shaderpack-support/audits/2026-10-20-iterationrp-static-audit.md`.

### Evidence corrections

The current tree already implements R16UI/R8UI→R32UI lowering and the
zero/immutable `glClearBufferSubData` path. Rows claiming those are absent
(`feature-matrix.md:170`, `README.md:48`, `decisions.md:110`) are stale. The
remaining defects are other illegal qualifiers, clear **reachability**, and
stage/lifecycle behavior. Correct those rows during Phase 1.

## Phases and exit gates

### Phase 1 — Truthful image/format/lifecycle contract

Order: **G1 → G2 → G3 → G4 → G6**.

- One legal-format table shared by core and bridge; no host-specific qualifiers
  in the bridge.
- Prove core-only image units; no unit-0 fallback.
- Expose SSBO clear on the pinned Iris path; retire bridge zero-fill.
- Stage-scoped graphics image rewriting.
- Clean lifecycle state and loud exhaustion.

Exit gate: original r5.9.2 ACT 192 + WSR 128 with **no bridge**; original
iterationRP compiles/renders previously failing programs; vertex multi-image
passes; A→B→A reload passes; every failure is observable.

### Sildur entry path (parallel to Phase 1)

Sildur does not require images/SSBO, so it does not wait for G1-G4. Start with
**G9/F0**:

1. Host FBO probe for RGB8, RGB10_A2, RGB16F, R11F and RGBA8.
2. Fix only the core legal/renderable target table if RGB8/RGB10_A2 fail.
3. Host-compile Vibrant `gbuffers_textured`, `composite1`, `deferred` through
   the Iris-preprocessed/patched source.
4. Device S0/S1: Vibrant first; Basic is the all-RGBA8 control if Vibrant
   fails before shader compilation; Enhanced follows.

Exit gate: Vibrant S0/S1 with no fallback and no format failure; Basic and
Enhanced S0/S1 controls; S2 effect matrix remains separate.

### Phase 2 — Original-pack metadata and advanced effects

Order: **G5 → G7**.

- Resolve or explicitly document the three custom uniforms.
- Run the same profile/resource matrix on r5.9.3 and Unbound; classify the
  floodfill circle against a desktop reference.
- No code change unless the reference shows a wrapper defect.

Exit gate: no unresolved uniforms (or explicit suppression table); staged
advanced passes on r5.9.2/r5.9.3/Unbound; no fallback; player refs off.

### Phase 3 — Qualification and frozen claims

Order: **G8 + lifecycle/performance**.

- Original BSL default import and named-effect matrix.
- Resize, suspend/resume, provider fallback, pack switch, context recovery.
- Keep performance and correctness gates separate.
- Freeze the safe-claim table; everything else is NOT RUN/UNSUPPORTED.

Exit gate: only safe claims are published; no FSR3/framegen claim; no effect
removed to obtain a pass.

## Safe claims today

| Claim | State |
| --- | --- |
| Complementary r5.9.2 High: active, no fallback, basic scene | SAFE TO CLAIM (staged, visual spot-check only) |
| Complementary r5.9.3 High: initializes, no fallback, basic scene | SAFE TO CLAIM (staged) |
| Complementary Unbound r5.9.2 High: active, no fallback, reload | SAFE TO CLAIM (staged) |
| Complementary effect-by-effect correctness | NOT SAFE |
| ACT 192 / WSR 128 on r5.9.2 | STAGED, bridge-assisted only |
| ACT/WSR on r5.9.3 or Unbound | NOT SAFE |
| WSR 256 / ACT 256 on X1E | BLOCKED by 128 MiB SSBO limit |
| Player reflections | FUNCTIONAL STAGED; PERFORMANCE/POWER FAIL |
| BSL v10.1.8 | NOT SAFE |
| Original iterationRP | BRING-UP ONLY |
| AuroraLumen private profiles | PRIVATE DEVICE PASS ONLY |
| FSR3 / framegen with packs | NOT SAFE / OUT OF SCOPE |

## Next executable action

Start Phase 1/G1 on the host:

1. add a shared legal image-format mapping test covering `rgba16`, `rg16`,
   `rg16f`, `rg32f`, and RGBA16/RG16 attachments;
2. make the bridge emit no provider-specific qualifier;
3. run the iterationRP shader-transform fixture and expect zero illegal
   qualifiers with truthful errors for unmappable cases.

Do not start G2 until G1's format table is shared; otherwise unit tests will
encode two mappings again.
