# Validation Artifacts

Store machine-readable results and reproduction metadata here. Binaries, shader archives, and captured
images belong outside Git; record their path and checksum instead.

| Artifact | Date | Level | Command | Result |
| --- | --- | --- | --- | --- |
| `2026-10-18-x1e-provider-probe.json` | 2026-10-18 | target device, below wrapper | see below | provider limits/format/behavioral probe on `C1L63708PZ` |
| `2026-10-18-x1e-device-identity.txt` | 2026-10-18 | target device | `adb -s C1L63708PZ shell 'getprop …'` | HP Googlebook 14, X1E80100, Android 17/API 37, 4096-byte pages |
| `2026-10-19-x1e-wrapper-stage1-stage2.json` | 2026-10-19 | target device, wrapper level | see below | shader-source, status, limit, frag-data and image-binding contract results |
| `2026-10-19-x1e-wrapper-stage2.json` | 2026-10-19 | target device, wrapper level | same harness | full suite including SSBO binding, 512 compute, and vertex image store |
| `2026-10-19-x1e-wrapper-final.json` | 2026-10-19 | target device, wrapper level | same harness | 23/23 contract cases, including RGB8_SNORM and desktop GLSL versions |
| `2026-10-19-x1e-wrapper-image-bridge.json` | 2026-10-19 | target device, wrapper level | same harness | 25/25 cases including extension consistency and formatless-image metadata bridge |
| `2026-10-19-x1e-wrapper-explicit-image.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 explicit_image` against ARM64 build `86a73e18…9093` | PASS: explicit `layout(binding=5)` preserved; write/read value `0x12345678` |
| `2026-10-19-x1e-wrapper-all-final.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `cc2b5e2f…8252` | 31/31 PASS, including dynamic image units `act_pingpong` values `11,22` and explicit bindings |
| `2026-10-19-x1e-wrapper-duplicate-image.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 duplicate_image` | PASS: two formatless images at duplicate `binding=0` separated to units 0/1; values `11,22` |
| `2026-10-19-x1e-wrapper-all-dup.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `5f8907a1…66ec` | 32/32 PASS after duplicate-binding handling |
| `2026-10-19-x1e-wrapper-all-global.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `a3f5d9d9…0f0c` | 32/32 PASS with whole-source image declaration parsing |
| `2026-10-19-x1e-wrapper-depth-filter.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 depth_filter` | PASS: D32F MIPMAP_LINEAR/LINEAR app state reads back 9986/9729, sampled depth `0.5` (not zero) |
| `2026-10-19-x1e-wrapper-timer-query.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 timer_query` | PASS: 64-bit timestamp query returns increasing values |
| `2026-10-19-x1e-wrapper-copy-image-depth.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 copy_image_depth` | PASS: same-format D32F `glCopyImageSubData`, sampled `0.5` |
| `2026-10-19-x1e-wrapper-all-final2.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `59a9b230…f699` | 37/37 PASS including depth filtering, timer queries and depth image copy |
| `2026-10-19-x1e-wrapper-all-invalidate.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `b554ce81…4b87` | 37/37 PASS after logical attachment invalidation mapping; host shuffle scenario 5b passes |
| `2026-10-19-x1e-wrapper-blit-resolve.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 blit_resolve` | PASS: 4x MSAA renderbuffer resolved to RGB `0.251,0.502,0.749` |
| `2026-10-19-x1e-wrapper-all-resolve.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `b554ce81…4b87` | 38/38 PASS including multisample resolve |
| `2026-10-19-x1e-wrapper-all-cachekey.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `5a357904…158f` | 38/38 PASS after cache identity fix; host `cache_key_test` all identities distinct |
| `2026-10-19-x1e-wrapper-depth-copy-mismatch.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 depth_copy_mismatch` against ARM64 build `7f0a052d…2284` | PASS: D24S8 source / D24 destination skipped, `copy_error=0`, fence valid |
| `2026-10-19-x1e-wrapper-blit-depth-mismatch.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 blit_depth_mismatch` | PASS: `GL_INVALID_OPERATION` (1282), fence valid, no device loss |
| `2026-10-19-x1e-wrapper-geometry-stage.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 geometry_stage` | PASS: desktop `#version 400 core` geometry program compiled/linked/drew `pixel=255,0,0` |
| `2026-10-19-x1e-wrapper-tessellation-stage.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 tessellation_stage` | PASS: VS/TCS/TES/FS compiled/linked/drew `pixel=255,0,0`; `GL_ARB_tessellation_shader` advertised; default-level calls report `GL_INVALID_OPERATION` |
| `2026-10-19-x1e-wrapper-all-stages.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `7f0a052d…2284` | 43/43 PASS including geometry, tessellation, depth-copy and blit-mismatch guards |
| `2026-10-19-x1e-wrapper-blit-depth-match.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 blit_depth_match` against ARM64 build `7286bd47…5aff` | PASS: matching D24S8→D24S8 depth+stencil blit, no error, fence valid |
| `2026-10-19-x1e-wrapper-copy-image-depth-mismatch.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 copy_image_depth_mismatch` | PASS: D24S8→D24 copy skipped, no device loss, fence valid |
| `2026-10-19-x1e-wrapper-clear-tex-level.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 clear_tex_level` | PASS: level 1 cleared, level 0 remains `7` |
| `2026-10-19-x1e-wrapper-depth-sampler-compute.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 depth_filter` | PASS: compute samples depth `0.5` with a LINEAR sampler object bound; sampler restored |
| `2026-10-19-x1e-wrapper-all-audit.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `7286bd47…5aff` | 46/46 PASS including legal/mismatched depth blits, copy-image guard, clear level and compute sampler guard |
| `2026-10-19-x1e-wrapper-translated-image-decl.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 translated_image_decl` against ARM64 build `7c4c2a57…6650` | PASS: `uniform writeonly highp image2D` parsed and bound to unit 1 without aliasing explicit unit 0 |
| `2026-10-19-x1e-wrapper-metadata-image-format.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 metadata_image_format` | PASS: `GL_RGBA8` metadata rewrites bare `playerAtlas_img` to `layout(binding = 0, rgba8)` |
| `2026-10-19-x1e-wrapper-all-playerimage.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `7c4c2a57…6650` | 48/48 PASS including both player-image declaration probes |
| `2026-10-19-x1e-wrapper-vertex-ssbo-atomics.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 vertex_ssbo_atomics` | PASS: six binding-3 vertex atomics, `min=-10` / `max=10`, fence valid |
| `2026-10-19-x1e-wrapper-vertex-image2d.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 vertex_image2d` | PASS: 2D RGBA8 vertex image store, readback `64,128,191`, fence valid |
| `2026-10-19-x1e-wrapper-all-playerops.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `7c4c2a57…6650` | 50/50 PASS including separate player-reflection operation probes |
| `2026-10-19-x1e-wrapper-translation-failure.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 translation_failure` | PASS: rejected source replaces backend text; compile status false |
| `2026-10-19-x1e-wrapper-uniform-initializer.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 uniform_initializer` | PASS: array suffix preserved, initializer removed |
| `2026-10-19-x1e-wrapper-frag-data-shared.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 frag_data_shared` | PASS: second program links restored source without program A's location 5 |
| `2026-10-19-x1e-wrapper-all-translation53.json` | 2026-10-19 | target device, wrapper level | `./wrapper_probe_arm64 all` against ARM64 build `aeb6861d…5aee` | 53/53 PASS including translation-failure, shared-shader and uniform-initializer probes |

## Game-process captures and logs

Large captures and game logs are kept in `/home/polar/MobileGlues-fixtures/captures/`
and are not committed. Reproduction is through FCL's Upload Log and the shader
pack screen; the saved copies are evidence snapshots.

| File | SHA-256 | Meaning |
| --- | --- | --- |
| `2026-10-19-game-prefix-rgb8snorm-fallback.log` | `9e31f16978b6088b2187101fd98dd644dc0f414567fb691e26c245235a8c183b` | Iris disabled shaders at `createColorFramebuffer` status 36054 |
| `2026-10-19-game-postfix-rgb8snorm-active.log` | `8750088973994aa93536bef14d7a33407d70473574c7c4174a00118acf74f0c0` | pipeline initializes; shaderpack active at High |
| `2026-10-19-complementary-r5.9.3-high-active.png` | `44e9ab2f5b7a822f2b49cf974a4fd5a603772914d7c9da28790059b2089a98b2` | active pack, water/fog/atmosphere |
| `2026-10-19-complementary-r5.9.3-high-f3.png` | `6e479abea7a07b507140b0ebde51909289b83c00a676810dd3b42d54d9a249e4` | F3: pack name without fallback, High, 22 FPS, provider |
| `2026-10-19-r592-high-f3.png` | `dcd335bdca10a08f3dc3ea48d60acd57f39c91c473938567c923f5f85ca55fb4` | r5.9.2 High, no fallback, 21 FPS, provider |
| `2026-10-19-r592-high-shaders-off.png` | `a18a82f373ae959275ea1f5c7dbc57ddfb49c3ed66df87a741bc35f65a9661b7` | paired shaders-off reference, 569 FPS |
| `2026-10-19-r592-high-motion.mp4` | `23fcabab75b7714cf3fff62eeff8fb275a69748af50cf3db261ede2650e8c223` | 12 s motion sample at 21 FPS |
| `2026-10-19-r592-high-motion-frame.png` | `9b5bcdcf5c59497d08e3240cc5e7cb7750e3e586ec8aa54eb66688e3147f4feb` | representative motion frame |
| `2026-10-19-game-r592-high-active.log` | `4c4051b7d4260a01ca2ffe0756d9e39d7531a8b907520fa308a7f5e138e6ba1f` | r5.9.2 High game log (`mclo.gs/z3qIi68`) |
| `2026-10-19-unbound-r592-high-f3.png` | `0c5c17b9dd30fbe7e33b338936417224b6aca2518174a7958435361b1f3c6980` | Unbound r5.9.2 High, no fallback, 20 FPS |
| `2026-10-19-unbound-r592-high-shaders-off.png` | `0a3e47ffc6805d707f47e012a67cf5eff254f93766ec6392f8b30f9bd4ae2891` | Unbound paired reference, 574 FPS |
| `2026-10-19-game-unbound-r592-high-active.log` | `a25ee3661516d86b5c3d1ea7cb8482ab3c03f392eb7ba12cf4c18233f7fab34c` | Unbound High game log (`mclo.gs/hH2OIs5`) |
| `2026-10-19-wrapper-act-volume.json` | `0c69688498ffe5045b4d262de35e9529fee1deb8e90645ce7f68b734d368796c` | full ACT volume + sampler pass |
| `2026-10-19-wrapper-act-pingpong-dynamic.json` | `fbdfcac4920ca6cefae31048befdb4bae5aada9d4def27a50a490f6e36640219` | dynamic image units alias (22,22) |
| `2026-10-19-wrapper-act-pingpong-binding.json` | `213145c23133ff2f460e6a12f4bfbfda71c725d98d6d8efc581fac7a4f93aeb4` | explicit bindings separate (11,22) |
| `2026-10-19-r592-act192-bridge-f3.png` | `0c2c95d3596fe67aa6e869034eaaeacfbe4454a3464aad15f520b29a99cda648` | ACT 192 stable scene with bridge |
| `2026-10-19-r592-act192-bridge-motion.mp4` | `291d3e217f10a14e42b768230d60c9f9508e0ed57d87444815a5f5bdc8b68465` | 8 s ACT stability sample |
| `2026-10-19-game-r592-act192-bridge.log` | `3c924b33f27c44618d1af8f078f930540ae69c96cb4fd776601210e54656b4fa` | bridge log (`mclo.gs/Dz8wXmm`) |
| `2026-10-19-r592-wsr128-water.png` | `4becdcd1cf4f459797391264d062cc8cd0f7a137570018c16a5d562acb857a44` | WSR 128 water scene |
| `2026-10-19-r592-wsr128-water.mp4` | `c6367b4800683fca74e5a0ac2a62e151c9521fd4ca9c2f805787c8dac8ae8aab` | WSR 128 motion sample |
| `2026-10-19-r592-wsr128-room.png` | `73386a36d9c1c29bfc592188a72649ee6258f8aa1b2eb9b77d9f649ed43b7389` | WSR 128 room scene |
| `2026-10-19-game-r592-wsr128-final.log` | `0001298247d25cadd53218dc5a4f82a728feecc0734a970d45a46ec054d52e7c` | WSR 128 final log (`mclo.gs/adG0NiA`) |
| `2026-10-19-r592-wsr192-water.png` | `f1bdcc210c17f2031fd61b4a1113596b14e10ca4dee8fe3b1d53f703fbd409b7` | WSR 192 water/terrain reflection |
| `2026-10-19-r592-wsr192-water.mp4` | `3552b28ea06ad71facd180ffbc9ef56981afec80d3418291fdde669bc7eff948` | WSR 192 motion sample |
| `2026-10-19-r592-wsr192-room.png` | `cc74efb895105222c15ffb31bd509b326e361c8a0f2cb7104a629469c5c6c745` | WSR 192/ACT 192 room frame |
| `2026-10-19-game-r592-wsr192.log` | `315446f192f24c6f881437170cfe26a6c05510c22340c837099ccf5b8d3c44cd` | WSR 192 log (`mclo.gs/FMnnMkC`) |
| `2026-10-18-game-main-menu-old-plugin.png` | `0258b18ccaecd64433880e1463c0a1d329feda7fa17f45201f51d3224571c85e` | installed 2.0.0 renderer smoke before test plugin |
| `2026-10-19-1.21.1-neoforge-21.1.262-world.png` | `c34d52587361c715b37394d21918f6a08a92c93a03d4c903d8e567e842cc766d` | staged 21.1.262 world rendered with the original pack and workspace renderer |
| `2026-10-19-1.21.1-neoforge-21.1.262-title.png` | `3c45814f66152721724c054f4bf5b7f2cc265cad24062488194fba191267ddff` | title screen: Minecraft 1.21.1 / NeoForge 21.1.262 (9 mods), pack active |

## Wrapper harness reproduction

```sh
ANDROID_NDK=$HOME/Android/Sdk/ndk/28.2.13676358 \
  sh MobileGlues-cpp/tools/wrapper_probe/build-arm64.sh
adb -s C1L63708PZ push build/android-arm64-release/libmobileglues.so /data/local/tmp/
adb -s C1L63708PZ push MobileGlues-cpp/tools/wrapper_probe/wrapper_probe_arm64 /data/local/tmp/
adb -s C1L63708PZ shell \
  'mkdir -p /data/local/tmp/mgconfig && \
   echo "{\"enableNoError\":0,\"enableANGLE\":0,\"customGLVersion\":40}" \
     > /data/local/tmp/mgconfig/config.json && \
   cd /data/local/tmp && \
   MG_DIR_PATH=/data/local/tmp/mgconfig MG_PROBE_OUTPUT=/data/local/tmp/wrapper-probe.json \
   MG_LIBRARY=/data/local/tmp/libmobileglues.so LD_LIBRARY_PATH=/data/local/tmp \
   ./wrapper_probe all'
adb -s C1L63708PZ pull /data/local/tmp/wrapper-probe.json
```

The harness loads the built renderer and creates a context through its own EGL
layer. It is a wrapper-level result: it does not prove a shaderpack's rendering,
and it intentionally asserts the current r16ui refusal until that lowering is
implemented. Stored result hashes:
`385e8dab0120ad164b051e7608ea553d5dad9060a41df30a037deae467928964` (stage 1/2)
`b98d262594654c67f2942465d20c255432efdb7935c03cacd73c20f566d0b726` (23-case suite),
and `51b9ef67e5108114b5c7d693b1c94b281aa6d1d24499a839d107d8c5d5fb0ab5` (25-case image-bridge suite).

## Provider probe reproduction

```sh
ANDROID_NDK=$HOME/Android/Sdk/ndk/28.2.13676358 \
  sh MobileGlues-cpp/tools/capability_probe/build-arm64.sh
adb -s C1L63708PZ push \
  MobileGlues-cpp/tools/capability_probe/capability_probe_arm64 /data/local/tmp/
adb -s C1L63708PZ shell \
  'chmod 755 /data/local/tmp/capability_probe_arm64 && \
   /data/local/tmp/capability_probe_arm64 /data/local/tmp/probe.json'
adb -s C1L63708PZ pull /data/local/tmp/probe.json \
  docs/mobileglues-googlebook/validation/2026-10-18-x1e-provider-probe.json
```

The probe deliberately links only EGL/GLESv2 and runs with no MobileGlues library in the process, so
its results describe the provider below the wrapper. It requests ES 3.2, a pbuffer, and one context.
It does not prove the wrapper's advertised capabilities or any shaderpack's rendering.

Hash of the stored probe result:
`d7a3ba31520ff393dd2ec43cf663a4e479ec38955b65836bd3887a9c8618185d`.

## Fixture acquisition

Official archives were downloaded from the Modrinth URLs in the plan on 2026-10-18 into
`/home/polar/MobileGlues-fixtures/`. SHA-256 values match the plan:

| Fixture | SHA-256 |
| --- | --- |
| `ComplementaryReimagined_r5.9.2.zip` | `7ae5f1d6a4237a5c8893578397e58c9d08b79d7403bc27242aef56957489a278` |
| `ComplementaryUnbound_r5.9.2.zip` | `2b4e9c717758fd1ba1d6306f2e847096704196f0823c4d6941b3de6f423121d2` |
| `BSL_v10.1.8.zip` | `36b0a50ff7918bf10e9422c401d93779f9d0088a26acea975b435243f96930b5` |

Archives are not redistributed in this repository.
