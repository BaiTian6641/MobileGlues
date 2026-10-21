# Image lowering validation and current-pack device regression

Date: 2026-10-21. Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 /
Adreno X1-85, ANGLE 2.1 / Vulkan 1.4.295, GLES 3.2.

## Host validation

`sh MobileGlues-cpp/tests/run.sh` passes.

New `compat_image_test` cases pin the explicit-format lowering:

| Input qualifier | Output |
|---|---|
| `r16ui` | `r32ui` |
| `r8ui` | `r32ui` |
| `rg32ui` | `rgba32ui` |

Existing checks for `rgba16`, `rg16`, `rg16f`, `rg32f`, `rgba16f`,
`rgba16ui`, multiline declarations, preserved `binding`, and untouched
non-image code also pass.

Commits: `98058e3` (lowering), `0d3b890` (host tests).

## Device regression

Stable bridge installed: `4d49e135…0684`. Temporary source-boundary probes were
removed before this run.

| Field | Value |
|---|---|
| Pack | Complementary Reimagined r5.9.3 |
| Condition | Night, same world |
| Result | World, sky, clouds, terrain, hand, and HUD render |
| FPS | 44 FPS at 2880×1585 (F3) |
| GPU | Adreno X1-85 / ANGLE / Vulkan 1.4.295, MobileGlues 2.0.0 string |
| Shader log | No compile/link failure for this pack |
| Capture | `/tmp/current-device.png`, SHA-256 `baa02e6c…fae25` |

The current pack does not exercise R16UI/RG32UI images, 1D images, or image
atomics. This run is a **regression pass**, not device qualification of the new
image-format lowering.

## Still pending

- A device wrapper probe that compiles and binds explicit `r16ui`, `r8ui`, and
  `rg32ui` images, writes/reads values, and checks the reported internal
  formats. Host tests alone do not prove the ESSL/driver path.
- 1D image virtualization and its device probe.
- Image atomics and subgroup semantics probes.

No support level is promoted by this record.
