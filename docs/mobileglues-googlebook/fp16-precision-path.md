# FP16 precision optimization path

Branch: `googlebook/x1e-fp16-precision`  
Base milestone: `355d797` (original iterationRP bring-up, Complementary perf model).  
Status: design/probe gate only. No runtime precision mode is enabled by default.

## Why this path exists

X1E-80-100 (Adreno X1-85) has a higher FP16 rate than FP32, while the Radeon
780M's FP16 is effectively FP32-rate. The X1E also has higher memory bandwidth.
FP16 ALU is therefore a relative platform advantage, not just parity.

Measured on the device through ANGLE (`tools/fp16_probe`):

| Precision contract | Driver result |
|---|---|
| `highp` | range 127, precision 23 (FP32) |
| `mediump` | range 15, precision 10 (FP16) |
| explicit `float16_t` ES extension | not exposed |
| dependent vec4 ALU loop, highp | ~8.19 ms |
| same loop, mediump | ~3.39 ms |
| ALU ratio | ~2.4x |

This is an ALU-rate probe. It does not promise a shaderpack FPS ratio; fill,
texture and bandwidth costs are unchanged.

## Non-goals

- No global `highp` → `mediump` token replacement.
- No FP16 for world/view positions, depth reconstruction, matrices, shadow
  coordinates, SSR/WSR ray positions, TAA/history, ACT/WSR indices, atomics or
  packed-ID buffers.
- No silent quality reduction. The mode is opt-in and per-pass.
- No second graphics backend; this stays inside MobileGlues' GLSL path.

## Proposed architecture

1. **Precision policy** in MobileGlues config:
   - `off` (default): current behavior, pack declarations preserved.
   - `safe`: only an allowlisted set of fragment color passes receive mediump.
   - Never a blanket stage/type rewrite.
2. **Preserve authored precision.** Do not force highp over pack-authored
   mediump/lowp. Inject highp only for uniforms that need it for linkage.
3. **Per-expression/variable transform.** Use an AST or validated source pass
   that marks selected temporaries and color math as mediump; keep address,
   coordinate, depth, matrix and history expressions highp.
4. **Cache identity.** The translation cache key must include the precision
   policy, per-pass allowlist version and backend FP16 capability. A shader
   compiled under `safe` must never be reused for `off` or another allowlist.
5. **Capability evidence.** `mediump` is permission, not a guarantee. Record the
   `glGetShaderPrecisionFormat` contract and measure each pass; the compiler may
   promote expressions.

## First allowlist (provisional)

Start with full-screen color-only work:

- composite/tonemap color accumulation,
- bloom downsample/upsample color math,
- final color grading and dithering.

Exclude in the first slice:

- all vertex stages and world/view coordinates,
- shadow sampling and PCF,
- SSR/WSR/voxel ray marching,
- TAA history/reprojection,
- ACT/WSR image indices and atomics,
- packed material ID buffers.

Expected outcome: ALU savings in color passes, with modest whole-frame gain
because High is also fill/texture-bound. Expand only after the first slice passes
its visual and performance gates.

## Audit result (scope control)

The precision audit of Complementary Reimagined r5.9.3, Unbound r5.9.2 and
iterationRP r0.8.31 found **zero authored precision statements** in all three
packs. The effective highp default came from MobileGlues. SPIRV-Cross's default
fragment float precision is mediump, and the old `forceSupporterOutput`
replacement deleted all precision lines and inserted highp, silently promoting
any RelaxedPrecision variable.

A mediump-able subset is limited to LDR color/luma work. Highest-value safe
candidates:

1. Complementary/Unbound `final` (color, sharpen, vignette) — keep UVs, depth,
   dither coordinates and WSR clear path highp.
2. iterationRP `composite85` (`FINAL_1`) — LDR output; keep UVs/material IDs highp.
3. Complementary `composite7` FXAA color/luma — keep depth/UV/directions highp.
4. Complementary `composite5` bloom-tile/saturation subgraph — keep tonemap,
   depth and HDR color highp.

Whole-pass mediump is unsafe for `composite`, `composite1`, composite HDR
tonemap/Bloom accumulation, shadows, SSR/WSR, TAA/history, ACT/WSR and packed
IDs. The static estimate for ranks 1–4 is only ~3–5% of executed fragment ALU,
roughly **1–2% of frame time** at High; the 2.3x ALU ratio is not a frame-time
ratio. This path is therefore a polish item, not a substitute for resolution and
profile cuts.

## Gates

- **G0 (done):** reproducible probe and driver precision contract recorded.
- **G1 (done):** SPIRV-Cross now emits an explicit highp default and keeps
  explicit mediump on relaxed variables; `forceSupporterOutput` no longer
  deletes precision lines. Translation cache schema bumped to 5.
- **G2:** preserve pack precision; no-op precision policy; existing shaderpack
  output unchanged.
- **G3:** `safe` allowlist for `final` only, A/B FPS and visual reference; no
  artifacts in shadows/SSR/TAA/IDs.
- **G4:** add `composite7`/`composite5` color subgraphs only after per-pass gain
  and reference validation; expand pass by pass.

## Validation

- Compare against the same frame with `off`; use exact/near-exact image metrics
  where appropriate and visual inspection for effects.
- Check shadow edges, SSR/WSR, TAA history, colored lighting, water, particles,
  and packed IDs specifically.
- Record FPS/frame time at 1920×1200 and native resolution; report p95/p99.
- Any precision artifact rejects the pass from the allowlist, not the whole
  mode.
