# FSR3 menu lifecycle, performance baseline and 26.2 isolation

Date: 2026-10-21. Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 /
Adreno X1-85, ANGLE 2.1 / Vulkan 1.4.295, GLES 3.2.

## Menu lifecycle regression: reproduced and fixed

With FSR3 enabled/Balanced, the 1.21.1 title screen rendered in a 1694x932
game window and did not fill the 2880x1800 desktop. With FSR3 disabled, the same
title filled the desktop. This reproduces the user's first-boot report.

Root causes addressed in commit `7ce1684`:

- The scene viewport conversion was one-way. Once a scaled target left the
  viewport at 1694x932, binding a native target did not restore 2880x1585. A
  thread-local tracker now restores the native extent for native draw targets
  and the default framebuffer, while leaving atlas, sub-rect, shadow and
  explicit scissor cases alone.
- The direct FSR3 presenter did not guard `GL_SCISSOR_TEST`. A reduced scene
  scissor could clip the presentation blit to 1694x932. `PresentGuard` now saves,
  disables and restores the scissor state around presentation.

Device result: with FSR3 enabled/Balanced, the 1.21.1 title menu now fills the
display. Capture `/tmp/title-menu-fixed2.png`, SHA-256 `0f7fe8de…0df4`.
Host suite passes, including the new native-restore cases in
`fsr3_viewport_runtime_test`.

## Performance baseline

| Run | Pack/profile | FSR3 | FPS |
|---|---|---|---:|
| Current | Sildur's Vibrant 2.02 Extreme, Custom | Balanced active | 35 |
| Prior control | Same pack/profile | dispatch bypassed | ~140 |

FSR3 scene extent is 1694x932 (Balanced 1.7 from 2880x1585). FSR3's post-final
dispatch alone was previously measured at 17.7-23.6 ms/frame on this X1E. The
35-to-140 FPS difference (~21 ms) is consistent with that dispatch, not with the
per-bind viewport queries.

No performance patch is claimed. The fastest measured path to >60 FPS is to
keep Balanced scene scaling and bypass the post-final FSR3 dispatch. Spatial-only
FSR3 or a cheaper upscale is the next engineering option; changing Balanced to
Performance/Ultra alone does not remove the fixed output-pass cost.

## 26.2 isolation

Zero-mod Minecraft 26.2 still renders a black world with the current core.
Tested and excluded:

- Iris/shaders: off.
- Sodium: absent in the zero-mod profile.
- FSR3: disabled in the test path.
- DSA buffers: full-wrapper 6/6 pass.
- `GL_ARB_direct_state_access` advertisement: disabled, still black.
- `GL_ARB_vertex_attrib_binding` advertisement: disabled, still black.
- Sampler mipmap min-filter fallback: applied, still black.

Captures: `/tmp/262-dsaoff-world2.png`, `/tmp/262-vaboff-world.png`,
`/tmp/262-sampler-world.png`. Terrain writes near-zero color while the hand is
textured and lit, so the remaining fault is in the 26.2 terrain fragment input
or translated shader, not the buffer/DSA paths already excluded.

## Open work

- 26.2 needs a diagnostic core/API trace of the terrain draw: program, sampler
  bindings, vertex attributes, lightmap/atlas textures, and translated shader.
- Performance needs a deliberate choice between bypass, spatial-only FSR3, or a
  cheaper upscale; preset changes alone are insufficient.
