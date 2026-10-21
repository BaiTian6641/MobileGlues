# X1E Sildur render-target format probe

Date: 2026-10-20
Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85.
Provider: system ANGLE over Vulkan 1.4.295, GLES 3.2.
Probe: `capability_probe_arm64`, SHA-256
`a59c71ff3d300390397f13a9afa2a23b7eafabf9706a83e35699be29b3409a77`.
Raw output: `2026-10-20-x1e-sildur-format-probe.json`.

The probe now includes plain RGB8 and RGB10_A2 in addition to the existing
format table. It tests texture allocation, FBO completeness, clear and image
binding below MobileGlues; this is provider evidence, not a pack pass.

| Format | Texture 2D | Texture 3D | FBO complete | Clear | Image bind |
| --- | --- | --- | --- | --- | --- |
| RGB8 | yes | yes | **yes** | yes | no |
| RGB10_A2 | yes | yes | **yes** | yes | no |
| RGB8_SNORM | yes | yes | no | no | no |
| RGB16F | yes | yes | yes | yes | no |
| R11F_G11F_B10F | yes | yes | yes | yes | no |
| RGBA8 | yes | yes | yes | yes | yes |

## Result

`SILD-02A` PASS. Plain RGB8 is color-renderable on this provider, so Vibrant's
`RGB8` colortex requests are not a first-order import blocker. RGB10_A2 is also
renderable for Enhanced Default.

This removes the format-creation branch from the Sildur first-failure path.
The remaining Sildur risks are host shader translation/link and the device
S0/S1 route, not RGB8/RGB10_A2 FBO completeness.

No core format-table change is justified by this probe.
