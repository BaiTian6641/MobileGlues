# AuroraLumen remix — Sildur / MakeUp / Fantasy audit and LUT-first changes

**Status:** PRIVATE / DEVICE PASS. The baked procedural nebula LUT loads at 55 FPS (2880×1585) with
no shader compile errors; final visual sign-off by the user is still pending.

## Reference packs

| Reference | Version | SHA-256 | License | Role |
| --- | --- | --- | --- | --- |
| Sildur's Vibrant Shaders | 2.02 Extreme | `824504bc937f9570277c0a809f5059ae4a5fe70c413cf4459b6ccff4dfed7ebe` | All rights reserved; modification/redistribution prohibited | Cheap screen-space godrays, distance fog, shadow/cloud structure |
| Sildur's Enhanced Default | 1.20 Fancy | `b30353b5dd831cb5ab060ee96e1ac762c571bb4de5d729d329463dff0f2f1235` | All rights reserved; no edit/copy/share | Minimal fog and effect layering |
| MakeUp - Ultra Fast | 9.6 | `71319ea5d0ad114143ef1c334890e315dbf1eeb2318d6bc35f3e5021fb3c8ba8` | LGPL-3.0 | 10-step 2.5D cloud slab; 4/6-sample godrays; fast fog |
| Fantasy Shaders Unbound | 1.11 | `02d5d63692dc9da1c68f5866f41da010a32bc78a207e8016db434d3533067044` | Complementary License 1.5 (derivative restrictions) | Colored shadow-map volumetric light, dense atmosphere |

No code from the all-rights-reserved packs was copied. The comparison is implementation-level:
loop counts, data layout, LUT use, and pass structure. MakeUp's LGPL code was also treated as a
reference only; the private derivative is not distributed.

## Audit conclusions

| Topic | Finding | Decision |
| --- | --- | --- |
| Fog | Sildur and MakeUp use cheap distance/height blends; Complementary's atmospheric fog is already richer | Keep Complementary's analytic height/distance fog; add one LUT noise fetch for low-altitude mist |
| Clouds | MakeUp uses a 10-step two-plane slab; Reimagined's 256² cloud/water LUT gives better shape at 1 fetch/step | Keep Reimagined LUT; cap the low profile at 24 steps and the HQ profile at 32 |
| Cloud shadows | Complementary implements a 1-2 tap cloud LUT shadow but leaves it disabled; the texture/uniform were incorrectly gated behind WSR | Fix the wiring and enable by default; disable only on lowest profiles |
| Volumetric light | MakeUp's 4-sample screen-space godrays are cheaper but lose shadow color; Complementary already uses 6 samples + colored shadow samples | Keep 6-sample `MOBILE` / 10-sample `MOBILE_HQ`; no screen-space downgrade |
| Aurora | Sildur uses 24 sky / 12 reflection slices; Complementary uses 25/10 | Already reduced to 14/8 on mobile |
| Nebula | Only Complementary has the 3-layer FBM; Sildur/MakeUp have no equivalent | Bake the procedural 3-layer field into a 256² LUT; the noise-texture LUT experiment produced blocky clumps and was rejected |

## Implemented in the remix slice

1. **Cloud-shadow wiring fix.** `cloudWaterTex` and its custom texture were declared only inside
   `WORLD_SPACE_REFLECTIONS > 0`; enabling `CLOUD_SHADOWS` without WSR would leave the sampler
   undeclared or unbound. The sampler and `customTexture.cloudWaterTex` are now unconditional under
   `CLOUD_SHADOWS`, and cloud shadows are enabled by default. Low profiles (`POTATO`, `VERYLOW`,
   `LOW`) explicitly disable them.
2. **LUT mist.** `MOBILE_FOG_MIST` adds one repeat-wrapped 128² noise-LUT fetch, height-masked to
   roughly 60-105 blocks. It modulates the existing atmospheric fog; no raymarch, pass, or history
   buffer is added.
3. **Cloud budget split.** `CLOUD_QUALITY=1` caps at 24 samples; `CLOUD_QUALITY>=2` caps at 32. This
   protects the low profile's horizon without changing the HQ setting.
4. **Baked nebula LUT.** The failed noise-texture FBM path was removed. A 256² RGBA LUT is generated
   offline from the original periodic three-layer, 5-octave procedurally hashed field. The shader
   reads three channels at the three animated layer coordinates, replacing 3×5 FBM octaves (60
   `sin` hashes) with three linear texture fetches. The original palette, alpha shaping, star boost,
   and layer motion are retained. `MOBILE_NEBULA_LUT=0` restores the procedural path for A/B.
5. **Minecraft semi-volumetric cloud style (`CLOUD_STYLE_DEFINE=4`).** The Reimagined 2.5D LUT is
   sampled on a quantized **12-block world grid**, with an **8-block-thick slab** and a fixed
   cell-density threshold. The march is capped at **12 samples**. This keeps a blocky Minecraft
   silhouette while giving the cells vertical thickness and the existing cloud lighting. The
   `MOBILE_MC_CLOUDS` profile is the mobile preset with style 4 selected. The default remains the
   smooth Reimagined style.

The low profiles now set `!CLOUD_SHADOWS` and `MOBILE_FOG_MIST=0`; `MOBILE` and `MOBILE_HQ` keep both.

## ACT / WSR performance audit

The first combined ACT + PBR/SSR + WSR profile measured **12 FPS** at 2880×1585. The duplicate
reflection work, not the LUT-able color table, was the dominant problem.

| Area | Cost | Decision |
| --- | --- | --- |
| WSR trace | 4× coarse occupancy DDA + fine 1× DDA over the whole volume | Added `WSR_MAX_DISTANCE` (48 blocks in `MOBILE_WSR`) and a 0.85 smoothness cutoff; rough/distant surfaces fall back to SSR/sky |
| WSR + SSR | `WORLD_SPACE_REF_MODE=2` ran both paths; `BLOCK=3/WATER=2` also enabled PBR/SSR | `MOBILE_WSR` now uses `WORLD_SPACE_REF_MODE=1`, `BLOCK=1`, `WATER=1` (WSR-only), no duplicated SSR pass |
| ACT floodfill | Six `texelFetch` neighbors per voxel, half-rate spread already enabled | No LUT: the volume is dynamic and scene-dependent; half-rate/shared-memory/downsampling are the applicable levers, not a baked table |
| ACT palette | `GetSpecialBlocklightColor` is a binary branch tree per light-source voxel, not per pixel | A 256-entry LUT is possible, but values depend on `END`, fire warmth, ore multipliers, XLIGHT, and candle settings; a texture fetch can be slower than the shallow branch tree. Deferred pending profiling |
| WSR hit shading | Full IPBR material chain per hit | Kept; the distance/smoothness gate cuts hit count first |
| DFG/PBR LUT | Classic split-sum LUT would replace some specular ALU | Not the measured bottleneck for WSR; deferred |
| Cloud/nebula/mist | Already LUT-based | Cloud-water 256², noise 128², baked nebula 256² |

Result: **29 FPS at 2880×1585** with `MOBILE_WSR` (ACT 128 + WSR-only + 48-block cap + 0.85
cutoff), versus 12 FPS with the duplicated SSR/WSR/PBR path. This is still below the default
profile, but it keeps ACT and a meaningful WSR trace instead of disabling either.

The LUT candidates that were **not** adopted are deliberate: a dynamic 3D floodfill cannot be
replaced by a static LUT, and the branchy palette is not the hot path. The useful LUTs are already
in the cloud, mist, cloud-shadow, and nebula paths.

## LUT strategy

The remix is LUT-first where a lookup removes real work:

| LUT | Size / binding | Consumer | Replaces |
| --- | --- | --- | --- |
| `cloud-water.png` | 256² RGBA, existing custom texture | Reimagined cloud shape, cloud shadows, water normals | Per-step procedural cloud density |
| `noise.png` | 128² RGB, existing | Aurora, mist, 3D smoke helpers | Multiple hash/octave evaluations |
| `cloudWaterTex` cloud shadow LUT | same 256², now correctly bound | Terrain/cloud-shadow lighting | A second cloud-density evaluation per fragment |
| `nebula-lut.png` | 256² RGBA, custom texture; SHA-256 `3c07ce8b0fceac187401935fb4bdef0f3d8581ca56a29d05ef162ad910693aaa` | Night sky | 3-layer × 5-octave procedural FBM (60 sin hashes) |

A full fog extinction LUT is not enabled: the existing analytic fog is one `exp` and a few
multiplies, while a LUT would add a dependent texture fetch and would need separate clear/rain
channels. The mist LUT is the useful part because it adds detail the analytic curve cannot express.

## Device evidence

Captured on `C1L63708PZ`, `AuroraLumen-Mobile-v2.zip`, Iris `MOBILE (+0 options changed)`:

| Scene | F3 | Notes |
| --- | --- | --- |
| Night aurora + nebula LUT | **55 FPS at 2880×1585**; p50 61, p98 25 | smooth curtains and diffuse nebula; no blocky clumps; no shader compile/fallback errors |
| Day, Minecraft semi-volumetric clouds | **40 FPS at 2880×1585**; p50 40, p98 37 | profile `MOBILE_MC_CLOUDS`; 12-block cells, 8-block slab, 12-step cap; no shader errors |
| Day, ACT 128 + WSR-only | **29 FPS at 2880×1585**; p50 35, p98 31 | profile `MOBILE_WSR`; 48-block cap, 0.85 cutoff, no SSR/PBR duplicate; ACT active; no shader errors |
| Same scene, duplicated SSR+WSR+PBR | 12 FPS at 2880×1585 | earlier profile; rejected |
| Earlier procedural nebula | 25 FPS at 2880×1585 | visual reference, rejected for night cost |
| Earlier noise-texture LUT | 53 FPS | rejected: blocky, oversized clumps |

The 55 FPS window is one F3 sample at 2880×1585, not a 1920×1200 or sustained benchmark. No shader
compile or fallback errors appeared in `latest.log`; the only Iris warning is the expected block-ID
map entry for `block.10104`.

The archive SHA-256 at the Minecraft-cloud device pass was
`d2a8024aab1bb9baf06c4a7f00ea5ae3b006dba488b3d39d73d1bb1c61a426ce`; at the ACT+WSR pass it was
`cc5628b1ef03296feb5a88572f6fde612ae40a6a6b5472c639fc6d386c171d79`. The final archive, including
the updated private notice, is
`6e8e19423968245e89bb482b2ed2ac9f8f063b935c16beaf77a78f0d0d4502b4`. Both installed
AuroraLumen entries were normalized to the same bytes; `AuroraLumen-Mobile-v2.zip` remains active.
Shader bytes are unchanged from the `cc5628…` WSR-tested build.

Evidence files (outside Git):

- `captures/2026-10-20-auroralumen-lut-nebula.png`
  SHA-256 `5b92f114e59e1f089bfe9420f975052309b519dd12063e80093eebca90a50d1b`
- `captures/2026-10-20-auroralumen-lut-f3.png`
  SHA-256 `fd49e0ac31fc583dbc688079d9f05ced596dea63db9cd9fca4378bea990f3de7`
- `captures/2026-10-20-auroralumen-lut-world.log`
  SHA-256 `5aa5f02a4f0a516368e84ae811e64dd0007406e0282240644627c69ec23f5b44`
- `captures/2026-10-20-auroralumen-mc-clouds.png`
  SHA-256 `efeb983ace1a817d63283936d69d69ed98a47b7ae8d93553b41311f13137234b`
- `captures/2026-10-20-auroralumen-mc-clouds-f3.png`
  SHA-256 `d4143a3b6c9a4bb1de16bd2cdef0ca60fff0c437a1cb9faf192309f5138b2cd4`
- `captures/2026-10-20-auroralumen-mc-clouds.log`
  SHA-256 `8a4285d24415d648d007d8bf19b27231bc4b3c872b13682bc63653055d81e464`
- `captures/2026-10-20-auroralumen-wsr-act.png`
  SHA-256 `a5be8a8eb7498a555c39fcd5c50be34b9e07a42a9176c4642d665390ecf220c8`
- `captures/2026-10-20-auroralumen-wsr-act-f3.png`
  SHA-256 `a9eba05ab7eed0ccebdfc9cd2d1c1a26f4b645b74adb8bca787a0f56d6f6bdd6`
- `captures/2026-10-20-auroralumen-wsr-act.log`
  SHA-256 `4e610ec0cac505787fb596b75018486dce7e88d471b5bddb83f5fe22b2db3b8e`

## Next gates

1. User visual sign-off on the LUT nebula; compare against the procedural look at several sky angles.
2. User visual sign-off on `CLOUD_STYLE=4`; check cell size, slab thickness, horizon aliasing, and
   whether the 12-sample cap is sufficient at low elevation.
3. Isolate cloud-shadow and mist on/off in a cloudy day scene; check for dark terrain bands or
   over-bright horizon haze.
4. Re-measure 1920×1200 and 10-minute steady state; current evidence is 2880×1585 and one F3 window.
5. Keep DH/Voxy static-contract checks separate from device qualification; neither mod is installed
   in the current test instance.

## Research notes

- NVIDIA, *Interactive Indirect Illumination Using Voxel Cone Tracing* — hierarchical voxel
  filtering for glossy reflections and GI: <https://research.nvidia.com/sites/default/files/pubs/2011-09_Interactive-Indirect-Illumination/GIVoxels-pg2011-authors.pdf>
- Iris ShaderDoc feature reference (light-block voxelization, custom images/textures):
  <https://github.com/IrisShaders/ShaderDoc/blob/master/iris-features.md>
- Rethinking Voxels, a Complementary-derived colored-light/voxel GI reference:
  <https://modrinth.com/project/kmwfVOoi>
- Split-sum DFG LUT is the standard PBR LUT candidate, but it does not replace the WSR DDA and was
  not the measured bottleneck here.
