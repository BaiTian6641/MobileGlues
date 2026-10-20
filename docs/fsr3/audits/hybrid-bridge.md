# Hybrid Minecraft/Iris bridge (experimental)

Status: **connector executes on X1E; native scene resolution only; not a
performance pass.** The bridge is optional and the MobileGlues config switch is
the authority. This is not a claim of low-resolution scene rendering.

## Architecture

The no-mod presentation path remains in MobileGlues (`gl/FSR3Direct.cpp`). The
optional bridge is a small Java/Mixin connector under
`integrations/minecraft/fsr3-bridge/`:

- `FinalPassRendererMixin` runs at the return of Iris'
  `FinalPassRenderer.renderFinalPass`, when the main framebuffer is current and
  before vanilla composes the HUD.
- `Fsr3Bridge` calls a tiny `libmgfsrhelper.so`, built inside the mod and
  extracted at runtime. The helper finds the MobileGlues core **already mapped
  by the renderer plugin** (`/proc/self/maps` plus `dlopen(..., RTLD_NOLOAD)`)
  and invokes its JNI entry points. It does not load a second core with separate
  EGL state.
- The native entry points resolve the current draw framebuffer's color and depth
  texture attachments, create a synthetic zero-motion texture, dispatch the
  in-tree FSR3 backend, and blit the backend output back to the current main
  framebuffer. HUD is composed after this point.
- The bridge contains no upscaler and no renderer rewrite. MobileGlues owns the
  FSR3 shaders, resources, history and output.

## Config and activation

- Added `fsr3Setting` to MobileGlues core config (`0` disabled, `1`
  experimental), with a matching switch in the FCL plugin settings UI.
- Both native presentation and the bridge check this setting. Installing the
  bridge alone does not activate FSR3.
- The switch warns that the scene is still rendered at native resolution and
  that the mode may reduce frame rate.

## Target observation

Test tuple: HP Googlebook 14 `C1L63708PZ`, X1E-80-100 / Adreno X1-85, Android
17, ANGLE/Vulkan, FCL 1.3.4.0, Minecraft 1.21.1 NeoForge 21.1.262, Iris
1.8.14-beta.1, Sodium 0.8.13, AuroraLumen-Mobile profile.

- Mixin applied (`FinalPassRendererMixin loaded`).
- Core mapped and helper initialized; `nativeAvailable=true`.
- Every final pass returned `submit rc=0`; FSR3 dispatched and the output was
  blitted.
- Visual result was not corrupted in the tested scene.
- **F3 measured 13 FPS**, versus 26 FPS with the bridge disabled. At native scene
  resolution, the eight FSR3 passes and format substitutions cost more than they
  save. This is a failed performance gate and is why the switch is off by
  default.

Evidence (outside Git for the captures):

| File | SHA-256 |
| --- | --- |
| `captures/2026-10-20-fsr3-hybrid-f3.png` | `7668f7e380e7e1b525e06997e3fbcc9e0480061ccf4bc7a0b26a33996be15d8b` |
| `captures/2026-10-20-fsr3-hybrid-bridge.log` | `81f05f1e00b37bc7de86c78f23f2bb67f06d2373873e13c1c9e5ae31c6db8a8c` |

## 16 KB packaging

- `readelf -lW` on the ARM64 core: every `LOAD` segment has alignment `0x4000`.
- The bridge helper was built with the same NDK and also reports `0x4000`.
- The final test APK is arm64-only, stores native libraries uncompressed, and
  passes `zipalign -c -P 16 -v 4`.
- The FCL plugin's info-getter was rebuilt with NDK r28c and
  `-Wl,-z,max-page-size=16384`; the stock 32/x86 libraries were omitted from the
  test APK.
- Final APK SHA-256: `c66bc117b129714a8f08798174471976824de0d5e09a01414b1286f647dbb6ae`.

## Missing low-resolution scene work

This bridge submits the **finished display-resolution** color/depth. It does not
change Minecraft/Iris render-target allocation, viewports, depth extent or
screen-size uniforms. Therefore it does **not** reduce scene work and is not the
requested low-resolution FSR3 path.

A true low-resolution Minecraft path still needs a scene-scale hook in the
adapter/core boundary: scaled Iris targets and matching depth/viewport/screen
uniforms, plus a documented profile. That remains the next implementation task.
