# Minecraft/Iris frame graph and temporal-input audit for FSR3

Status: read-only source audit, 2026-10-20. No game, pack, shader ZIP, Iris/Sodium source, or device
was modified. No device command was run. This report separates:

- **[SOURCE]** a statement read directly from a pinned source tree or archive member;
- **[RUNTIME-REPORTED]** a result recorded by the project but not re-run here;
- **[INFERENCE]** a conclusion that follows from the cited source but was not directly observed;
- **[UNRESOLVED]** a question this audit could not close from the available material.

`[PACK-r593]` and `[PACK-r592]` mean the original, unmodified ZIP members extracted for reading.
Line numbers are member-relative (`shaders/...`), not lines in an extracted working tree that could be
edited. `[IRIS]` is the Iris `1.21.1` source at
`IrisShaders/Iris@eb7afb99f747` (the branch tip at the time the audited 1.8.14-beta.1 jar was
published). `[SODIUM]` is `CaffeineMC/sodium@dae4167e7d32` (`mc1.21.1-0.8.13`).
`[MG]` is the plugin core submodule `8ac7036f5ac5903536f382937114f32b15fa2da1`.

## 1. Lane identity and provenance

| Lane | Engine tuple | Iris | Sodium | Evidence status |
| --- | --- | --- | --- | --- |
| **Supported delivery lane** | Minecraft 1.21.1, NeoForge 21.1.262, Java 21 (`java-runtime-delta`), LWJGL 3.3.3 | `1.8.14-beta.1+1.21.1-neoforge`, Modrinth `KduFYu4t` | `mc1.21.1-0.8.13-neoforge`, Modrinth `uMOpc5uV` | Runtime staged/working; effect/TAA/performance gates open. `docs/03_MobileGlues_Googlebook_Validation_and_Sources.md:11-19`, `docs/mobileglues-googlebook/execution-state.md:16-29` |
| **Legacy lane** | Minecraft 1.20.1, Java 17, Fabric | `1.7.6`; local source is a modified snapshot, not a release binary | `0.5.12` / `0.5.13` candidate; local build metadata names a `0.5.12` snapshot | Source-only; no installed legacy game binary or device run verified here. `docs/03...:16`, `/home/polar/Iris/build.gradle.kts:15,18` |
| **Future/experimental lane** | Minecraft 26.3, Java 25, LWJGL 3.4.3, SDL/ShaderC/SPIRV-Cross | Release `1.11.7` per docs; audited `26.3` branch source is `1.11.6` | `0.9.2` per docs | Not qualified; no device. Do not mix with 1.21.1 findings. `docs/03...:15,385-403`; `Iris-26.3/build.gradle.kts:20` |
| Optional comparison | Minecraft 1.21.11, Java 21 | `1.10.8` | `0.8.14` | Not part of the required lane. `docs/03...:17` |

### 1.1 Exact assets used

| Asset | Identity | SHA-256 |
| --- | --- | --- |
| Complementary Reimagined r5.9.3 | `/home/polar/MobileGlues-fixtures/ComplementaryReimagined_r5.9.3.zip` | `fed6c879e732cf7fc7bc6570ebb1def43235aa7e96f62859fbdbdbfa0b484b38` |
| Complementary Reimagined r5.9.2 | `/home/polar/MobileGlues-fixtures/ComplementaryReimagined_r5.9.2.zip` | `7ae5f1d6a4237a5c8893578397e58c9d08b79d7403bc27242aef56957489a278` |
| Iris NeoForge jar | `target-1.21.1-neoforge/iris-neoforge-1.8.14-beta.1+mc1.21.1.jar` | `60d5f8bf52f25e9986440f4a4270a6bd986d9cff994510e1108ac1547063b314` |
| Sodium NeoForge jar | `target-1.21.1-neoforge/sodium-neoforge-0.8.13+mc1.21.1.jar` | `575d187b15328d7bddd8a9b1bee64333c22c14735f5780ec49eac0cd5503c08d` |

**Provenance limits.** The jar `META-INF/neoforge.mods.toml` reports `1.8.14-beta.1+mc1.21.1`;
Modrinth version `KduFYu4t` was published `2026-06-13T15:57Z`; the `1.21.1` branch tip
`eb7afb99f747` is dated `2026-06-13T15:38Z` and its `build.gradle.kts:20` says `1.8.14`. The
source-to-binary match is therefore an **inference from version and timestamp, not a byte-for-byte
rebuild**. Sodium's source tag resolves to `dae4167e7d32`; the jar is a Modrinth release. Neither jar
was decompiled to prove every class matches the source commit. [UNRESOLVED: reproducible jar/source
comparison.]

### 1.2 Runtime evidence is reported, not reproduced

The project records that the 1.21.1 / NeoForge 21.1.262 tuple loaded, rendered an original pack, and
saved/quit, and later that r5.9.2 High and r5.9.3 High initialized without fallback; ACT 192 and
WSR 128 are staged. Those are **[RUNTIME-REPORTED]** in
`docs/mobileglues-googlebook/execution-state.md:50-67,290-300,335-355`, not observations made here.
No effect-by-effect TAA/history qualification exists. `execution-state.md:362-364` explicitly lists
TAA/history isolation as open.

## 2. Actual frame graph on the supported lane

This section describes the Iris 1.8.14 path with Sodium 0.8.13 terrain. The pack in the audited
runtime is Complementary Reimagined; the pack supplies the `gbuffers_*`, `deferred*`, `composite*`,
and `final` programs. Iris supplies the pass schedule and targets; Sodium supplies terrain draws.

### 2.1 Ordered pass schedule

| Order | Producer | Writes / binds | Evidence |
| --- | --- | --- | --- |
| 0 | Frame setup | captures `gbufferModelView` / `gbufferProjection`; starts frame counter/time | `[IRIS] common/src/main/java/net/irisshaders/iris/mixin/MixinLevelRenderer.java:83-96`; `uniforms/MatrixUniforms.java:20-38`; `uniforms/SystemTimeUniforms.java:26-30` |
| 1 | Level begin / clears | clears images and shadow targets; dispatches `shadowcomp` compute before shadow geometry; clears configured color targets | `[IRIS] pipeline/IrisRenderingPipeline.java:847-906`, especially `:885-890`; `[PACK-r593] shaders/shaders.properties:109-112` |
| 2 | Shadow map | shadow viewport; shadow terrain, entities, block entities, translucent shadow terrain; then shadow composite | `[IRIS] shadows/ShadowRenderer.java:357-375,462-475,578-581`; `[IRIS] mixin/MixinLevelRenderer.java:155-158` |
| 3 | Opaque world / gbuffers | Sodium solid + cutout terrain; Iris binds the gbuffer FBO/program before the draw; entities, block entities, particles, sky, clouds use Iris gbuffer programs | `[SODIUM] .../render/SodiumWorldRenderer.java:266-276`; `[SODIUM] .../render/chunk/DefaultChunkRenderer.java:44-104`; `[IRIS] compat/sodium/mixin/MixinShaderChunkRenderer.java:34-43`; `[IRIS] pipeline/programs/SodiumPrograms.java:154-180` |
| 4 | Hand solid | copies current depth to `depthtex2` (pre-hand), renders solid hand, then copies depth to `depthtex1` (pre-translucent) | `[IRIS] mixin/MixinLevelRenderer.java:290-295`; `[IRIS] pipeline/IrisRenderingPipeline.java:1045-1065`; `[IRIS] pathways/HandRenderer.java:73-112` |
| 5 | Deferred | runs `deferred*` before world translucent geometry; writes scene color / reflection / cloud / VL targets | `[IRIS] pipeline/IrisRenderingPipeline.java:1054-1067`; `[PACK-r593] shaders/program/deferred1.glsl:436-460` |
| 6 | Translucent world | water, translucent entities, weather, etc.; pack writes color, material, normal, reflection targets | `[PACK-r593] shaders/program/gbuffers_water.glsl:288-305`; `shaders/program/gbuffers_entities.glsl:259-278` |
| 7 | Hand translucent | rendered at `renderLevel` RETURN before composite | `[IRIS] mixin/MixinLevelRenderer.java:131-135`; `[IRIS] pathways/HandRenderer.java:114-148` |
| 8 | Composite chain | `composite`, `composite1` … `composite7` in array order; each pass binds its FBO, sets its viewport to the target size, runs optional compute, then a full-screen quad | `[IRIS] pipeline/CompositeRenderer.java:253-312`; `[IRIS] shaderpack/programs/ProgramSet.java:163-171`; `[IRIS] pipeline/CompositePass.java:3-8` |
| 9 | Final | pack `final` reads `colortex3` and writes the Minecraft main color texture; if there is no final program, colortex0 is copied; flipped buffers are copied back to main | `[IRIS] pipeline/FinalPassRenderer.java:194-270,280-292`; `[PACK-r593] shaders/program/final.glsl:100-160` |
| 10 | Color-space finish | `finalizeGameRendering` processes the main color texture | `[IRIS] pipeline/IrisRenderingPipeline.java:1093-1094`; `[IRIS] mixin/MixinGameRenderer.java:494-497` |
| 11 | HUD / inventory / chat | vanilla GUI composition over the main framebuffer, outside Iris | **[INFERENCE]** from Iris having no HUD pass and finishing the main target in step 10; the exact 1.21.1 client call order was not source-verified here |

### 2.2 Composite-chain order actually present in Complementary

Iris loads `composite`, `composite1`, `composite2`, … with a maximum of 100 entries
(`[IRIS] shaderpack/programs/ProgramSet.java:163-171`). At the audited High profile:

| Pass | Actual role at High | Outputs / key reads | Evidence |
| --- | --- | --- | --- |
| `composite` | reflection trace + temporal reflection accumulation | writes `colortex7`; also `colortex1` when PBR reflections; reads depth/color/material | `[PACK-r593] shaders/program/composite.glsl:100-226` |
| `composite1` | fog, sky/reflection composite, translucency handling | writes `colortex0`; conditionally `colortex5` | `[PACK-r593] shaders/program/composite1.glsl:127-150,325-344` |
| `composite2` | reserved/no-op | copies `colortex0` to itself | `[PACK-r593] shaders/program/composite2.glsl:19-29` |
| `composite3` | world blur / DOF; default `WORLD_BLUR 0`, so it copies `colortex0` unchanged | writes `colortex0` | `[PACK-r593] shaders/lib/common.glsl:164`; `shaders/program/composite3.glsl:126-162` |
| `composite4` | bloom tiles; motion blur is off by default | writes `colortex3`; writes `colortex0` too only with motion blur | `[PACK-r593] shaders/lib/common.glsl:148`; `shaders/program/composite4.glsl:62-82,181-187` |
| `composite5` | bloom composite, color grading, tonemap, lens flare, saturation | reads `colortex0`; writes `colortex3` | `[PACK-r593] shaders/program/composite5.glsl:160-236` |
| `composite6` | **final TAA** | reads `colortex3`, `depthtex1`, `colortex6`, `colortex2`; writes `colortex3` + `colortex2` | `[PACK-r593] shaders/program/composite6.glsl:16-45`; `shaders/lib/antialiasing/taa.glsl:134-251` |
| `composite7` | FXAA; enabled at High | reads `colortex3`, depth, `colortex2`; writes `colortex3` | `[PACK-r593] shaders/program/composite7.glsl:27-37`; `shaders/lib/antialiasing/fxaa.glsl:155-195` |
| `final` | underwater distortion, chroma, sharpen, error text, SSBO clear, vignette, dither | reads `colortex3`; writes main color | `[PACK-r593] shaders/program/final.glsl:95-160` |

High-profile defaults that determine this order: `DETAIL_QUALITY=2`, `FXAA_DEFINE=1`,
`COLORED_LIGHTING=0`, `WORLD_SPACE_REFLECTIONS=-1` (`shaders/shaders.properties:6`); `RP_MODE=1`,
`BLOCK_REFLECT_QUALITY=3`, `PBR_REFLECTIONS` on, `BLOOM_ENABLED=1`, `MOTION_BLUR_EFFECT=-1`,
`IMAGE_SHARPENING=5` (`shaders/lib/common.glsl:16,26,145-148,520-522`; `shaders/shaders.properties:6`);
`WORLD_BLUR=0` (`shaders/lib/common.glsl:164`).
At `DETAIL_QUALITY>=2` and `TAA_DEFINE=-1`, `TAA` is defined (`common.glsl:547-550`).

### 2.3 Render targets and formats

The pack declares its formats as `const` lines inside a `/* ... */` block
(`[PACK-r593] shaders/lib/pipelineSettings.glsl:1-17`). **Iris still consumes them.** Iris's directive
parser splits source by line, trims each line, and accepts any line beginning with `const`; it does
not strip comments (`[IRIS] shaderpack/parsing/ConstDirectiveParser.java:20-48,60-105`), and
`ProgramSet` feeds those lines to the target directives (`[IRIS] shaderpack/programs/ProgramSet.java:255-269`).
The target format parser then accepts `colortexNFormat` (`[IRIS] shaderpack/properties/PackRenderTargetDirectives.java:104-113`).
If a directive were truly absent, the default is unsized `RGBA`
(`PackRenderTargetDirectives.java:127-135`), so the comment convention is load-bearing here.

| Logical target | Declared format | Declared role | Size | Clear |
| --- | --- | --- | --- | --- |
| `colortex0` | `R11F_G11F_B10F` | main HDR scene color before tonemap | main/render size | true |
| `colortex1` | `RGB8_SNORM` | half-res normal/reflection history | `REFLECTION_RES` (0.5) of main | false |
| `colortex2` | `RGB16F` | **TAA history** | main/render size | false |
| `colortex3` | `RGBA8` | translucent mult, bloom, final LDR color | main/render size | true |
| `colortex4` | `RGBA8_SNORM` | normal / reflection strength | main/render size | false |
| `colortex5` | `RGBA8` | water-reflection scene / cloud depth / VL factor | main/render size | false |
| `colortex6` | `RGB8` | smoothness, **material mask**, sky light | main/render size | true |
| `colortex7` | `RGBA16F` | **reflection temporal history** + previous depth | `REFLECTION_RES` (0.5) of main | false |
| `colortex8` | `RGBA16F` | SSR results / topmost translucent opacity | main/render size | false |
| `colortex12` | `RGBA8` | improved rain | main/render size | true |
| `colortex13` | `R8` | entity linear depth | main/render size | true |
| `colortex18` | `R8` | voxy shadow | main/render size | false |
| `colortex19` | `RGBA8` | voxy reflections | main/render size | false |

Evidence: formats and comments `[PACK-r593] shaders/lib/pipelineSettings.glsl:1-17`; clears
`:19-32`; `size.buffer.colortex1/7 = REFLECTION_RES` `[PACK-r593] shaders/shaders.properties:118-122`;
`REFLECTION_RES` default `0.5` `shaders/lib/common.glsl:240`. The relative override is implemented as
`original * relative` in `[IRIS] gl/texture/TextureScaleOverride.java` and applied at allocation in
`[IRIS] targets/RenderTargets.java:122-128,193-201`.

At High, `COLORED_LIGHTING=0` and `WORLD_SPACE_REFLECTIONS=-1`, so ACT/WSR are off; however
`BLOCK_REFLECT_QUALITY=3` still defines `PBR_REFLECTIONS`, so the reflection history in
`colortex1/7` is active at half resolution. Do not describe `colortex7` as WSR-only.

**Known provider boundary.** The X1E provider probe records `RGB8_SNORM` as texture-valid but not
color-renderable, while `RGBA8_SNORM`, `R11F_G11F_B10F`, `RGB16F`, and `RGBA16F` render
(`docs/mobileglues-googlebook/execution-state.md:105-106`). This is a provider fact, not a pack
qualification. Any FSR3 profile that causes `colortex1` to be rendered must recheck this boundary.

### 2.4 Where scene color and depth are bound

- **Scene color:** gbuffer programs write `colortex0` (HDR scene color) plus material/normal targets
  through a gbuffer FBO. The FBO is built from the pack's `DRAWBUFFERS` and the current flip state
  (`[IRIS] pipeline/programs/SodiumPrograms.java:131-140`; `[IRIS] targets/RenderTargets.java:332-369`).
- **Scene depth:** the gbuffer FBO attaches `currentDepthTexture`, which is the **Minecraft main
  depth texture**, not an Iris-owned depth texture (`[IRIS] targets/RenderTargets.java:296-305`).
  Iris re-attaches it if the main depth texture/version changes (`:147-183`).
- **`depthtex0`:** this main depth. It includes the hand and translucents by the time composites run.
- **`depthtex1`:** copied at `beginTranslucents`, i.e. after hand-solid and before world translucents
  (`[IRIS] pipeline/IrisRenderingPipeline.java:1063-1065`; `[IRIS] targets/RenderTargets.java:209-219`).
- **`depthtex2`:** copied at `beginHand`, i.e. before the hand (`IrisRenderingPipeline.java:1048-1050`;
  `RenderTargets.java:221-230`).
- **Composite FBOs:** color attachments only; depth is sampled, not attached. `FinalPassRenderer`
  creates `colorHolder` and attaches the main color texture
  (`[IRIS] pipeline/FinalPassRenderer.java:117-121,215-226`).
- **Final output:** `final` writes `colorHolder` at `main.width/height`
  (`FinalPassRenderer.java:198-200,221-255`); otherwise colortex0 is copied
  (`:257-270`). It does not create a display target of its own.

### 2.5 Size, viewport, and ping-pong conventions

- Iris allocates every logical color target from `main.width/height`, multiplied by the pack's
  `size.buffer` override (`[IRIS] targets/RenderTargets.java:122-128,193-201`). It has **no runtime
  render-scale parameter**. The launcher/surface size is the render size base.
- Composite passes compute their viewport from their own target dimensions and the pass's
  `viewportScale` (`[IRIS] pipeline/CompositeRenderer.java:292-296`). Pack gbuffer/geometry draws do
  not get an Iris-side scaled viewport; Sodium uses the current viewport and Iris only resets it for
  shadow rendering (`[IRIS] compat/sodium/mixin/MixinShaderChunkRenderer.java:52-57`). A true
  lower-resolution scene therefore cannot be achieved by changing one allocation: all geometry
  viewports and the final/post targets must be coherent.
- Each `RenderTarget` owns a main and an alternate texture
  (`[IRIS] targets/RenderTarget.java:40-62`). `DRAWBUFFERS`/`RENDERTARGETS` entries are flipped after
  a pass writes them (`[IRIS] pipeline/CompositeRenderer.java:137-174`), and the next pass reads the
  appropriate variant through `stageReadsFromAlt` (`[IRIS] targets/BufferFlipper.java`). At the end
  of the frame Iris copies flipped alternate content back to main; its own comment says this is not
  a full swap (`[IRIS] pipeline/FinalPassRenderer.java:123-145,280-292`).
- TAA history (`colortex2`) and reflection history (`colortex7`) are not cleared per frame
  (`[PACK-r593] shaders/lib/pipelineSettings.glsl:19-26`), which is why they can persist. If a pass
  writes zero to `colortex2` (for example when TAA is disabled but composite6 still runs), the
  history is effectively invalidated for the next frame
  (`[PACK-r593] shaders/program/composite6.glsl:35-45`).

**Consequence for FSR3.** The current Iris target graph has one size per logical target and a final
pass that writes the main framebuffer. MobileGlues' existing FSR1 swap hook redirects framebuffer 0
and upscales it at presentation (`[MG] egl/egl.cpp:404-417`; `[MG] gl/FSR1/FSR1.cpp:432-477`), which
is after Iris has already produced a native-resolution main image. It is not a lower-resolution Iris
scene path.

## 3. Temporal ownership

### 3.1 Final TAA is owned by the pack, not Iris

`composite6` is the final TAA pass:

- it reads `colortex3` (LDR color), `depthtex1`, and `colortex6` (material mask);
- it calls `DoTAA` only when `TAA` is defined;
- it writes the resolved color to `colortex3` and the new history to `colortex2` with
  `DRAWBUFFERS:32`;
- `colortex3MipmapEnabled = true` is declared in the same pass.

Evidence: `[PACK-r593] shaders/program/composite6.glsl:13-45`; `DoTAA` and reprojection
`shaders/lib/antialiasing/taa.glsl:59-78,95-251`.

`DoTAA` behavior relevant to FSR3:

- Reprojection is **camera-only**: it reconstructs view space from `depthtex1`, adds
  `cameraPosition - previousCameraPosition`, then multiplies by previous model-view/projection
  (`taa.glsl:59-78,137-174`). There is no per-object term.
- It only reprojects when `z1 > 0.56`; nearer pixels (hand/close entities) keep `prvCoord = texCoord`
  and blend in place (`taa.glsl:171-174`).
- It rejects history for material-mask bands around 149.5 (entity-reflection handling) and 254
  (no-SSAO/no-TAA/reduced-reflection), and for mask 253 treats leaves as high-edge
  (`taa.glsl:134-156,208-212`; mask table `shaders/lib/common.glsl:844-870`).
- It uses a 3×3 neighborhood (8 neighbors) with depth-edge detection and an AABB history clamp
  (`taa.glsl:95-132`).
- History is sampled with `textureCatmullRom` when `TAA_MOVEMENT_IMPROVEMENT_FILTER=1`, otherwise
  with `texture2D` (`taa.glsl:197-201`).
- If history is zero/NaN, the current color becomes the new history
  (`taa.glsl:203-206`). This is the first-frame reset path.

**Iris 1.8.14 has no TAA/jitter implementation.** There is no jitter uniform, resolve pass, or
temporal history in the Iris source path audited; the only `velocity` uniform is a camera-speed
scalar in `HardcodedCustomUniforms` (`[IRIS] uniforms/HardcodedCustomUniforms.java:54`), not
per-object motion. Final temporal ownership is entirely the pack's.

### 3.2 Jitter is partial and per-vertex

The pack owns jitter too:

- `TAAJitter` uses 8 hard-coded offsets and `framemod8` (`[PACK-r593] shaders/lib/antialiasing/jitter.glsl:1-24`).
- `framemod8` is a pack custom uniform, `frameCounter % 8` (`shaders/shaders.properties:241-245`);
  `frameCounter` is Iris's per-frame counter (`[IRIS] uniforms/SystemTimeUniforms.java:26-30`).
- With default `TAA_JITTER=1`, the offset is scaled by `0.125`; `0.33` for value 2
  (`jitter.glsl:16-20`; defaults `shaders/lib/common.glsl:157-160`).
- The jitter is applied **inside vertex shaders**, not to the projection matrix. It appears in
  `gbuffers_terrain`, `gbuffers_water`, `gbuffers_block`, `gbuffers_clouds`, `gbuffers_basic`,
  `gbuffers_beaconbeam`, `dh_terrain`, `dh_water`, `voxy_opaque`, and `voxy_translucent`
  (for example `program/gbuffers_terrain.glsl:145-147,228-232,476-478`;
  `program/gbuffers_water.glsl:418-420`).
- It is **not** applied in `gbuffers_entities`, `gbuffers_hand`, `gbuffers_textured`,
  `gbuffers_skybasic`, `gbuffers_skytextured`, `gbuffers_spidereyes`, `gbuffers_lightning`, or
  `gbuffers_weather`; those files do not contain `TAAJitter`. Hand vertex output is plain
  `ftransform()` (`program/gbuffers_hand.glsl:229-234`).

Consequences:

1. There is no single scene-wide jitter value that describes the rasterized image. Terrain and some
   effect geometry are jittered; entities, particles, hands, and several textured/effect programs
   are not.
2. `gbufferProjection` is **not jittered**; the pack's reprojection uses the unjittered projection
   plus per-vertex offsets. A temporal upscaler that is told a jitter value must be given the actual
   raster jitter, which this design does not currently provide globally.
3. If FSR3 reuses the pack's 8-phase jitter, only part of the scene has that subpixel offset. That is
   not a valid FSR3 input, even though it is adequate for the pack's own clamped TAA.

### 3.3 Effect histories and denoisers

| History / effect | Storage | Lifetime / reset | Owner |
| --- | --- | --- | --- |
| TAA color history | `colortex2` RGB16F, full render size, `clear=false` | reset per-pixel on zero/NaN or mask rejection; pass writes new history each frame | pack `composite6` |
| Reflection color + previous depth | `colortex7` RGBA16F, `REFLECTION_RES` (0.5), `clear=false` | camera/previous-normal reprojection; history blend in `composite`; composite1 consumes | pack `composite` / `composite1` |
| Reflection normal history | `colortex1` RGB8_SNORM, `REFLECTION_RES` (0.5), `clear=false` | written as `gl_FragData[1]` by `composite` when PBR reflections | pack `composite` |
| ACT flood-fill light volume | `floodfill_img` / `floodfill_img_copy` RGBA16F 3D images | ping-pong by `framemod2`; previous volume is read and next written; per-frame `voxel_img` clear is a separate behavior | pack `shadowcomp` |
| WSR voxel volume / LOD | `wsr_img` R16UI 3D, `wsr_lod_img` R8UI 3D, SSBO | fixed dimensions independent of screen size | pack `shadow` / WSR compute |
| Player reflection atlas | `playerAtlas_img` 64×64 RGBA8 + `bufferObject.3` | optional; default off | pack WSR/player-ref path |
| Center depth smoothing | Iris `CenterDepthSampler` | time-smoothed sample for packs using `centerDepthSmooth` | Iris, not a screen history |
| Bloom / blur / FXAA / sharpen | no history | spatial only | pack composite4/5/7/final |

Evidence: `[PACK-r593] shaders/lib/pipelineSettings.glsl:2-32`;
`shaders/program/composite.glsl:220-227`; `shaders/program/composite1.glsl:142-187`;
`shaders/program/shadowcomp.glsl:36-38,100-152`; `shaders/program/shadow.glsl:261-271`;
`shaders/shaders.properties:160-227`; `[IRIS] pathways/CenterDepthSampler.java` (if a pack uses it).
The reflection history is a camera/previous-normal reprojection, not a TAA resolve; it has its own
ghosting failure modes and must not be reset merely because FSR3 resets its own history.

### 3.4 Motion data actually available

| Data | 1.21.1 / Iris 1.8.14 | 1.20.1 / Iris 1.7.6 | 26.3 future |
| --- | --- | --- | --- |
| Current view/model-view matrix | yes: `gbufferModelView`, `gbufferProjection` and inverses | yes, same uniform family | yes |
| Previous view/model-view matrix | yes: `gbufferPreviousModelView`, `gbufferPreviousProjection` | yes | yes |
| Camera position / previous | yes: `cameraPosition`, `previousCameraPosition` | yes | yes |
| Frame counter/time | yes: `frameCounter`, `frameTime`, `frameTimeCounter` | yes | yes |
| Scene depth | yes: `depthtex0`, `depthtex1`, `depthtex2` | yes | yes |
| Entity/material IDs | yes: `entityId`, `mc_Entity` / `iris_Entity` | yes | yes |
| Per-object velocity | **no**; `IrisVertexFormats.ENTITY` has no `at_velocity` (`[IRIS] vertices/IrisVertexFormats.java:49-60`); Sodium program binds no velocity (`[IRIS] pipeline/programs/SodiumPrograms.java:158-167`) | **no**; legacy source has no velocity element (`/home/polar/Iris/src/main/java/net/irisshaders/iris/vertices/IrisVertexFormats.java:41-76`) | **not in the 26.3 branch**; an unmerged `at_velocity` branch exists |

The `at_velocity` branch is a prototype, not a release facility. It is based on `1.8.0-beta.4`
(`/tmp/irisvel/Iris-at_velocity/build.gradle.kts:14`), adds `VELOCITY_ELEMENT` to particle/entity
formats (`vertices/IrisVertexFormats.java:43-63`), binds attribute 15 in `ProgramCreator`, and
computes per-vertex deltas with a 3-frame reset (`pipeline/QuadPositions.java:5-39`). It does not
appear in the audited 1.8.14, 1.7.6, or 26.3 source. A grep of the 26.3 tree finds only the
camera-speed `velocity` custom uniform, not `at_velocity`.

The pack's `current-to-previous` reprojection is therefore the only motion source available on the
supported lane. It can reconstruct **static-scene** motion from depth and camera matrices, but it
cannot produce correct motion for independently moving geometry.

One implementation caveat: Iris's `gbufferPrevious*` matrices are a lazy supplier that replaces its
stored "previous" value whenever the uniform is read (`[IRIS] uniforms/MatrixUniforms.java:66-84`).
An adapter must capture current/previous view-projection at a defined frame boundary; it should not
opportunistically read those uniforms from different programs and assume one frame's pair.

## 4. Hands, HUD, and UI boundary

- **Hand solid** is rendered after the opaque world and before `beginTranslucents`; `beginHand`
  copies `depthtex2` first, so the pack can distinguish pre-hand depth
  (`[IRIS] mixin/MixinLevelRenderer.java:290-295`; `IrisRenderingPipeline.java:1045-1050`).
- **Hand translucent** is rendered after the world, at `renderLevel` RETURN, and before the
  composite chain (`MixinLevelRenderer.java:131-135`; `HandRenderer.java:114-148`).
- Both hand paths write into the same gbuffer color targets and main depth as the world; they are
  part of the scene color. The pack's hand vertex shader is not TAA-jittered
  (`[PACK-r593] shaders/program/gbuffers_hand.glsl:229-234`), and the hand material has no velocity
  data.
- **HUD/inventory/chat** are not written to `colortex0..8`; Iris finishes the main framebuffer, then
  vanilla GUI composition occurs outside the pipeline. The exact 1.21.1 client call order is
  [INFERENCE]; Iris source only proves that it has no HUD pass and finishes the main target in
  `finalizeGameRendering` (`[IRIS] mixin/MixinGameRenderer.java:494-497`).

For FSR3, the safe boundary is: scene (including hands if the profile accepts hand ghosting) →
FSR3 → pack finishing → **native-resolution UI**. FSR3 must not receive HUD/inventory/chat pixels.
If hands are excluded from FSR3 and composited afterward, their depth/occlusion and pack lighting
relationship changes; that is a separate profile decision, not a free fix.

## 5. Complementary r5.9.3 / r5.9.2 integration boundary

### 5.1 r5.9.3 versus r5.9.2

The two original ZIPs are nearly identical for this audit:

- `shaders/program/dh_water.glsl`, `gbuffers_entities.glsl`, and `gbuffers_hand.glsl` differ.
- r5.9.3 adds `IRIS_INLINE_GLINT` handling in entities/hand; r5.9.2 does not.
- r5.9.3 adds a `translucentMult` initialization in `dh_water`.
- `common.glsl`, `shaders.properties`, `pipelineSettings.glsl`, `composite*.glsl`, `final.glsl`,
  `taa.glsl`, and `jitter.glsl` are byte-identical between the two archives.

Therefore the pass graph, final-TAA location, jitter sequence, sample-count default, and target
layout are the same in r5.9.2 and r5.9.3. A TAA/FSR3 change must be qualified on both, but a visual
difference caused by inline glint or the `dh_water` initialization is not a TAA/FSR3 regression.
Diff basis: `diff -rq` of the two extracted `shaders/` trees; the three changed files are the only
differences.

### 5.2 What changes when final TAA is replaced

If FSR3 replaces `composite6`:

1. **Final TAA is gone.** `colortex3` is no longer resolved by `DoTAA`; FSR3 becomes the temporal
   resolve. Keeping `composite6` as an appended pass would double-resolve and is not acceptable.
2. **`colortex2` loses its writer.** `composite6` writes new TAA history to `colortex2`; skipping it
   leaves stale/zero history. `composite7` FXAA reads `colortex2` to decide `skipFXAA`
   (`[PACK-r593] shaders/lib/antialiasing/fxaa.glsl:166-190`). Either FSR3 must publish a compatible
   history/confidence value to `colortex2`, or the profile must set `FXAA_TAA_INTERACTION=0` and
   revalidate FXAA. Simply leaving the buffer stale is wrong.
3. **Jitter must become global or zero.** The pack's per-vertex 8-phase jitter is not a valid FSR3
   jitter (section 3.2). The qualified profile should either:
   - keep `TAA` defined for the pack's effect branches, set `TAA_JITTER=0`, stop calling `DoTAA`,
     and inject the actual jitter into the projection used by all scene draws; or
   - disable TAA/jitter and accept that every `#ifdef TAA` branch changes.
   The second option is not a drop-in: `TAA` gates dither/noise, reflection-noise scaling, banding
   fixes, and shadow sampling branches in `composite`, `composite1`, `composite5`, `deferred1`,
   `gbuffers_*`, and `shadowSampling` (`[PACK-r593] shaders/program/composite.glsl:110-112`;
   `composite1.glsl:127-130,291-292,321-322`; `composite5.glsl:188-191`;
   `deferred1.glsl:226-228`; `lib/lighting/shadowSampling.glsl:32,135`). Keeping `TAA` defined and
   disabling only jitter/resolve preserves more of the pack's intended effect behavior.
4. **Sample-count coupling.** The pack's own TAA uses one history sample per frame with an 8-phase
   jitter sequence, not an 8-sample accumulation in one frame. `TAA_SMOOTHING=3`,
   `TAA_MOVEMENT_IMPROVEMENT_FILTER=1`, and `blendMinimum/blendVariable/blendConstant` determine its
   blend (`taa.glsl:1-18,245-248`). FSR3's frame/jitter sequence must be independent; do not assume
   the pack's `framemod8` phase equals an FSR3 frame index, and do not feed the pack's partial jitter
   to FSR3 as if it were global.
5. **Color stage.** `composite6` operates on post-tonemap `colortex3` RGBA8, after `composite5`'s
   tonemap and color grading. The exact encoding (perceptual/sRGB/gamma) is not declared in the
   pack; this is [UNRESOLVED]. If FSR3 replaces this slot, it is an LDR profile. A higher-quality
   variant would run FSR3 earlier, on `colortex0` HDR after `composite1`, before `composite4`'s
   bloom and `composite5`'s tonemap; that requires a display-resolution HDR target and revalidation
   of bloom/tonemap resolution. The pack has no separate HDR post target today.
6. **Finishing order.** `composite7` FXAA and `final` sharpen/chroma/vignette/dither currently run
   after TAA. If FSR3 replaces TAA, those should run **after** FSR3 at display resolution; otherwise
   the image is sharpened/FXAA'd at render resolution and then upscaled. `final` reads `colortex3`
   (`[PACK-r593] shaders/program/final.glsl:100-110`) and `composite7` reads/writes `colortex3`
   (`composite7.glsl:29-36`), so the output target must be display-sized before those passes.

### 5.3 Scene sampler mip bias

- The pack enables mipmaps on scene targets for bloom: `colortex0MipmapEnabled` in `composite4`,
  `colortex3MipmapEnabled` in `composite6`, and `colortex5MipmapEnabled` in `composite`
  (`[PACK-r593] shaders/program/composite4.glsl:13`; `composite6.glsl:16`;
  `composite.glsl:20`). Iris generates the mips and switches the sampler filter before the pass
  (`[IRIS] pipeline/CompositeRenderer.java:202-226`).
- Iris does not expose a scene-wide texture LOD bias. Its texture wrappers set
  `GL_TEXTURE_LOD_BIAS` to `0.0` for custom/gl textures (`[IRIS] gl/texture/GlTexture.java:45-48`;
  `gl/image/GlImage.java:64-67`) and there is no render-scale bias control in the audited pipeline.
- A lower-resolution scene changes implicit LOD selection for world textures. The pack's
  `ANISOTROPIC_FILTER`/`miplevel` paths and `breaksAnisotropy = true`
  (`[PACK-r593] shaders/shaders.properties:158`) make this especially visible on distant textures,
  foliage, and fences.
- The FSR3 profile must apply the pinned version's recommended scene-sampler bias only to the
  world/scene samplers, not globally to shadow/lookup/volume/post/UI samplers, and must account for
  existing pack bias and AF. There is no proven ANGLE/MobileGlues path for this in the audited code;
  [UNRESOLVED], requires backend and visual tests.

### 5.4 ACT / WSR / player-reflection regression cases

These are independent temporal systems. FSR3 history resets must not silently reset them, and
lowering scene resolution must not double-scale their screen-relative resources.

| Case | What exists | FSR3-specific risk | Required regression |
| --- | --- | --- | --- |
| ACT colored lighting | `shadowcomp` compute dispatches before shadow geometry; writes `floodfill_img` / `floodfill_img_copy` with `framemod2` ping-pong; `shadow` writes `voxel_img`; sizes 128/192/256/384/512/768/1024 (`[PACK-r593] shaders/program/shadowcomp.glsl:36-38,100-152`; `shaders/program/shadow.glsl:261-271`; `shaders/shaders.properties:160-190`) | fixed-size volume is not screen-scaled, so FSR3 should not change its cost; a global temporal reset must not clear the volume. `voxel_img` per-frame clear is separately required. | ACT 192: distinct sea-lantern/glowstone colors, no flicker/ghosting over motion; log/image units; `voxel_img` clear. Runtime is staged, not effect-qualified (`execution-state.md:335-343,362-364`). |
| WSR voxel reflections | `wsr_img` R16UI `COLORED_LIGHTING×64×COLORED_LIGHTING`; `wsr_lod_img` R8UI; SSBO `bufferObject.0` 50,855,936 B at 128; reflection history `colortex7` half-res; `composite` writes it, `composite1` consumes (`[PACK-r593] shaders/shaders.properties:197-221`; `composite.glsl:220-227`; `composite1.glsl:142-187`) | If scene scale changes, `size.buffer.colortex1/7 = REFLECTION_RES` follows the new main size; ensure the FSR scale is not applied twice. Reflection history is a separate camera-reprojected history; FSR3 does not fix its ghosting. | WSR 128: water/room reflections stable in motion; no provider crash; player refs off for supported target; WSR 192 optional; WSR 256 blocked by 128 MiB SSBO (`execution-state.md:54-60,101-104,343-355`). |
| Player reflections | `playerAtlas_img` 64×64 RGBA8 + `bufferObject.3` 5376 B when `WORLD_SPACE_PLAYER_REF == 1`; default `-1` (`[PACK-r593] shaders/shaders.properties:223-226`; `shaders/lib/common.glsl:31-45`; `final.glsl:151-152`) | separate atomics/vertex image/SSBO path; functional staged pass but performance/power fail. Not covered by FSR3 camera-motion work. | Player-ref on/off separately; expect staged function, `FAIL/NOT QUALIFIED` performance. `execution-state.md:205-219`. |
| r5.9.2 regression | TAA/pass graph identical to r5.9.3; only three shader files differ | do not attribute glint/`dh_water` differences to FSR3 | Run same FSR3 profile on r5.9.2 and r5.9.3; compare TAA/reflection/ACT/WSR rows separately. |
| Iris/pack diagnostics | Custom-uniform warnings (`BIOME_PALE_GARDEN`, `BIOME_SULFUR_CAVES`, `endFlashIntensity`) on 21.1.262 / 1.8.14-beta.1 | can suppress effects and confuse FSR3 qualification; not a renderer failure to paper over | resolve with compatible Iris/pack tuple; do not edit pack ZIP. `execution-state.md:365-367`. |

## 6. Adapter contract proposal

These are **project contract concepts**, not existing Iris/MobileGlues/FSR APIs. Names and ABI are
placeholders; the frame identity/lifetime fields follow the FSR3 research plan's contract.

### 6.1 Semantic inputs

| Group | Required semantics | Source on supported lane | Status / limitation |
| --- | --- | --- | --- |
| Identity | frame id, graphics session id, share-group/context id, surface id, resource generation | Iris pipeline/session; main target color/depth versions (`[IRIS] pipeline/FinalPassRenderer.java:119-121,215-219`; `RenderTargets.java:147-153`) | Must reject stale/mismatched generation |
| Dimensions | allocated render extent, active rectangle, display extent, origin convention | `main.width/height`; pack `size.buffer` relative overrides | No runtime render scale in Iris today; active rect is full target unless a profile says otherwise |
| Color | current scene color, HDR/LDR, encoding, exposure/pre-exposure, premultiplied/straight alpha | `colortex0` HDR R11F before tonemap; `colortex3` RGBA8 after tonemap | LDR encoding unspecified; HDR profile needs a display HDR target |
| Depth | sampleable current depth, depth range/convention, reversed/infinite flag, near/far | `depthtex0` (visible), `depthtex1` (pre-translucent), `depthtex2` (pre-hand); pack's `near=0.05`, `far=renderDistance*16`, standard `[0,1]` linearization (`[PACK-r593] shaders/lib/antialiasing/taa.glsl:22-24`; `[IRIS] uniforms/CameraUniforms.java:26-34`) | Declare which depth texture is the reconstruction depth; do not silently use `depthtex1` for a final scene that includes translucents/hand |
| Motion | current-to-previous pixel-space vector, units/sign/origin, jitter inclusion, confidence/coverage | Camera matrices + depth; no per-object data | Camera-only; synthetic test must encode “object moves +2 px right ⇒ motion.x = −2 at current pixel” |
| Jitter | actual per-frame offset in render pixels/NDC; sequence id | Pack 8-phase jitter; partial per-vertex | Not global; must be replaced or zeroed for FSR3 |
| Temporal | actual jitter, frame time, reset flag/reason, history continuity | `frameCounter`, `frameTime`, Iris session | Reset policy below |
| Camera | near/far, vertical FOV radians, coordinate scale | pack uniforms and projection | Verify FOV extraction from projection; do not assume shaderpack `fov` uniform |
| Masks | reactive mask, transparency/composition mask | `colortex6` material mask 0–255; depth discontinuities; entity/hand identification | Reactive mask is a damage-control input, not motion; do not mark all pixels reactive |
| Output | display-res color view, color space, alpha policy | new display-res target / main color before UI | Must not include HUD/inventory/chat |
| Scheduling | producer completion, accepted stage id, consumer lifetime, GPU fence | Iris render thread; same EGL/GLES context | No CPU readback/global wait in normal frames |

### 6.2 Pass boundary

**Recommended primary boundary for Complementary qualification: replace `composite6`.** FSR3
consumes `colortex3` (LDR scene), `depthtex0` (or a declared depth view), and a generated
camera-motion texture; it writes a **display-resolution** color target. Then the pack's
`composite7` FXAA and `final` sharpen/chroma/vignette/dither run at display resolution, and the
final pack pass writes the Minecraft main color. `composite6`'s `DoTAA` must be bypassed, not
appended.

**Higher-quality optional boundary:** run FSR3 after `composite1` on `colortex0` HDR, before
`composite4` bloom and `composite5` tonemap; then bloom/tonemap/FXAA/final operate at display
resolution. This preserves HDR but changes bloom and tonemap resolution and needs its own
qualification. Do not claim it from the LDR profile.

**Rejected boundary:** upscaling at `eglSwapBuffers` or after `final`. By then Iris has already
rendered and finished a native-resolution world image and the HUD is composed over it. The existing
FSR1 path is a presentation-time default-FBO upscale (`[MG] egl/egl.cpp:404-417`;
`[MG] gl/FSR1/FSR1.cpp:432-477`), not an Iris scene-input provider.

**Implementation prerequisite.** Iris 1.8.14 has one size per target and a final pass that writes
the main framebuffer. A display-resolution post target plus render-resolution scene targets requires
an Iris-side adapter/mixin (target allocation, pass viewport, sampler redirection) or an equivalent
engine change. It is not achievable through the existing MobileGlues swap hook. This is the main
`[UNRESOLVED]` integration item.

### 6.3 Native UI ownership

- FSR3 output is scene-only; the pack's finishing passes operate on it before UI composition.
- HUD, inventory, chat, debug overlay, and menus stay at display resolution and are never inputs to
  FSR3.
- Do not reuse the FSR1 swap path for FSR3; it would upscale/compose whatever the main framebuffer
  contains at swap time.

### 6.4 Reset triggers

Reset or invalidate FSR3 history on:

1. First frame after enabling FSR3, and any history-zero/NaN event.
2. Surface resize, target reallocation, depth texture/version change
   (`[IRIS] targets/RenderTargets.java:147-205`; `IrisRenderingPipeline.java:920-933`).
3. Graphics-session/context/share-group loss or surface-only recreation.
4. Iris pipeline create/destroy, dimension change, level load/unload, shader-pack reload.
5. Camera teleport (the camera tracker itself shifts at >1000 blocks;
   `[IRIS] uniforms/CameraUniforms.java:69-97`) and any perspective/FOV change or camera cut.
6. Pack option changes that alter TAA/jitter/WSR/ACT/reflection history.
7. Render-scale/output-extent change.
8. `frameCounter` discontinuity/wrap or an explicit producer reset flag.

Do **not** reset ACT/WSR volumes merely because FSR3 history resets. Their temporal state has its
own contract; `voxel_img` clear and `floodfill` ping-pong are pack/bridge behavior, not FSR3
history.

### 6.5 Per-lane limitations

| Lane | Camera-only static scene | Independently moving objects | Jitter | Per-object motion | Render-scale path |
| --- | --- | --- | --- | --- | --- |
| 1.21.1 / Iris 1.8.14 / Sodium 0.8.13 | Supportable with depth + current/previous matrices | Not correct: entities, particles, hands, animated foliage, waving water, Create contraptions, vehicles; use reactive mask only as damage control | pack 8-phase, partial per-vertex; must be replaced/zeroed | none | requires Iris adapter; no runtime scale today |
| 1.20.1 / Iris 1.7.6 / Sodium 0.5.12 | same camera-only basis | not correct; no velocity attribute | pack-owned, same idea but older source; profile-specific | none | requires legacy adapter; no installed binary/device qualified here |
| 26.3 / Iris 1.11.7 / Sodium 0.9.2 | future lane; engine shader path differs (ShaderC/SPIRV-Cross) | `at_velocity` not in 26.3 branch; unmerged prototype only | not verified | prototype entity/particle velocity; not terrain/foliage | future; do not mix with 1.21.1 |

## 7. What camera-only versus independently moving objects supports today

**Supportable today (with a correct motion pass and masks):**

- Static opaque terrain, static blocks, sky, clouds treated as a static layer, and other geometry
  whose motion is fully explained by camera translation/rotation.
- Static translucency can be reprojected with the same camera model, but depth discontinuities and
  the pack's `depthtex1`/`depthtex0` choice need explicit testing; glass/water/particles are not
  static in general.

**Not supportable today:**

- Entities, players, mobs, item frames, armor stands, and animated models: no previous transform or
  velocity in the supported Iris/Sodium source.
- Particles: no velocity attribute in 1.8.14.
- Hands: rendered before composite, not jittered, no velocity; camera reprojection either ghosts the
  hand or requires a reactive mask that sacrifices accumulation.
- Foliage/grass/leaves with vertex waving, water waves, lava, and other vertex-animated geometry:
  camera-only reprojection treats them as static; they ghost.
- Create contraptions/vehicles and modded animated geometry: worse than vanilla entities; no engine
  motion contract.
- Effects with their own history (reflections/WSR) remain independently ghost-prone; FSR3 replacing
  final TAA does not repair them.

**`at_velocity` status:** not available on the supported lane. The unmerged branch supplies
per-vertex velocity for particle/entity formats only; no terrain, foliage, or hand coverage was
found in the inspected format and mixin files. Do not advertise moving-object support from that
branch without a release provenance and a separate coverage proof.

## 8. Unresolved questions

1. **Exact jar/source identity.** The Iris/Sodium source commits are inferred from release timing
   and tag; no reproducible rebuild or class-hash comparison was performed.
2. **HDR versus LDR boundary.** `composite6` is LDR post-tonemap; the pack does not declare its
   encoding. A camera capture is needed to determine whether an LDR FSR3 profile is acceptable, or
   whether the HDR boundary after `composite1` must be implemented.
3. **Display-resolution post target.** How to allocate/alias a display-res target after FSR3 while
   keeping the pack's `composite7`/`final` passes and their sampler names is not solved. Iris has no
   runtime target-scale API; this needs an Iris-side adapter or patch.
4. **Scene-sampler mip bias.** No audited Iris/MobileGlues path applies a per-scene-sampler LOD bias
   at reduced render resolution, and `breaksAnisotropy = true` makes this visible. Needs a backend
   and visual test across distant textures/foliage/fences.
5. **Jitter replacement.** Whether the pack can keep `TAA` defined, set `TAA_JITTER=0`, and receive
   a global projection jitter without changing `#ifdef TAA` effect branches needs a controlled
   shader-transform test. The current per-vertex jitter is not a valid global input.
6. **FXAA interaction.** With `composite6` bypassed, `colortex2` history/confidence and
   `FXAA_TAA_INTERACTION` behavior are undefined. Either publish compatible history or disable the
   interaction and requalify FXAA.
7. **ACT/WSR at changed scene scale.** `colortex1/7` are relative half-res targets; verify that a
   render-scale change does not double-apply the reflection scale and that reflection history resets
   independently of FSR3.
8. **Player reflections.** Functional staged pass but performance/power fail; no FSR3 support claim.
9. **Hands.** Native-resolution hand compositing after FSR3 is not proven to preserve pack lighting,
   depth, and occlusion.
10. **Backend formats.** GLES compute/image support for FSR3, including any required R11F/RGB16F
    input conversions, is owned by the backend spike; this audit does not establish it.
11. **HUD call order.** Vanilla 1.21.1 GUI composition after `finalizeGameRendering` is inferred from
    Iris hooks, not from a pinned Minecraft 1.21.1 client source in this workspace.

## Appendix A. Source reference index

### Iris 1.8.14 source (`1.21.1@eb7afb99f747`)

- `common/src/main/java/net/irisshaders/iris/pipeline/IrisRenderingPipeline.java`
  - `:265` render-target construction from main size
  - `:847-906` level begin, shadow compute dispatch, clears
  - `:1045-1067` `beginHand`, `beginTranslucents`, deferred order
  - `:1085-1094` composite/final/color-space finish
- `common/src/main/java/net/irisshaders/iris/pipeline/FinalPassRenderer.java`
  - `:117-121` final color holder attaches main color
  - `:194-270` final pass / colortex0 fallback
  - `:280-292` flipped buffer copy-back
- `common/src/main/java/net/irisshaders/iris/pipeline/CompositeRenderer.java`
  - `:137-174` drawbuffers/flip/target size
  - `:253-312` pass execution, viewport, FBO bind
- `common/src/main/java/net/irisshaders/iris/targets/RenderTargets.java`
  - `:122-128` target allocation and scale override
  - `:147-205` resize/depth reattach
  - `:209-230` `depthtex1`/`depthtex2` copies
  - `:296-305` gbuffer FBO with main depth
  - `:332-369` main/alt attachment selection
- `common/src/main/java/net/irisshaders/iris/mixin/MixinLevelRenderer.java`
  - `:83-96` matrix capture
  - `:122-135` begin/end level and hand translucent/finalize
  - `:155-158` shadow rendering
  - `:290-295` beginHand/hand solid/beginTranslucents
- `common/src/main/java/net/irisshaders/iris/mixin/MixinGameRenderer.java:494-497` color-space finish
- `common/src/main/java/net/irisshaders/iris/pathways/HandRenderer.java:73-148` hand phases
- `common/src/main/java/net/irisshaders/iris/uniforms/MatrixUniforms.java:20-84` matrices and previous capture
- `common/src/main/java/net/irisshaders/iris/uniforms/CameraUniforms.java:26-34,69-97` camera position/history
- `common/src/main/java/net/irisshaders/iris/uniforms/SystemTimeUniforms.java:26-30` frame counter/time
- `common/src/main/java/net/irisshaders/iris/uniforms/HardcodedCustomUniforms.java:54` camera-speed `velocity`
- `common/src/main/java/net/irisshaders/iris/shaderpack/parsing/ConstDirectiveParser.java:20-105` line-wise const parsing
- `common/src/main/java/net/irisshaders/iris/shaderpack/properties/PackRenderTargetDirectives.java:104-135` format/clear/default
- `common/src/main/java/net/irisshaders/iris/shaderpack/programs/ProgramSet.java:163-171,255-269` composite array order and directives
- `common/src/main/java/net/irisshaders/iris/compat/sodium/mixin/MixinShaderChunkRenderer.java:34-57` Sodium FBO/program bind/shadow viewport
- `common/src/main/java/net/irisshaders/iris/pipeline/programs/SodiumPrograms.java:131-140,154-180` gbuffer FBO and attributes
- `common/src/main/java/net/irisshaders/iris/vertices/IrisVertexFormats.java:49-60` entity format without velocity
- `common/src/main/java/net/irisshaders/iris/shadows/ShadowRenderer.java:357-375,462-475,578-581` shadow viewport/order

### Sodium 0.8.13 source (`dae4167e7d32`)

- `common/src/main/java/net/caffeinemc/mods/sodium/client/render/SodiumWorldRenderer.java:266-276`
  solid/cutout/translucent terrain render layers
- `common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/DefaultChunkRenderer.java:44-104`
  region draw loop and matrices
- `common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/ShaderChunkRenderer.java`
  (Iris redirects its program/framebuffer at `MixinShaderChunkRenderer`)

### Complementary Reimagined r5.9.3 archive members

- `shaders/shaders.properties:2-9` profiles; `:29-33` TAA/UI options; `:100-112` shadow/composite7/shadowcomp toggles; `:118-122` half-res targets; `:158` anisotropy; `:160-227` ACT/WSR/player images/SSBO; `:241-245` custom `framemod8`
- `shaders/lib/pipelineSettings.glsl:1-32` formats (commented but parsed) and clears
- `shaders/lib/common.glsl:31-45` WSR/player options; `:157-164` TAA/jitter/FXAA defaults; `:240` `REFLECTION_RES`; `:520-550` TAA define; `:844-870` material-mask table
- `shaders/lib/antialiasing/jitter.glsl:1-24` 8-phase jitter
- `shaders/lib/antialiasing/taa.glsl:59-78,95-132,134-251` reprojection/clamp/DoTAA
- `shaders/program/composite6.glsl:13-45` final TAA and history write
- `shaders/program/composite5.glsl:160-236` tonemap/finishing to `colortex3`
- `shaders/program/composite4.glsl:62-82,181-187` bloom/motion-blur outputs
- `shaders/program/composite.glsl:100-226` reflection history
- `shaders/program/composite1.glsl:127-150,325-344` fog/reflection composite
- `shaders/program/composite7.glsl:27-37` FXAA
- `shaders/program/final.glsl:95-160` LDR read/sharpen/vignette/SSBO clear
- `shaders/program/deferred1.glsl:436-460` deferred outputs
- `shaders/program/gbuffers_terrain.glsl:145-147,228-232,476-478` jitter use
- `shaders/program/gbuffers_water.glsl:288-305` translucent outputs; `:418-420` jitter use
- `shaders/program/gbuffers_entities.glsl:259-278` entity outputs (no jitter)
- `shaders/program/gbuffers_hand.glsl:229-234` hand vertex (no jitter)
- `shaders/program/shadowcomp.glsl:36-38,100-152` ACT ping-pong
- `shaders/program/shadow.glsl:261-271` ACT/WSR voxel writes
- `shaders/lib/antialiasing/fxaa.glsl:166-190` FXAA uses `colortex2` and camera delta

### MobileGlues core 8ac7036

- `egl/egl.cpp:404-417` FSR1 presentation hook
- `gl/FSR1/FSR1.cpp:158-212` FSR1 target/render scale calculation (render = surface size, target = scaled larger)
- `gl/FSR1/FSR1.cpp:432-477` FSR1 pass and blit to framebuffer 0
- `gl/framebuffer.cpp:170-200` default framebuffer redirect to FSR1 render FBO

## Appendix B. Bottom line for the coordinator

1. On the supported 1.21.1 lane, Iris has **no TAA, jitter, or motion-vector ownership**. The pack
   owns an 8-phase per-vertex jitter and the final TAA in `composite6`; Iris owns pass order, targets,
   and the camera/previous-camera uniforms.
2. Camera-only motion is the only honest motion source today. Static world geometry can be
   reprojected; entities, particles, hands, foliage, water animation, vehicles, and Create
   contraptions cannot.
3. Replacing final TAA means replacing `composite6`, not appending a pass. That also invalidates the
   pack's `colortex2` history used by FXAA, removes the pack's partial jitter, and changes every
   `#ifdef TAA` effect branch unless a profile keeps `TAA` defined with `TAA_JITTER=0`.
4. A true lower-resolution Iris scene plus a display-resolution post/finish chain is not available
   in the current target graph. It requires an Iris-side target/pass adapter; the MobileGlues FSR1
   swap hook is presentation-only.
5. Complementary r5.9.2 and r5.9.3 share the TAA/pass graph; differences are inline glint and one
   `dh_water` initialization. Both need the same FSR3 profile, but their non-TAA visual differences
   must not be blamed on FSR3.
6. ACT/WSR/player reflections are independent temporals. FSR3 resets must not clear them; changing
   scene scale must not double-scale the half-res reflection history.
