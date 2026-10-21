# Viewport-regression fix and DSA staging probe

Date: 2026-10-21. Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 /
Adreno X1-85, ANGLE 2.1 / Vulkan 1.4.295, GLES 3.2.

## Fix

Commit `351328a` (`fix(gl): restore bind-time scene viewport conversion`)
restores the conversion removed by `c3cf54c`. When experimental FSR3 is active,
a nonzero draw framebuffer is bound, and final scope is inactive, the current
full-surface viewport is scaled through `FSR3AdjustViewport`. The FBO-0 native
reset was deliberately not restored.

The host regression now binds a 1694x932 color target with a current 2880x1585
viewport and requires conversion to 1694x932. Native, atlas, partial-menu,
read-target, final-scope, and FSR1-redirect cases remain unchanged. Host suite
passes (`sh MobileGlues-cpp/tests/run.sh`, exit 0).

## Device validation, 1.21.1 NeoForge

| Field | Value |
|---|---|
| Core | `2.0.8-dev` built from `351328a`; unpacked core hash `ea7a36f0…e459a` |
| Installed stripped core | `55b5a966…9602` |
| Marker | `src-9004e39bf61a947d` |
| Bridge | 0.2.4, `mgfsr3bridge-0.2.4-1.21.1-neoforge.jar` |
| Pack | Complementary r5.9.3, LOW (+2) |
| FSR3 | Active; bridge log reports `scene depth 1694x932` |
| Result | Full terrain, clouds, hand, HUD; no white-plane crop |
| Frame rate | 35 FPS in the captured frame |

Captures:

- Clean world: `/tmp/new-core-world-clean.png`, SHA-256 `cac1fee2…e0c0`
- F3 tuple: `/tmp/new-core-world-f3.png`, SHA-256 `2a282c00…21db5`
- Initial loaded frame: `/tmp/new-core-world1.png`, SHA-256 `a199b33c…7ab8`

The test APK is local-only: `/tmp/mg-plugin-core-test/app/build/outputs/apk/debug/app-debug.apk`,
SHA-256 `76a84322…c4b`. It uses the same debug certificate as the installed
plugin, so it was installed as an update without clearing launcher data.

## DSA staging probe

The full-wrapper buffer probe now includes `dsa_persistent_explicit_0x72`:
`glNamedBufferStorage` + `glMapNamedBufferRange(0x72)` + explicit flush + a
copy while the source remains mapped + GPU fence + read-mapped destination.
This matches the Minecraft 26.2 / Sodium persistent staging sequence.

Against installed 2.0.8, all six cases pass:

- `persistent_explicit_0x72`
- `nonpersistent_explicit`
- `persistent_coherent`
- `invalid_flags`
- `invalid_flush_range`
- `dsa_persistent_explicit_0x72`

Result: `build/qualification/2026-10-21-dsa-probe4/result.json`, SHA-256
`a5809bbf…239b`, pass=true. Probe commit `93f39f3`.

This removes the DSA persistent-upload hypothesis from the FSR3-off 26.2
failure. It does not qualify 26.2 game rendering.

## 26.2 status

The available 26.2 traces are **2.0.4/0.2.1** and **2.0.5/0.2.2**. The
user reports current 2.0.8 + 0.2.4 fails with FSR3 both on and off, but no
current F3/log was captured. FSR3-off disables the bridge scaling path, so the
FSR3-off failure remains a core-path defect; it cannot be attributed to
`c3cf54c` or the DSA mapping path.

FCL's current UI did not expose the 26.2 instance selector during this run.
A 26.2 retest must launch the user's 26.2 profile with the installed
`src-9004e39bf61a947d` core and capture F3 plus `latest.log` before attribution.
