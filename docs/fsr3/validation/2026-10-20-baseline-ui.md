# FSR baseline UI capture

- Device: C1L63708PZ, Android 17 / API 37, ANGLE/Vulkan, FCL 1.3.4.0, MobileGlues 2.0.0.
- Pack: AuroraLumen-Mobile-v2.zip (the FSR3 plan's Complementary baseline is still pending).
- FSR setting: `fsr1Setting=0` in the paired game logs inspected earlier; FSR1 off.
- Inventory and pause UI are visible, correctly scaled, and not washed white.
- Captures are stored outside Git in `/home/polar/MobileGlues-fixtures/captures/`.

| File | SHA-256 |
| --- | --- |
| `2026-10-20-fsr-baseline-inventory.png` | `13068fe5faaa8e18e9a931c48812b508505bddba33e87099405229ffa52a7cae` |
| `2026-10-20-fsr-baseline-pause.png` | `a3dd9f223baa7d1dd15aa5e2e1991c2940a028ac0258b81d6413cf61379f8e5d` |
| `2026-10-20-fsr-baseline-game.log` | `665c5c3b39eaae2e143971abcd423add339bac70cd16258333564f2cd1cc8619` |

This is a baseline for the off path only. It does not qualify FSR1 or FSR3, and it does not use the
Complementary qualification pack.
