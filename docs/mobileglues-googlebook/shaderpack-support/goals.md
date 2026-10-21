# Shaderpack support goals

Status: **planning; no support claim.**
Updated: 2026-10-20.

## Program goal

Make the listed original shaderpack archives load, render their named effects,
and remain stable on the X1E Googlebook tuple without editing the archives or
requiring a hidden secondary bridge.

Target tuple:

- HP Googlebook 14 `C1L63708PZ`, Snapdragon X1E80100 / Adreno X1-85.
- Android 17 / API 37, system ANGLE over Vulkan 1.4.295, GLES 3.2.
- Minecraft 1.21.1 / NeoForge 21.1.262 / Iris 1.8.14-beta.1 / Sodium 0.8.13.
- FCL 1.3.4.0.

## Support levels

| Level | Requirement | Evidence |
| --- | --- | --- |
| S0 Import | Pack appears in Iris, selected, no launcher crash, truthful compile diagnostics | Device log and F3 |
| S1 Pipeline | No fallback shader; world, sky, terrain, entities, water and HUD render; no GL error storm | Device run, screenshot/log |
| S2 Effects | Named effects match a desktop/vanilla reference; effect matrix complete | Side-by-side captures/log |
| S3 Product | Resize, reload, pack switch, suspend/resume, sustained performance and thermal gates | Matched run + F3/thermal |

A pack is not called "supported" at S0 or S1. "Works" means S2 for the named
effect set with the exact archive hash and tuple.

## Named effects to preserve

- Sildur's: shadows, water/reflections, volumetric light/godrays, bloom,
  TAA/FXAA, SSAO, fog, clouds, hand and underwater behavior.
- iterationRP: voxel/WSR/ACT or IRC features only when the selected original
  profile requests them; no silent effect removal. Default profile first.

## Constraints

- Original ZIP bytes stay unchanged; hashes are tracked, archives are not
  committed.
- No second bridge mod is a support dependency. Core-only behavior is the
  target; a bridge may be a research aid but not a release requirement.
- Compile/link/FBO failures remain errors; no fallback shader, unit-0 fallback,
  or effect suppression may create a false pass.
- Do not switch renderer, provider, or system graphics to obtain a pass.
- Product performance is measured separately from correctness.
- Sildur packs are All-Rights-Reserved: local test fixtures only, no
  redistribution or code copying. iterationRP is also All-Rights-Reserved;
  private modifications do not become public support targets.

## Exit criteria

1. Sildur's Vibrant reaches S3 on the target tuple.
2. Enhanced Default and Basic reach at least S2 without regressing Vibrant.
3. iterationRP original reaches S2 on its default profile after the shared
   format/image/SSBO contract lands; advanced profiles are separate gates.
4. Safe-claim table in `backend-support-fixing-plan.md` is updated only with
   evidence; unsupported features stay explicitly listed.
