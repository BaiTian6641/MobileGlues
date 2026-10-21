# Spatial-only FSR3 performance path

Date: 2026-10-21. Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 /
Adreno X1-85, ANGLE 2.1 / Vulkan 1.4.295.

## Gate result

| Path | Sildur Extreme FPS | GPU |
|---|---:|---:|
| FSR3 temporal (8 passes) | 39 | 98% |
| Debug bypass (no upscale) | 81 | 94% |
| Spatial accumulate+RCAS | 52 | — |
| **Spatial bilinear+RCAS** | **78** | **94%** |

The shipped spatial path clears the >60 FPS gate without using the debug bypass.
It keeps FSR3 Balanced scene scaling at 1694x932 and produces a native
2880x1585 output.

## Implementation

Commit `48fe5bf` adds `fsr3SpatialOnly` to the native settings and a spatial
branch in `fsr3_backend.cpp`:

- Skip temporal passes 0-5 and their clears.
- Blit the 1694x932 scene color into an output-sized RGBA8 texture with linear
  filtering.
- Run the existing RCAS pass, with `r_rcas_input` bound to the upscaled texture.
- No temporal history, motion or accumulation resources are read.

Plugin 2.0.9-dev adds **FSR3 spatial only (skip temporal passes)** and writes
`fsr3SpatialOnly` as a JSON integer. The earlier boolean encoding was ignored by
native `config_get_int`.

Artifact: plugin debug APK SHA-256 `8c979559…c352`, version 2009.
The setting was enabled through the UI; the user confirmed the same Sildur scene
exceeds 60 FPS.

## Quality boundary

This is bilinear+RCAS, not EASU+RCAS. It is a spatial reconstruction fallback,
not an equivalent replacement for temporal FSR3. The image is stable (no
temporal history), but edge quality is below a full EASU path. A future EASU
pass can replace the bilinear blit without changing the config/API.

## 26.2 status

The zero-mod 26.2 black-terrain failure remains. The complete mipmap/sampler
clamp (texture and all sampler setters) did not change it. Already excluded:

- DSA buffers and DSA extension advertisement.
- `GL_ARB_vertex_attrib_binding`.
- Sampler mipmap min-filter fallback.
- FSR3/Iris/Sodium (zero-mod profile still fails).

The next step is a diagnostic dump of the first terrain draw: translated shader,
program, sampler units/textures, attribute descriptors, UBO bindings, and atlas
upload state. No 26.2 fix is claimed.
