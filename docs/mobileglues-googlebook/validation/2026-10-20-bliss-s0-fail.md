# Bliss 2.1.2 S0 device failure

Date: 2026-10-20
Device: C1L63708PZ, X1E80100 / Adreno X1-85, ANGLE Vulkan 1.4.295, GLES 3.2.
Pack: `Bliss_v2.1.2_(Chocapic13_Shaders_edit).zip`
SHA-256 `f41db92acc585fe9ffe0cc124d6ecaea27cef9ded91129ebc2a8e60226947b2c`.
Installed core/bridge: `c741b74d…` / `92aa55cf…`.
Profile: default (LPV off).

## Result

**S0 FAIL.** Iris selected the pack, then reported:

```
Shader compilation log for deferred.fsh: ERROR: 0:948: 'uniform' : syntax error
The shaderpack failed to load! ...
Failed to create shader rendering pipeline, disabling shaders!
```

This is a truthful failure, not a silent fallback. Shaders were disabled after
the pipeline error. No Bliss render can be evaluated.

Evidence:

- screenshot: `/home/polar/MobileGlues-fixtures/Bliss-Solas/evidence-2026-10-20-bliss-fail.png`
  SHA-256 `a2b1058555309a7c8911af6b42c5cf6198307885b8c2fbec15ef2b2dd6990d33`
- log: `.../evidence-2026-10-20-bliss-fail.log`
  SHA-256 `8f7b4f1fa4ade03a41844b29a1fcb9319a659dad1f0ccab9dc0dfa3c95bf21f3`

## Next diagnostic

The failing source is the Iris-transformed `deferred.fsh`; the raw archive is
only 385 lines, so line 948 is in the expanded/generated source. The next step
is to enable Iris `enableDebugOptions` temporarily and dump
`patched_shaders/deferred.fsh`, then compare the generated ESSL around the
reported line. Do not edit the Bliss archive. Do not claim Bliss S0/S1 until
this compile error is fixed in the core/Iris boundary.
