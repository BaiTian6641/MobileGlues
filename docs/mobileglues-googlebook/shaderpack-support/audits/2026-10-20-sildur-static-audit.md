# Sildur's series static adoption audit

Date: 2026-10-20. Read-only source audit; no host/device compile.
Targets: Vibrant 2.02 Extreme, Enhanced Default 1.20 Fancy, Basic 2.7 Fancy.
Original ZIPs are All-Rights-Reserved local fixtures; no source is copied here.

## Verdict

All top-level programs are `#version 120` vertex/fragment pairs. There are no
compute, geometry, tessellation, image, SSBO, atomic, or subgroup
dependencies. ES 3.2 feature coverage is sufficient.

The "clearly won't work" hypothesis is wrong if it means raw legacy GLSL
cannot be consumed. Iris 1.8.x preprocesses and patches these packs to
`#version 330 core`, rewriting `attribute`/`varying`, `gl_FragData`,
`texture2DLod`/`GradARB`, `shadow2D`, `ftransform`, and fixed-function inputs.
The real risk is Iris metadata/FBO formats, then generated ESSL/ANGLE compile
capacity. S0/S1 remains **NOT RUN**.

## First risks

| ID | Pack/program | Evidence | Class | Smallest fix | Gate |
| --- | --- | --- | --- | --- | --- |
| B1 | Vibrant `gbuffers_textured`, world copies | `shaders/gbuffers_textured.fsh:23-34` declares `colortex1/3=RGB8`, `colortex2/7=RGB16F`, `colortex4/5/6=R11F_G11F_B10F`; `colortex1` is in `DRAWBUFFERS:412`; `colortex3` is in `composite.fsh` `DRAWBUFFERS:35` | Provider/FBO format + Iris comment-directive parsing | Extend the core legal/renderable target table for RGB8; if not renderable, widen truthfully to an accepted format, never a silent shader fallback | S0/S1 |
| B2 | Enhanced `gbuffers_textured` | `.../gbuffers_textured.fsh:21-25` declares `gnormalFormat=RGB10_A2` | Provider/FBO format | Verify RGB10_A2 FBO completeness; widen only the internal target if incomplete | S0/S1 |
| B3 | Vibrant `composite1`, `deferred`, `composite2` | 855-line/29 KB programs; default VPS 8, shadow samples 12, bloom loops, 3072 shadow map | GPU compile/time capacity | Measure compile/link time and frame cost; no effect removal | S1/S3 |
| B4 | Vibrant custom uniforms | `shaders.properties:85-86` `BiomeTemp`, `framemod8` | Iris metadata | Qualify tuple or document suppression; no pack edit | S2 |
| B5 | Raw legacy path | `texture2DLod`, `shadow2D`, `ftransform` | Translator compatibility, masked by Iris | Host negative diagnostic only; do not gate on raw 120 | Diagnostic |
| B6 | Enhanced `shadow.fsh` | `DRAWBUFFERS:02` on shadow program | Iris directive/FBO quirk | Verify shadow FBO draw state | S1/S2 |
| B7 | DH/Voxy programs | `dh_terrain`, `voxy.json` / `voxy.glsl` | Host-only optional mods | Exclude from first import; separate lane | NOT RUN |

SILD-02A now confirms plain RGB8 and RGB10_A2 are FBO-complete on the target
provider (`validation/2026-10-20-x1e-sildur-format-probe.md`). `RGB8_SNORM`
remains non-renderable, but Sildur does not request it. The B1/B2 format-creation
risks are closed; host shader compile/link is the next gate.

## Pack differences

| Dimension | Vibrant | Enhanced | Basic |
| --- | --- | --- | --- |
| Programs / default active | 21 / 15 | 11 / 9 | 8 / 7 |
| Formats | RGB8, RGB16F, R11F | RGBA8, RGB10_A2 | RGBA8 only |
| Shadows | PCSS, colored | colored | none |
| Effects | bloom, godrays, TAA, SSAO, reflections, volumetrics | TAA, SSAO, godrays, reflections | TAA, SSAO, fog, crossprocess |
| Risk | highest | medium | lowest control |

## First host probes

F0 format/FBO probe is the prerequisite: RGB8, RGB10_A2, RGB16F, R11F, RGBA8.

1. Vibrant `gbuffers_textured`, default `nMap=0`: parsed formats and
   `DRAWBUFFERS:412`, 3 outputs, PCSS, translated 330-core source.
2. Vibrant `composite1`: `textureLod`, TAA/bloom/godrays, link and uniform count.
3. Vibrant `deferred`: `shadow2D`/`sampler2DShadow`, 2 outputs, PCSS.
4. Then `gbuffers_water`, `composite2`, `final`, followed by Enhanced and Basic.

Use the actual Iris preprocessing/patching path. A raw-120-only compile is a
negative diagnostic, not a gate. Do not commit preprocessed source.

## Device import order

1. Vibrant S0, then S1. If FBO creation fails, record the exact format.
2. If Vibrant fails, import Basic first as the all-RGBA8 control to distinguish
   shared Iris/core failure from Vibrant-specific formats.
3. Enhanced next. Only then S2 effect matrix.
4. DH/Voxy are separate and NOT RUN for the first import.

No S0/S1/S2 claim follows from this audit.
