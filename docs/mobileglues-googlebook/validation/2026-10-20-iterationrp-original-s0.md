# iterationRP original S0/S1 device evidence

Date: 2026-10-20
Pack: original `iterationRP Alpha 0.8.31.zip`
SHA-256 `334d0b0dadb5e2b927b992f2b93596c4cc1a518305c1e2a9b175c754a4d946e8`.
Installed core: `179afa32…` (G1 RG16F/RG32F partial); bridge `92aa55cf…`.
Profile/options: pack defaults; FSR3 bridge scaling active, dispatch bypassed.

## Result

**S0 PASS; S1 FAIL.** Iris logged `Using shaderpack: iterationRP Alpha
0.8.31.zip` and produced no shader compile/link error. The first rendered
frame is split: only a 960x528 scene-sized region appears in the lower-left;
the rest is sky. This is a viewport/target ownership failure, not a
translation failure.

Screenshot: `/home/polar/MobileGlues-fixtures/iterationRP-original-s0-split.png`
SHA-256 `cef494971c3f5158bc4e11169209554b2642fc16d283655b30564ad3deb00766`. Log: `.../iterationRP-original-s0-split.log`.

## Next isolation

Set FSR3 to NativeAA (scale 1.0) and reload the same original pack. If the
full frame returns, the failure is the FSR bridge scaling/final-viewport path.
If the split remains, it is the pack's own custom render-resolution/viewport
path or an image-unit mismatch. Do not edit the pack.

## NativeAA isolation

With FSR3 NativeAA (scale 1.0), the lower-left split disappears and the full
frame is presented, but terrain is brown/black and the sky is black. This
isolates two independent defects:

1. FSR scene scaling causes the 960x528 lower-left-only frame.
2. The pack/render path still produces wrong colors/lighting at NativeAA.

NativeAA screenshot: `/home/polar/MobileGlues-fixtures/iterationRP-original-nativeaa.png`
SHA-256 `00accade848dbbe4a04ed0a8481f1e3f1edb9eb549be3ca34e0e8fe0b149b224`. Log: `.../iterationRP-original-nativeaa.log`.

S1 remains FAIL. The next isolation is FSR fully off; the bridge must not be a
prerequisite for original-pack support.

## FSR correction

FSR fully off produces the same brown/black lighting as NativeAA; the FSR split
only appears when the scene is scaled. Therefore the remaining S1 failure is
not FSR. The current diagnostic target is Iris custom-image metadata:
`mgirisbridge` is not installed, while the core log shows image units being
assigned but no format producer.

An image-bridge JAR is a **diagnostic only**; it is not a support dependency and
does not qualify the original pack.

## Diagnostic bridge cleanup

`mgirisbridge-1.0.22.jar` was copied to the mods folder for one isolation test,
but the game did not relaunch and the JAR never loaded. FCL's content provider
does not implement delete, and the app is not debuggable, so it was not
removable from the host. It must be removed through the FCL file manager before
the next launch. It is a diagnostic artifact only and must not be listed as a
support dependency.

The stable core `c741b74d…` was reinstalled after the G1 experiment. The G1
source remains committed and host-tested; it is not currently the installed
device build.
