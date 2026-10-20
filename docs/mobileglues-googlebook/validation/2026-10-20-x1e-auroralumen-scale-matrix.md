# AuroraLumen X1E scale/profile performance matrix

Date: 2026-10-20
Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85,
Android 17 / API 37, system ANGLE over Vulkan 1.4.295, 2880x1800 at 120 Hz.
Installed renderer: APK `7e8a85a3…`, core `c741b74d…`, bridge `92aa55cf…`.
FSR3 dispatch bypassed (`debug.mgfsr3.bypass=1`), history off.
Original pack archive: `AuroraLumen-Mobile-v2.zip`, SHA-256
`93562b77c4cd0e67bdc02a459ed6201f199c5b9485c12aaecfc82ef53a0792ad`.

## Scope and caveat

These are short user-operated route samples with F3 and SurfaceFlinger
presentation intervals, not sustained benchmarks. The camera path and pack
options were not held identical across every row; profile selection reset
some pack options. Rows are evidence for the scale/profile tradeoff, not a
controlled per-option A/B and not a thermal or lifecycle qualification.

## Measurements

| Configuration | Scene target | F3 | SurfaceFlinger median | p95 |
| --- | ---: | ---: | ---: | ---: |
| Quality 1.5, original pack, Custom +4 | 1920x1057 | 41 FPS | not captured | trace median 34.9 ms |
| Performance 2.0, original pack, Custom +4 | 1440x793 | 62 FPS | 16.67 ms (60 FPS) | 25.0 ms |
| Ultra 3.0, original pack, Custom +4 | 960x528 | 58 FPS | 16.67 ms (60 FPS) | 33.33 ms |
| Ultra 3.0, PerfTest pack, MOBILE +0 | 960x528 | 93 FPS | 8.33 ms (120 FPS) | 16.67 ms |
| Ultra 3.0, original pack, MOBILE_MC_CLOUDS +5 | 960x528 | 93 FPS | 8.33 ms (120 FPS) | 16.67 ms |

F3 reported GPU 100% in every row; host-side CPU was largely idle. The wrapper
CPU microbenchmarks (421.9/552.0 ns guard, 22.1 ns identical image bind) are
below the frame-time scale and cannot explain the scale dependence.

## Interpretation

- The dominant lever is scene extent. Quality -> Ultra raised the observed
  frame rate from 41 to 93 FPS, but the intermediate Performance point is the
  useful quality/FPS balance.
- The 58 -> 93 FPS difference at the same 960x528 extent is confounded by pack
  profile/options and route. It is not evidence of a wrapper improvement.
- A private `AuroraLumen-Mobile-PerfTest.zip` (`251a6be2…`) was tested with an
  early return from the `composite` SSR pass when both PBR and WSR consumers
  are compiled out. It produced 93 FPS, but the original pack at MOBILE_MC_CLOUDS
  also produced 93 FPS. The SSR early return is therefore **not adopted** and no
  speedup is claimed.
- The original archive was not modified. The PerfTest ZIP is a temporary
  private test artifact, not a release input.
- SurfaceFlinger intervals at 8.33 ms indicate the display can present at
  120 Hz; F3's 93 FPS is the application-limited rate, not a display cap.

## Current recommendation

Keep all measured scale points user-selectable. Performance 2.0 is the
recommended quality/FPS balance; Ultra 3.0 is the maximum-FPS experimental
point. No default or pack-option change is justified by this evidence.

Further GPU work must target a measured pass (light shafts, aurora, clouds,
post chain) or a clean same-profile A/B; wrapper micro-optimization is not the
next frame-time lever for this pack.
