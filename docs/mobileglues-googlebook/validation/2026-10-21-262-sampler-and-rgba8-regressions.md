# Minecraft 26.2 sampler and RGBA8 storage regressions

Date: 2026-10-21. Device: HP Googlebook 14 `C1L63708PZ`, Snapdragon X1E80100 /
Adreno X1-85, ANGLE 2.1 / Vulkan 1.4.295, GLES 3.2, Minecraft 26.2.

## Result

Three fork-only regressions were confirmed and fixed in source:

1. The translator preserved SPIRV-Cross's implicit `layout(binding = 0)` on
   sampler declarations. In ESSL 3.1+ that fixed both 26.2 terrain samplers to
   texture unit 0, so `Sampler0` (block atlas) and `Sampler2` (lightmap) aliased
   the same unit. Upstream MobileGlues strips `layout(binding)`.
2. `glTexImage2D(level 0, null)` converted ordinary `GL_RGBA8` textures to
   immutable one-level storage because the fork's new image-format table treats
   RGBA8 as a native image format. 26.2 allocates its 1024x512 block atlas this
   way, so the atlas was allocated as immutable storage with no upload.
3. Commit `609ff8e` replaced native `glDrawArraysInstanced`,
   `glDrawArraysIndirect` and `glDrawElementsIndirect` forwards with wrappers in
   `drawing.cpp` but omitted their `drawing.h` declarations. Hidden symbol
   visibility therefore removed them from the shared library. 26.2's atlas
   sprite render pass uses the missing draw entry point, so the block atlas
   stayed empty and terrain sampled black.

All three fixes are committed. A zero-mod 26.2 world now renders terrain, trees,
HUD, hand and sky. FSR3 code was not changed. The bind-time viewport regression
is already fixed by `351328a`; FSR3 remains a separate verified path.

The third regression was found by device bisect between upstream `97558a6`
(good) and fork HEAD. `36ce41a` and `a2b1a70` rendered terrain; `609ff8e` was the
first bad commit. A clean build of upstream `97558a6` also rendered the same
world, confirming the fault was fork-only.

## Evidence chain

### Launcher cache

The first diagnostic runs were invalid because Fold Craft Launcher kept loading
a cached core. The live log marker did not match the installed APK marker. Every
result below was captured only after the live marker matched:

- live: `MG src-...`
- installed: `src-...`

This check must precede any future 26.2 interpretation.

### Sampler alias

A draw/state dump of the 26.2 terrain program showed:

```
Sampler2 unit=0
Sampler0 unit=0
```

The original 26.2 assets declare unqualified samplers:

- `assets/minecraft/shaders/core/terrain.vsh`: `uniform sampler2D Sampler2;`
- `assets/minecraft/shaders/core/terrain.fsh`: `uniform sampler2D Sampler0;`

SPIRV-Cross emitted `layout(binding = 0)` for both. The fork kept that qualifier
for ESSL 3.10+; commit `9a91ede` introduced the retention while preserving SSBO
bindings. Upstream `removeLayoutBinding()` removes it.

The fix strips binding qualifiers from sampler declarations only. UBO, SSBO and
image bindings are unchanged. After the fix the terrain program reports:

```
Sampler2 unit=1
Sampler0 unit=0
```

### Empty block atlas

The terrain atlas is a 1024x512 `GL_RGBA8` texture. An FBO readback at the
terrain draw initially returned:

```
nonzero=0/524288
```

The upload trace showed only:

```
TexImage2D tex=154 1024x512 RGBA8 pixels=0
TexStorage2D tex=154 levels=1 1024x512
```

There was no `TexSubImage2D`, `glCopyImageSubData`, `glCopyTexSubImage2D`, or
blit filling that object.

Commit `788ef3c` added `gl/image_format.h`. `mg_is_native_image_format(GL_RGBA8)`
is true, and the null-data `glTexImage2D` path converted the allocation to
immutable one-level storage. Upstream has no `image_format.h` and no such
null-data conversion.

The fix excludes `GL_RGBA8` and `GL_RGBA8_SNORM` from that conversion. The
atlas then becomes mutable, but the 26.2 copy/upload path still does not fill it.
Readback after the fix:

```
nonzero=6665/524288
```

That sparse content is not a complete block atlas; terrain was still black.

### Missing draw exports

`609ff8e` moved four draw entry points out of `gl_native.cpp` and defined
wrappers in `drawing.cpp`. Three of them, `glDrawArraysInstanced`,
`glDrawArraysIndirect` and `glDrawElementsIndirect`, were not declared in
`drawing.h`. The build uses hidden symbol visibility, so they were absent from
the shared library:

```
$ nm -D --defined-only libmobileglues.so | grep glDrawArrays
00000000005dfacc T glDrawArrays
# glDrawArraysInstanced missing
```

The zero-mod 26.2 atlas path is a render pass (`TextureAtlas.uploadInitialContents`
drawing into `mipViews`), not an upload or copy. Its sprite draw used the missing
entry point, so the atlas was allocated and logged as created but never filled.
This also explains why disabling the depth guard, the depth-filter push, the
texture-state shadow, or `prepareForDraw` did not fix the world: they were not
the failure.

Adding the three declarations restored the exported symbols:

```
T glDrawArrays
T glDrawArraysIndirect
T glDrawArraysInstanced
T glDrawElementsIndirect
```

A zero-mod 26.2 world then rendered terrain, trees, HUD, hand and sky on current
HEAD. Capture `/tmp/262-current-fixed2.png`, SHA-256
`8cc8dc7c...07d40f105`, device `C1L63708PZ`.

The upstream control used a clean arm64 build of `upstream/main` (`97558a6`) on
the same world and rendered terrain. Device bisect then found:
`36ce41a` good, `a2b1a70` good, `609ff8e` first bad. The depth-filter logic
inside `609ff8e` was separately disabled (guard, push, sampling shadow,
`prepareForDraw`) without restoring terrain; the missing draw exports were the
remaining fork-only change.

### 26.2 texture assembly

The 26.2 client jar was extracted from the device and inspected with `javap`.
The relevant path is:

- `com.mojang.blaze3d.opengl.GlCommandEncoder.copyTextureToTexture`
  -> `DirectStateAccess.blitFrameBuffers`
  -> `ARBDirectStateAccess.glBlitNamedFramebuffer`
- `DirectStateAccess$Core.bindFrameBufferTextures`
  -> `ARBDirectStateAccess.glNamedFramebufferTexture`
- `GlCommandEncoder.copyBufferToTexture`
  -> `GlStateManager._texSubImage2D` with a pixel-unpack buffer

The fork's FBO trace recorded only the default/FSR blit
(`draw=0 read=2`) and no `tex=154` attachment through the traced
`glFramebufferTexture2D` path. The DSA named attachment/blit path therefore
remains the open boundary. 26.2 logs `Created: 1024x512x0
minecraft:textures/atlas/blocks.png-atlas` with no exception or GL error, so the
game believes the atlas exists.

## Upstream comparison

| Area | Upstream `97558a6` | Fork before this fix |
|---|---|---|
| Sampler bindings | `removeLayoutBinding()` strips layout binding | Retained for ESSL 3.10+ (`9a91ede`) |
| `image_format.h` | Absent | Added by `788ef3c` |
| Null RGBA8 `glTexImage2D` | Normal mutable allocation | Converted to immutable one-level storage |
| Draw entry points | Native forwards, exported | Wrappers added by `609ff8e`; three hidden until `1bd40dc` |
| FSR3 viewport | N/A | `c3cf54c` regression, fixed by `351328a` |

The fork also removed upstream's `buffer_coherent_as_flush` setting. That is
recorded as a separate candidate for the buffer-path review; it is not required
for the two fixes above.

## Gates

- Host semantic suite: PASS after all changes.
- 26.2 sampler units: fixed (distinct).
- 26.2 atlas allocation: mutable; atlas render pass now reaches the target.
- 26.2 zero-mod world: **PASS** — terrain, trees, HUD, hand and sky render on the
  target device, current HEAD `1bd40dc`.
- 26.2 NeoForge/Iris/Sodium lane: **PENDING**.
- 1.21.1 / shaderpack regression lane: **PENDING** after the image/translator
  changes.

## Next

Run the 26.2 NeoForge/Iris/Sodium lane and the ordinary 1.21.1 shaderpack lane.
Verify the RGBA8 image-load/store carve-out against an Iris pack that uses an
RGBA8 custom image; if it regresses, defer the immutable conversion to image
bind time rather than restoring it for every RGBA8 allocation. Do not claim a
full 26.x release qualification from the zero-mod result alone.
