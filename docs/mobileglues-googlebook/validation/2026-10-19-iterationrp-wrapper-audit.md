# iterationRP wrapper audit

Date: 2026-10-19  
Device: HP Googlebook 14 (quartz), X1E80100, Adreno X1-85, ANGLE Vulkan 1.4.295, ES 3.2.

## Wrapper fixes landed

- `glClearBufferSubData`/`glClearBufferData` now zero immutable SSBO storage with a
  scratch `glCopyBufferSubData`; `glBufferSubData` is invalid without
  `GL_DYNAMIC_STORAGE_BIT`.
- `glBindBufferBase`/`glBindBufferRange` update the generic-binding shadow.
- Null-data 2D image allocations use immutable `glTexStorage2D`; 3D already did.
- `layout(rgba16)` / `layout(rg16)` image uniforms and null-data image storage are
  lowered to `rgba32f` / `rg32f` (ES has no unorm16 storage-image qualifier).
- Plain 2D image binds lower `layered=TRUE` to the equivalent unlayered bind.
- A rejected `glBindImageTexture` is no longer recorded as a valid physical bind.
- `glDeleteTextures` drops logical image-binding records for deleted names.
- glslang pre-pass lowers `const matN X = transpose(Y)` and renames the
  reserved identifier `active`.
- Geometry limit queries also run on core ESSL 3.20, not only when the ES 3.1
  extension strings are present.

## Verification

- Host tests: `sh MobileGlues-cpp/tests/run.sh` PASS.
- Wrapper probe: 58/58 PASS, including `image2d_bind` and `image2d_fbo_bind`
  (2880x1585 immutable RGBA16F attached to an FBO).
- Derived r2 pack passes translation for every enabled shader; no compile/link
  failure remains in the uploaded log.
- Device rendering from r2 is still visually incorrect (orange terrain, black
  sky). Direct image bind, FBO-attached bind, immutable clear, and the full probe
  suite pass, so the remaining fault is not basic wrapper binding.

## Remaining hard limitation

The original iterationRP archive uses `GL_KHR_shader_subgroup_*` and
`GL_NV_shader_subgroup_partitioned` unconditionally in SPD, Exposure, SH_TRACING,
BuildingWarp and BackwardAnalysis. The provider exposes no subgroup extension.
No correctness-preserving generic wrapper emulation exists for divergent
subgroup quad operations; a pattern-specific shader transform or a different GL
route is required. The private r2 port demonstrates the transform but still
needs visual debugging against a desktop reference. No shaderpack is modified in
the repository.
