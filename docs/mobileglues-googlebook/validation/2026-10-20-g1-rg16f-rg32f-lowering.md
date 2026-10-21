# G1 partial: RG16F/RG32F image legalization

Date: 2026-10-20
Host-only change; no device run.

## Change

- `gl/glsl/compat_image.cpp` lowers `rg16f` declarations to `rgba16f` and
  `rg32f` to `rgba32f`.
- `gl/image_metadata.cpp` maps RG16F backing to RGBA16F and exposes the
  `rgba16f` qualifier.
- `gl/texture.cpp` widens null RG16F image storage to RGBA16F, matching the
  qualifier and bind format.
- `tests/compat_image_test.cpp` pins both lowerings.
- `tests/image_metadata_test.cpp` pins backing/qualifier/unit mappings; added
  to `tests/run.sh`.

## Verification

`sh MobileGlues-cpp/tests/run.sh` -> rc 0; `compat_image` and
`image_metadata` all checks passed. This is host syntax/contract evidence, not
an iterationRP compile or render.

## G1 status

**PARTIAL.** The provider-rejected `rg16f`/`rg32f` image qualifiers now have a
legal RGBA backing. Still open:

- target-aware RGBA16/RG16 image-bound attachments;
- stage-scoped image rewriting (G4);
- core-only deterministic image units (G2);
- an original-iterationRP host transform sweep and device S0/S1.

Do not promote iterationRP from this result.
