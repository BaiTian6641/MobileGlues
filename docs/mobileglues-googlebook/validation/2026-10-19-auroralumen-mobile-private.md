# AuroraLumen Mobile — private X1E shaderpack design and first device pass

> Historical first pass. Superseded by `2026-10-20-auroralumen-remix-four-pack-audit.md`, which
> replaces the noisy nebula LUT with a baked procedural LUT and adds cloud shadows/mist. The hash
> and nebula details below describe the earlier build only.

**Status:** PRIVATE / EXPERIMENTAL. This is not a public shaderpack, not a release-qualified
replacement for Complementary, and not part of the MobileGlues repository's shader sources.

## Base and license

| Field | Value |
| --- | --- |
| Base | Complementary Reimagined r5.9.3 (unmodified archive) |
| Base archive SHA-256 | `fed6c879e732cf7fc7bc6570ebb1def43235aa7e96f62859fbdbdbfa0b484b38` |
| Private derivative | `/home/polar/MobileGlues-fixtures/AuroraLumen-Mobile/` |
| Private archive | `/home/polar/MobileGlues-fixtures/AuroraLumen-Mobile.zip` |
| Private archive SHA-256 | `bab6822663a51f7965491f6225036343a79901c6c1068008198469ac6090afe6` |
| License | Complementary License Agreement 1.7, retained in `License.txt` |
| Distribution | Not distributed. The derivative does not claim Section 1.3 redistribution rights. |

The original pack archive was not modified. The private tree is not committed. The derivative exists
only for the author and a small private group, consistent with the user's stated intent.

## Audit inputs

The design was based on three read-only audits of the r5.9.3 source plus comparison against BSL
v10.1.8, iterationRP, and AuraLite:

- **Hot paths:** TAA (~40 taps/pixel), water/SSR reflection chains (30/38 dependent steps), cloud
  marching (12 to hundreds of samples near the horizon), aurora (25 slices in `deferred1`, 10 in
  reflections), nebula (three 5-octave sin-hash FBM layers), and a no-op `composite3` copy.
- **Portability:** no subgroup/geometry/tessellation dependency; all legacy compatibility GLSL is
  translated by MobileGlues + Iris. `RGB8_SNORM` is not a complete X1E attachment. The installed
  `r16ui`/`r8ui` image formats are substituted with `r32ui`; WSR 256/512 exceed the 128 MiB SSBO
  limit. Player reflections are a measured performance/power failure.
- **External ideas:** AuraLite's profile-scoped quality and half-resolution thinking; BSL's
  four-channel noise packing and aurora color bands; iterationRP's low-resolution sky cache and
  analytic cloud light march. These were used as design references, not copied code.

| Reference | Adopted | Deferred / rejected |
| --- | --- | --- |
| AuraLite (CC BY-NC-SA) | profile-scoped quality; bounded full-screen work | half-resolution bloom/sky buffers (needs new passes and history plumbing) |
| BSL v10.1.8 | single-noise-LUT idea; aurora color-band palette | 4-channel packed noise texture; full BSL cloud replacement |
| iterationRP | temporal low-res sky-cache idea; analytic cloud-light march idea | 128³ NUBIS noise and full sky recompute in reflections |
| Complementary Unbound | none for the default | 4-octave `Noise3D` cloud march is heavier than the Reimagined LUT on X1E |
| Complementary Reimagined | base cloud LUT, aurora/nebula structure, lighting palette | per-hit shadow-map cloud fetch and unbounded cloud samples |

## Design

The private pack keeps Complementary Reimagined's cloud LUT, aurora curtain construction, nebula
palette, and lighting palette. It adds an Android/X1E default path and bounds the worst cases.

### Profiles

| Profile | Intent | Key settings |
| --- | --- | --- |
| `MOBILE` | default, 1920×1200-class | shadow 1 / 128 m, water 1 (mirrored), block 1, light shafts 1, clouds 1, TAA, FXAA, no ACT/WSR |
| `MOBILE_HQ` | heavier 1920×1200 | shadow 2 / 160 m, water 2 (ray-marched), light shafts 2, clouds 2, no PBR SSR/ACT/WSR |
| `MOBILE_ACT` | advanced colored light | MOBILE + ACT 192, no WSR; stays under the 128 MiB SSBO limit |
| `MOBILE_WSR` | advanced reflection experiment | MOBILE + ACT 128 + WSR 128; optional, not the default |
| `ULTRA` / `COMPLEMENTARY` | legacy profiles | capped to ACT 192 and WSR off; WSR 256/512 are not selectable |

The shader defaults in `lib/common.glsl` now match `MOBILE`, so Iris selects `MOBILE` on first load.
`MOBILE_MODE=1` is a hidden compile switch; setting it to 0 recovers the original loop budgets.

### Code changes

| Area | Change | Reason |
| --- | --- | --- |
| Aurora | 25→14 slices in `deferred1`, 10→8 in reflections; visibility condition default 0 | Keeps the curtain signature, removes ~44% of night-sky fetches, makes aurora visible on clear nights |
| Nebula | enabled; 4 octaves; `perlin` reads the 128² noise LUT instead of four `sin` hashes | Removes ~48 transcendental hashes per octave-set; preserves the three-layer blue/amber/pale structure |
| Clouds | 32-sample cap; `continue`→`break` once the ray passes opaque depth | Bounds the sub-degree horizon spike; skips the rest of the march over terrain |
| Clouds | aurora/nebula tint scales with cloud alpha | Free, strengthens the night cloud/aurora interaction |
| Reflections | mobile SSR 30→20 steps, 6→4 refinements | Shortens the dependent chain; `MOBILE` uses mirrored water reflections |
| TAA | four cardinal neighbours instead of eight; hoisted linear-depth terms | 16 fewer dependent fetches/pixel; keeps temporal accumulation |
| Post | `composite3` disabled when `WORLD_BLUR == 0` | It is a pure `colortex0` copy in that configuration |
| Attachments | `colortex1` `RGB8_SNORM`→`RGBA8_SNORM` | X1E FBO-complete attachment; avoids wrapper substitution |
| Iris compatibility | Pale Garden/Sulfur Caves/end-flash custom uniforms replaced with static zero | Iris 1.8.14 on 1.21.1 cannot resolve those identifiers; warnings suppressed unrelated effects |
| Stars | one-hash blackbody tint | Cheap night-sky variation; no extra texture fetch |

No subgroup operations, hardware ray tracing, new image formats, or new SSBOs were added.

## Compatibility gates

| Surface | Status | Evidence / limitation |
| --- | --- | --- |
| MobileGlues / ANGLE ES 3.2 | **device pass** | FCL loaded the pack; Iris `Profile: MOBILE`; no shader compile/fallback errors in `latest.log` |
| Desktop GLSL → MobileGlues | **host + device pass** | Host glslang: 12/12 active paths pass, both `MOBILE_MODE=1` and `0` for the changed passes |
| Distant Horizons | **static contract only** | `dh_terrain`/`dh_water` programs retained and host-compiled with `DISTANT_HORIZONS`; `dhShadow.enabled=false`, `dhClouds=off` deliberately retained. DH not installed in the test instance |
| Voxy | **static contract only** | `voxy.json`, `voxy_opaque`, and `voxy_translucent` retained; not installed in the test instance. No Voxy-specific shader edits |
| Vulkan | **not claimed** | Pack remains compatibility-profile GLSL (`texture2D`, `gl_FragData`, fixed-function built-ins). A Vulkan path needs separate core-profile sources, explicit descriptors, and attachment layouts |
| ACT / WSR | **not tested in this pass** | ACT 192 and WSR 128 profiles exist but were not selected on device; WSR 256/512 remain blocked |

The DH choice is intentional: Iris 1.8.14/DH can break vanilla shadows when `dhShadow` is enabled, and
the pack has no qualified DH cloud implementation. Keeping DH terrain/water and disabling those two
overrides avoids presenting a known-bad path as supported.

## Device evidence

Captured on `C1L63708PZ` (HP Googlebook 14, X1E80100, Adreno X1-85, Android 17 / API 37):

| Field | Value |
| --- | --- |
| Provider | ANGLE 2.1 / Vulkan 1.4.295 / GLES 3.2 |
| Renderer | MobileGlues 2.0.0 (installed test build) |
| Loader | NeoForge 21.1.262, Iris 1.8.14-beta.1, Sodium 0.8.13 |
| Shaderpack | `AuroraLumen-Mobile-v2.zip` (same bytes as the private archive hash above) |
| Iris profile | `MOBILE (+0 options changed by user)` |
| F3 | 33 FPS at 2880×1585; p50 34, p98 31, p99.5 10; GPU 100% |
| Visual | Night world: aurora curtains, stars, cloud layer, snow, torch/block light |
| Shader errors | None in `latest.log`; only expected Iris block-ID map warnings |

Evidence files (outside Git):

- `captures/2026-10-19-auroralumen-mobile-night.png`
  SHA-256 `5a098aab78b4191667eb76689595041c503f3c9dd74172585f2f8e0d1f397b7e`
- `captures/2026-10-19-auroralumen-mobile-f3.png`
  SHA-256 `0f82f1a81bbd66a73de144f5a3897c62d3ff15dbdba31e2d4cec44a223c98c78`
- `captures/2026-10-19-auroralumen-mobile-world.log`
  (Iris profile/pack/provider lines and no shader compile failures)

The 33 FPS value is **one F3 window at 2880×1585**, not a 1920×1200 benchmark and not a sustained
median. It is above the 30 FPS target at a higher pixel count, but the 1920×1200 and 20-minute gates
remain open.

## Host validation

`glslangValidator` was run on recursively preprocessed `world0` wrappers with `MC_VERSION=12101`,
`IRIS_VERSION=10814`, `TAA`, and `DISTANT_HORIZONS` where applicable:

```
PASS composite.fsh, composite1.fsh, composite4.fsh, composite5.fsh,
     composite6.fsh, composite7.fsh, deferred1.fsh, final.fsh,
     gbuffers_water.fsh, gbuffers_terrain.fsh,
     dh_terrain.fsh, dh_water.fsh
12 / 12
```

The changed composite/deferred/water programs also pass with `MOBILE_MODE=0`. The host harness
injects the Iris compatibility environment; it proves GLSL structure, not device effect fidelity.

## Open gates

1. Measure 1920×1200, 10-minute steady state, and per-effect A/B (aurora, nebula, clouds, light
   shafts, reflections, TAA).
2. Validate DH terrain/water on a DH-installed instance; decide whether `dhShadow` can ever be
   enabled without the Iris/DH interaction.
3. Validate Voxy programs on a Voxy-installed instance; the current evidence is source-contract only.
4. A/B the nebula LUT against the procedural look; A/B the 32-sample cloud cap near the horizon.
5. Check TAA four-neighbor ghosting on diagonal geometry; revert to six/eight if visible.
6. ACT 192 / WSR 128 profiles remain untested; WSR 256/512 and player reflections stay blocked.
7. A Vulkan path remains a separate renderer project, not a shaderpack claim.

## References

- Iris Distant Horizons contract: <https://shaders.properties/current/reference/mod-support/distant_horizons>
- Iris shader settings/profiles: <https://shaders.properties/current/reference/shadersproperties/shader_settings/>
- AuraLite Shaders (reference only): <https://github.com/AlexanderNyr/AuraLite-Shaders>
- Complementary Reimagined r5.9.3, BSL v10.1.8, and iterationRP Alpha 0.8.31 fixtures under
  `/home/polar/MobileGlues-fixtures/`; hashes belong in Git, archives do not.
