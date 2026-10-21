# Minecraft 26.2 core render failure after viewport fix

> Superseded for root cause and current status by
> [26.2 sampler, RGBA8 and draw-export regressions](2026-10-21-262-sampler-and-rgba8-regressions.md).
> This note retains the pre-fix failure evidence; a zero-mod 26.2 world now renders.

Date: 2026-10-21. Device: HP Googlebook 14 `C1L63708PZ`, Snapdragon X1E80100 /
Adreno X1-85, ANGLE 2.1 / Vulkan 1.4.295, GLES 3.2.

## Current artifact

- Core: local test APK built from `351328a`, unpacked marker
  `src-9004e39bf61a947d`, stripped core SHA-256 `55b5a966…9602`.
- Test APK SHA-256 `76a84322…c4b` (debug-signed, same certificate as installed).
- Installed as an in-place update; launcher data was not cleared.

## 26.2 with NeoForge/Iris/Sodium

F3 from the 26.2 profile:

| Field | Value |
|---|---|
| Game | Minecraft 26.2, `26.2-NeoForge/neoforge-26.2.0.93` |
| Iris | 1.11.4+mc26.2, **Shaderpack: (off)** |
| Sodium | 0.9.2+mc26.2 |
| Display | 2880x1585 |
| Core | `MG src-9004e39bf61a947d` |
| OpenGL | 4.0.0 MobileGlues 2.0.0 |
| Renderer | Adreno X1-85 / ANGLE / Vulkan 1.4.295 |
| Result | Sky, clouds, HUD, hand render; terrain is black/missing |

Capture: `/tmp/262-world-f3.png`, SHA-256 `d76a0ed7…05496`.

This run has **no shaderpack and no FSR3 bridge final pass**. The bridge sees
`packHasFinalPass` false, so this is not an FSR3 path failure.

## Zero-mod 26.2 control

The `26.2` profile with **Mods:0** was launched and a new world generated.
Result: black world; only HUD/hand and a small sky-colored area render.

Captures:

- Title: `/tmp/262-vanilla-title.png`, SHA-256 `d19d37a9…feed`
- World: `/tmp/262-vanilla-world2.png`, SHA-256 `78439751…dd9f`

Log: `.minecraft/versions/26.2/logs/latest.log` (vanilla profile). It records:

- `Using graphics backend OpenGL, using drivers: 4.0.0 MobileGlues 2.0.0`
- `Using graphics device: ... MG src-9004e39bf61a947d`
- Extensions: `GL_ARB_buffer_storage`, `GL_ARB_vertex_attrib_binding`,
  `GL_ARB_direct_state_access`
- No shader compile/link error, no GL error, no exception.

The `26.2-NeoForge` log is at
`.minecraft/versions/26.2-NeoForge/logs/latest.log`; it also contains no shader
or GL error.

## Conclusion

The 26.2 black world is a **MobileGlues core rendering failure**. It reproduces
with:

- no Sodium,
- no Iris shaders,
- no FSR3 bridge/final pass,
- and the viewport fix installed.

The DSA persistent-buffer path passes 6/6 (`build/qualification/2026-10-21-dsa-probe4`),
so the failure is not the tested buffer upload path. Remaining candidates are
the 26.2 Blaze3D render-pass/DSA texture, vertex-array, framebuffer, or UBO path.
The log's advertised OpenGL 4.0.0 + DSA extension set is the relevant boundary.

The 1.21.1 NeoForge/Complementary/FSR3 regression is fixed and device-verified in
`2026-10-21-viewport-regression-fix-and-dsa-probe.md`. It is a separate issue.

No 26.2 fix is claimed. Next work needs a debug core/API trace around the 26.2
render pass; the current production logs contain no error to attribute.
