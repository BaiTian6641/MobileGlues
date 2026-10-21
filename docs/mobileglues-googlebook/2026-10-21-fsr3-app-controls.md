# FSR3 app controls packaging correction

The `2.0.2-dev` APK omitted the FSR3 enable switch and quality slider. Its native
core supported both settings, but the packaging script used the upstream plugin
revision before the maintained app's FSR3 additions. The previous unit tests did
not cover those fields. This was a packaging regression, not a removed renderer
feature.

Correction commit: `ad59bf1bab4c1ee82bf7570fe78db8006dcbbc6a`.
App version: **2.0.3-dev**, version code **2003**.

## Correction

The build now applies the reviewed app-only patch from maintained plugin
revision `f84ba048d76b6ded7657f44f6126c39bd41d33e7` to the pinned upstream
revision `f5105e4ddd1752cccdcc6421c402960d40ec3d9d`. It restores:

- FSR3 enable/disable and the shared `fsr3Setting` configuration key.
- Native AA, Quality, Balanced, Performance and Ultra performance selections,
  saved as `fsr3Quality` values 0 through 4.
- Material and Miuix settings controls, with English and Chinese resources.

The explanatory text distinguishes bridge-dependent scene scaling from the
direct presentation path, states FSR3's precedence over FSR1, and tells users
to restart the game after changing FSR3 settings. The patch corrects two
misplaced cache-slider comments. The maintained sibling checkout and native
renderer implementation were not changed.

Patch SHA-256:
`f17ec2099ff3e9567602e1ec42230d4d6a8690f92ed232f74ac1a87df5bced06`.
The build records this hash and the new regression-test hash in its provenance.
A second clean preparation reproduced all 128 app source/test, build configuration
and provenance files used by the locally built APKs.

## Verification

- Both local debug and unsigned release APKs built successfully with ARM64 and
  x86_64 payloads. All packaged native libraries passed the 16 KB ELF alignment
  check.
- **57 unit tests passed**, with no failures, errors or skipped tests. Six new
  codec tests cover all ten mode/quality combinations, old configs, invalid
  values, numeric strings and preservation of unrelated nested configuration.
- Android lint reported **zero errors and 47 existing warnings**. Independent
  review found no blocking findings in the patch, scripts or tests.
- The new artifact gate rejects both earlier CI APKs because they lack FSR3 UI
  resources. Both corrected APKs pass the resource and DEX-key gate. Independent
  `aapt2` inspection also confirmed the exact FSR3 resources and Chinese strings.
- On the physical X1E Googlebook running Android 17 with ANGLE, the corrected
  APK was installed as an update. The app displayed its existing FSR3 enabled
  state and Balanced quality. All five Material quality positions were selected
  and observed. A subsequent check selected Performance and verified it after
  restarting the app, then restored Balanced and verified it after another
  restart. Miuix also displayed the FSR3 switch and Balanced
  selection. The original Material theme was restored and verified.

The app was left on the FSR3 settings screen with **FSR3 enabled / Balanced**.
Raw UI layouts, the OS/provider record and a screenshot remain in local
qualification evidence. This check establishes app controls and settings
persistence; it does not establish FSR image quality or performance in a game.

| Local artifact | SHA-256 |
|---|---|
| Debug APK installed for this check | `b2afc38e0539126925c64521e86d2dc127345bbfb2287d4c7a1c3184cb4d82c1` |
| Unsigned release APK | `bcd49a35e906e20a8c0276f0bcf672220ba34442cf0ae7f0ab23e4df97ab03d1` |

GitHub Actions run:
[corrected APK build](https://github.com/BaiTian6641/MobileGlues/actions/runs/39557734453).
**Passed**, including both native builds, host semantic tests, scoped native
AST/analyzer checks, app builds/tests/lint, APK validation and artifact upload.
Both uploaded APKs were downloaded and independently passed the same FSR3
resource/config-key and native ABI/alignment verifier.

[Download the APK artifact bundle](https://github.com/BaiTian6641/MobileGlues/actions/runs/39557734453/artifacts/12326229367).
It contains a debug-signed APK, unsigned release APK and provenance/checksums.

| Downloaded CI artifact | SHA-256 |
|---|---|
| Debug APK | `339b941f68aae5eabcf53bc2df53cc6393148c47beff461b7d803ba6af71eec0` |
| Unsigned release APK | `e949884f774412b8f9e9ccec753b8bacc2bcfb24aed456377615c9480f363c70` |
