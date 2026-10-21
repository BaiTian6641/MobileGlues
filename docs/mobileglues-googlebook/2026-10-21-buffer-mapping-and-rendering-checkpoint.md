# Buffer mapping correctness and 26.2 terrain investigation

Date: 2026-10-21. This records a general buffer-contract correction under S4.9.
It does not establish that missing terrain, HUD rectangles or any shaderpack
failure is fixed. The physical game results and latest build status remain in
[the runtime record](2026-10-21-fsr3-depth-contract-and-runtime-status.md).

## Finding and correction

Minecraft 26.2's inspected `GlBuffer.Direct` path maps immutable writable
buffers with access mask `0x72`: write, explicit flush, unsynchronized and
persistent. It retains a persistent root mapping; closing each mapped view
flushes before releasing that view, without unmapping the root. Its
`DynamicUniformStorage.writeUniform` uses this path before a draw.

MobileGlues previously enabled `buffer_coherent_as_flush` according to its
bundled-ANGLE selection. When enabled, the policy added coherent/persistent
storage permissions, removed the mapping's explicit-flush bit, and suppressed
flush calls. It did not request coherent mapping access. System ANGLE can still
be the actual provider while this setting is enabled; the renderer string alone
cannot identify the setting.

Khronos specifies coherent mapping through the access flags of
`MapBufferRange`. Coherent storage permits such a mapping; it does not replace
the mapping request. For a noncoherent persistent mapping, explicit flush is the
client-write visibility operation. Removing it cannot promise visibility to
subsequent drawing commands. See the mapping rules in
[ARB_buffer_storage, section 6.3 and issue 1](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt)
and [EXT_buffer_storage, section 6.3](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_buffer_storage.txt).

Commit
[`ed851883ef8234af989943541fdf7a01f7954126`](https://github.com/BaiTian6641/MobileGlues/commit/ed851883ef8234af989943541fdf7a01f7954126)
preserves the caller's storage flags, mapping access flags and explicit flush
ranges. It removes the broad policy and its internal setting/report fields.
Existing borrowed-target handling remains: operations on desktop
`GL_PARAMETER_BUFFER` use a temporary GLES copy-write binding and restore it.
Invalid flags still reach the provider for rejection; the wrapper does not
repair them into valid flags. No extension or limit is added.

Removing a provider-wide workaround can expose an underlying driver defect.
Any replacement needs a reproducible provider-specific failure and demonstrated
visibility semantics. Automatically adding coherent mapping flags would be a
different policy, with its own storage permissions and error behavior; this
correction does not introduce that policy.

## Exact Sodium comparison

The inspected 1.21.1 Sodium 0.8.13 OpenGL path directly invokes
`glMultiDrawElementsBaseVertex`. Sodium 0.9.2 for 26.2 routes its draw through
Mojang `RenderPass` and `GlCommandEncoder`, ending at the same entry point.
Its OpenGL `GLDrawBatch` does not introduce an application indirect-command or
base-instance requirement. The shader's `_draw_id` is decoded from a packed
vertex attribute; the identifier is not evidence of a `gl_DrawID` requirement.
[Old GLRenderDevice](https://github.com/CaffeineMC/sodium/blob/dae4167e7d32a7812556a02d8be73cd5aba459c0/common/src/main/java/net/caffeinemc/mods/sodium/client/gl/device/GLRenderDevice.java),
[new GLDrawBatch](https://github.com/CaffeineMC/sodium/blob/6c26e7b7eded82ce5a1d27f9b147ce5d8de99b7a/common/src/main/java/net/caffeinemc/mods/sodium/client/gpu/device/batch/GLDrawBatch.java).

New dependencies before the draw include the `u_Globals` std140 uniform buffer,
an R32I `u_SectionTimeInfo` texture buffer for fade data, and Mojang's selected
separate-attribute/DSA buffer paths. The exact Iris 26.2 Sodium transformer also
injects `u_Globals`, so the matrix buffer remains relevant with a shaderpack
active. Whether a particular pack retains fade calculations after shader
optimization requires its actual linked shader. This evidence motivates buffer
visibility testing; it does not identify the game's failed draw by itself.
[UniformBufferManager](https://github.com/CaffeineMC/sodium/blob/6c26e7b7eded82ce5a1d27f9b147ce5d8de99b7a/common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/UniformBufferManager.java),
[DefaultChunkRenderer](https://github.com/CaffeineMC/sodium/blob/6c26e7b7eded82ce5a1d27f9b147ce5d8de99b7a/common/src/main/java/net/caffeinemc/mods/sodium/client/render/chunk/DefaultChunkRenderer.java).

| Inspected artifact | SHA-256 |
| --- | --- |
| Sodium 0.8.13+mc1.21.1 NeoForge outer JAR | `575d187b15328d7bddd8a9b1bee64333c22c14735f5780ec49eac0cd5503c08d` |
| Sodium 0.9.2+mc26.2 NeoForge outer JAR | `8364f21b122ffe975392dea12eba0b3bc90609c982113e92dc2e30df2c335805` |
| Minecraft 26.2 client | `40896ee9f1e2bec3c934daac7e93d41e9e3d9c2f8ae0ca366d52ffbfd1afa290` |
| Pinned Iris 26.2 NeoForge | `5faeb646ff117bfc29e636baa9085f6fff6d048a328618749a991fd01c70019d` |

The new Sodium download was verified against its
[official version metadata](https://api.modrinth.com/v2/version/DmnNKsfS).
The comparison inspected the nested NeoForge code JARs and the actual Minecraft
and Iris bytecode, not merely version labels or current development branches.

## Verification and limits

- The [recording-backend regression](https://github.com/BaiTian6641/MobileGlues/blob/ed851883ef8234af989943541fdf7a01f7954126/MobileGlues-cpp/tests/buffer_mapping_test.cpp)
  links the real `buffer.cpp`. It passed persistent `0x72`, nonpersistent explicit,
  coherent, invalid-flag, returned-pointer, relative-range and borrowed-binding
  checks. Independent ASan/UBSan execution passed. This is API-forwarding evidence,
  not GPU visibility proof.
- The pre-fix source, with its old policy explicitly enabled, failed ten checks.
  Only the setting condition and include paths were changed for that negative
  witness. An old zero-initialized setting takes the unaffected branch and would
  not be a meaningful regression witness.
- The [visibility probes](https://github.com/BaiTian6641/MobileGlues/blob/ed851883ef8234af989943541fdf7a01f7954126/MobileGlues-cpp/tools/buffer_visibility_probe/README.md)
  each passed four cases: persistent explicit, nonpersistent explicit, coherent
  and invalid flags. Every rendering case changes a UBO color and vertex positions
  between two fenced draws, uses nonzero mapping offsets and checks lit/unlit
  pixels. One executable runs below the wrapper; the other links the actual
  buffer translation unit while shader/render operations use the provider.
- The actual host was Linux x86_64 with Mesa llvmpipe, LLVM 20.1.8, OpenGL ES 3.2
  Mesa 26.0.0-devel. These are real software-provider results, not physical Adreno
  or hardware-GPU qualification. The adapter uses the provider error queue;
  invalid-flag checks do not qualify the complete wrapper frontend error queue.
- The actual Android ARM64 compiler invocation also parsed the definitions of
  `glMapBufferRange`, `glBufferStorage` and `glFlushMappedBufferRange` successfully.
  This focused buffer AST check is separate from the nine-translation-unit
  baseline lint audit.
- The repository host suite and strict compilation of the new probe main passed.
  Independent source review found no blocker. The ARM64 provider-only probe built,
  SHA-256 `4bf79b442c184b159e9b332a88b351aef8f2edf58f89fff39aa274f2ff8d5fc3`.
  The same binary subsequently ran below MobileGlues on the physical X1E target:
  **all four cases passed**, with no recorded stderr or timeout.

The physical result used Android 17 / API 37, build
`HP/quartz/quartz:17/CL3B.260622.276.R2/16560795:user/release-keys`, and system
ANGLE 2.1 (`89ef25f8f92e103b5bec64e19e36f308aac15164`) over Vulkan 1.4.295,
Adreno X1-85 driver 512.886.6, exposing GLES 3.2. Its JSON SHA-256 is
`c97e9cdde212dad6c60f7233b9a741ec48cccae8dba60637e45d9f76038807b8`.
The device owner ran an offscreen pbuffer executable while the device was locked;
this did not enter the game or access game data. It narrows provider compatibility
uncertainty, but the executable never loads MobileGlues.

Physical provider visibility, full-wrapper error/state behavior and the
Minecraft terrain/HUD result remain separate gates; the later full-library
checkpoint below records their distinct outcomes. The pending terrain-phase
trace must observe actual draw state rather than infer it from sky or a final
framebuffer. The interrupted FSR-disabled comparison still has no rendering
result. S4.9 is not closed by this host slice, and no performance gain is claimed.

## Full-library probe setup checkpoint

A subsequent offscreen attempt loaded the complete production 2.0.7 ARM64
library, SHA-256
`251e9efa71c99891cfcef50b40ccc5ec8980772b0e66902d6590a7dd7e962a21`,
and observed its expected `src-c338d26c9afb945b` marker. The harness returned
exit 1 at **EGL context setup**, before executing any buffer visibility case.
It bound `EGL_OPENGL_ES_API` but requested major version 4. The MobileGlues EGL
adapter rewrites desktop version requests when the bound API is desktop OpenGL;
that rewrite does not make a GLES-major-4 request valid.

The original result remains unchanged, SHA-256
`8acc153ebb9aa1d969453d43e5d592bd95911519c9b5d11c56f69affadca64a2`.
Its OS and GL identity fields are empty because they were captured only after
context creation; the result must not be used as independent provider
qualification. This is a harness setup failure, not a buffer pass or failure.
The revised harness explicitly requests GLES 3 without fallback while retaining
GLSL 330 core input and every EGL/GL call through the exact MobileGlues DSO.
It records OS before context creation, reports requested API/version, leaves
unavailable backend identity empty, and keeps strict frontend error checks.
The revised physical run used probe SHA-256
`4237209f7b055a8e178cd60be20db06a64e7fd870b0f77ce60669fdc9eff814a`
and the same exact production core. It recorded the expected source marker,
Android build and system ANGLE/Adreno identity above. All three visibility cases
passed through the complete wrapper: persistent explicit `0x72`, nonpersistent
explicit and persistent coherent. The strict invalid-flags case **failed** with
`invalid storage flags were not rejected`; the overall process exited 1.
Result JSON SHA-256:
`580024563980a5fb414f64e7bb7ea59a64e6c0aca1691595533041fa8f304b6a`.

This closes the bounded visibility readback for these valid mappings through
that exact library, while exposing a separate frontend error-contract gap.
The error check is retained and the result remains FAIL; the probe must not be
weakened to conceal it. The subsequent correction and passing full-library
physical rerun are recorded below. This result does not establish that the missing terrain or HUD rendering is fixed.


## Follow-up error-contract verification and physical five-case pass

The subsequent bounded correction promotes errors from `glBufferStorage`,
`glMapBufferRange` and `glFlushMappedBufferRange` into the application error
queue. The reviewed source distinguishes fresh application-operation errors
from stale or borrowed-binding diagnostics, preserves the first pending error,
and does not publish an allocation size after a failed storage request.
The actual source SHA-256 at this host checkpoint is
`0523ce684254c6fb81b0dfb56fce457cb8ac9fa483090e0dd0e45d8ddc70c589`;
the semantic change is committed at
[`95499319f3081e51ffc4da1267b0c78eb76f8432`](https://github.com/BaiTian6641/MobileGlues/commit/95499319f3081e51ffc4da1267b0c78eb76f8432).
The [durable probe tooling](https://github.com/BaiTian6641/MobileGlues/commit/c02565ef034fc798686271f053a5e25c67832472)
is a separate test-only commit. Packaging
[`ab4d31db0aa849cb5bbd3446cee91d60a71ef793`](https://github.com/BaiTian6641/MobileGlues/commit/ab4d31db0aa849cb5bbd3446cee91d60a71ef793)
identifies the new candidate as **2.0.8-dev / 2008**. The exact local core and
physical result are recorded below. The corresponding production-build CI artifacts
are independently verified in the [runtime and artifact record](2026-10-21-fsr3-depth-contract-and-runtime-status.md#208-full-library-pass-and-remaining-delivery-gates);
game rendering remains pending. Separate physical runs exercised the local
unstripped core and the exact stripped ARM64 payload extracted from the CI APK;
their core hashes and evidence bindings remain distinct.

Independent review found no blocking issue. The actual `buffer.cpp` and
`getter.cpp` passed **187 focused checks under ASan/UBSan**, and the full host
semantic suite passed. A completed comparison using the previous `ce8b6c6`
source and the same normal/stale/borrowed-error subset failed **38 assertions**;
the corrected source failed none. That comparison stops before the new
missing-dispatch and repeatedly failing-context guard tests; it is not a claim
that the old source completed all 187 checks.

Android compile-command syntax checks and decoded Clang AST covered the three
entry points and the borrowed-target helper. The nine existing header/log-format
warnings matched the previous source by message and count, with zero new
syntax warnings. This focused audit is separate from the earlier
nine-translation-unit lint baseline. Manual security, readability and comment
review confirmed bounded error draining, explicit missing-entry rejection,
unchanged caller flags/ranges/results and preserved error ownership; it does
not establish whole-repository correctness.

The durable probe keeps the four earlier cases and adds `invalid_flush_range`:
a 32-byte explicit-flush mapping at offset 16 must reject a negative relative
flush offset and a range extending past its end with `GL_INVALID_VALUE`, then
accept a legal flush and unmap. Earlier four-case results remain unchanged.
Both the software-provider executable and the adapter linking actual
`buffer.cpp` plus `getter.cpp` pass all five cases on the same host Mesa lane.
The adapter requires the expected buffer errors from the actual frontend queue;
it rejects an error left only in the provider queue. Its raw rendering checks
remain separate so shader/draw errors are not hidden by the frontend getter.

As a negative control, the pre-error-correction `ed85188` buffer source linked
against the same real getter/adapter passes the three visibility cases but fails
both error cases. Thus the adapter can detect the specific error-promotion gap
observed physically. The new complete-library ARM64 harness builds with strict
warnings and no raw EGL/GLES imports; its generic expected-marker argument and
hash-bound runner replace the temporary version-specific pin. The runner's
local-only mode, path/error guards and result-parser rejection fixtures pass.
These host/build checks preceded the separate physical five-case result below.
Minecraft rendering remains pending.

A subsequent physical **below-wrapper** run of the expanded suite passed all
five cases on the same recorded Android 17/system ANGLE/Adreno X1-85 lane, with
exit 0 and empty stderr. Its provider-only probe SHA-256 is
`38631387e7dab2df8b71fa5c4b90dc507663c551ada56dd3df2358c425163704`;
result JSON SHA-256:
`6bcd22714f94e26fe80837df0c897f1c025831307326a9a39f9bbbc1d590a264`.
This specifically adds physical provider evidence for invalid flush ranges.
It never loads the corrected MobileGlues library. The separate full-library
run below closes the bounded wrapper gate; both earlier four-case results
remain immutable.


The **complete MobileGlues 2.0.8 library subsequently passed all five cases**
on the physical target, with exit 0 and empty stderr. The run recorded the same
Android build, system ANGLE revision, GLES 3.2 and Adreno X1-85 driver detailed
above. EGL and every GL entry point, including shader compilation and
`glGetError`, resolved through the exact supplied DSO. The expected source
marker was `src-beaba336ea4452da`.

| Physical full-library 2.0.8 evidence | SHA-256 |
| --- | --- |
| Production ARM64 core | `966fef021838e35f58c7889fff7c4105b8ada3379c0e9c87c4484de33d277912` |
| Five-case full-library probe | `27cd1c9ddc987e472472c5200bbcc035f741f3ee3d8359d9f55dcc25d9ac2105` |
| Immutable result JSON | `edb91b4779e1896617386b7bcca87e6324acf290c327a91231d67379f2495ed0` |

This establishes the five tested valid-mapping visibility and application-error
contracts through that exact library/provider. It does not establish every
S4.9 mapping or synchronization behavior, Minecraft terrain/HUD correctness,
shaderpack effect fidelity, temporal FSR3 or performance. The 2.0.7 failure
remains in the evidence chain; it is not overwritten by the new result.


The exact stripped ARM64 core from verified CI artifact `12335463401` was also
extracted and tested in a separate physical full-library run. Core SHA-256
`ad68056127073cd540d148ad73a8063b6cec44d68256d109940247b3a526af94`
passed the same five cases with exit 0, the expected marker and the same recorded
OS/provider. Independent verification matched the APK entry bytes to the
runner's core hash and pushed binary hash.

Its result JSON is byte-identical to the earlier local-core result because all
reported identity and case fields match; both have SHA-256 `edb91b4779e1896617386b7bcca87e6324acf290c327a91231d67379f2495ed0`.
The separate runner evidence binds the CI payload rather than the local core:
its SHA-256 is `92c5d1eeca1f6157e0df754b8577fe0783798e203d934e37a1030d80459b5bbd`.
Both runs are retained. This adds exact-CI-payload physical acceptance only for
the five tested buffer contracts; it does not qualify Minecraft rendering or
the full APK/application lifecycle.
