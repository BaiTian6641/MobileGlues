# AGENTS.md

Project: MobileGlues X1E Googlebook implementation. The requirements baseline is
`docs/01_MobileGlues_Googlebook_Architecture.md`, with the execution plan in
`docs/02_MobileGlues_Googlebook_Implementation_Plan.md` and acceptance/sources in
`docs/03_MobileGlues_Googlebook_Validation_and_Sources.md`. Execution records live in
`docs/mobileglues-googlebook/`; versioned distribution locks live in `compat/`. The older
`docs/MobileGlues_Googlebook_X1E_Optimization_Plan.md` is historical and superseded.

## Target runtime lane

The delivery lane is Minecraft 1.21.1 on NeoForge 21.1.264 with Java 21
(`java-runtime-delta`), Iris 1.8.14-beta.1+1.21.1-neoforge (Modrinth `KduFYu4t`) and
Sodium mc1.21.1-0.8.13-neoforge (`uMOpc5uV`). Ordinary shaderpack ZIPs must import,
select their own profile and render their enabled effects in that lane with
desktop-equivalent OpenGL/GLSL semantics. The stable fallback pair is Iris 1.8.12
NeoForge (`t3ruzodq`) with Sodium 0.6.13 NeoForge (`Pb3OXVqC`); never pair Iris 1.8.12
with Sodium 0.8.13.

The installed instance is NeoForge 21.1.262; its game-process smoke is preliminary
evidence only. The exact 1.21.1 / 21.1.264 tuple and all effect, performance and
lifecycle gates are **NOT RUN**. Minecraft 26.3 / Java 25 / LWJGL 3.4.3 SDL on Fabric
is future/experimental; Minecraft 1.20.1 / Java 17 Fabric is legacy. Neither replaces
the delivery lane, and 26.3 findings in the docs are retained as future-lane evidence.

## Target hardware

A real target device is connected over ADB:

| Field | Value |
| --- | --- |
| ADB serial | `C1L63708PZ` |
| Product | HP Googlebook 14 (`quartz`) |
| SoC | Snapdragon X Elite X1E80100 (Adreno X1-85) |
| OS | Android 17 / API 37, build `HP/quartz/quartz:17/CL3B.260622.276.R2/16560795:user/release-keys` |
| ABI | `arm64-v8a` |
| Page size | 4096 bytes (the release artifact still ships 16 KB-aligned ELF segments) |
| Display | 2880 × 1800, 120 Hz capable; density 260 |
| System EGL | `ro.hardware.egl=angle`; ANGLE 2.1 over Vulkan 1.4.295, Adreno driver 512.886.6 |
| Installed launcher | FCL `1.3.4.0` (`com.tungsten.fcl`) |
| Installed renderer plugin | MobileGlues `2.0.0` (`com.fcl.plugin.mobileglues`) |

Use the serial explicitly when more than one device is attached. The x86_64
emulators (`emulator-5580`, `emulator-5582`) are not target devices.

### Device-use rules

- Do not reboot, factory-reset, clear launcher data, change the system graphics
  provider, install a system image, or modify `/system` or `/vendor` unless the
  user explicitly authorizes it.
- Push test binaries only to `/data/local/tmp` or app-owned storage. Keep game
  worlds and user settings intact.
- Capture the actual provider/renderer and OS build in every result. Emulator,
  host, and X1E evidence are separate levels and must not be relabelled.
- Preserve original shaderpack archives byte-for-byte. Fixtures are stored in
  `/home/polar/MobileGlues-fixtures/`; only hashes belong in Git.

## Build and test

Host semantic tests:

```sh
sh MobileGlues-cpp/tests/run.sh
```

ARM64 release build (pinned NDK r28c):

```sh
NDK=$HOME/Android/Sdk/ndk/28.2.13676358
cmake -S MobileGlues-cpp -B build/android-arm64-release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DANDROID_STL=c++_static -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/android-arm64-release -j 24
```

Below-wrapper device probe:

```sh
ANDROID_NDK=$HOME/Android/Sdk/ndk/28.2.13676358 \
  sh MobileGlues-cpp/tools/capability_probe/build-arm64.sh
adb -s C1L63708PZ push MobileGlues-cpp/tools/capability_probe/capability_probe_arm64 /data/local/tmp/
adb -s C1L63708PZ shell 'chmod 755 /data/local/tmp/capability_probe_arm64 && /data/local/tmp/capability_probe_arm64 /data/local/tmp/probe.json'
adb -s C1L63708PZ pull /data/local/tmp/probe.json
```

The probe must remain below MobileGlues. Do not "fix" a failed provider test by
advertising an extension or raising a limit in the wrapper.

## Engineering rules for this task

- Extend existing MobileGlues mechanisms; do not add a parallel backend or a
  Vulkan rewrite without a demonstrated constraint and a bounded experiment.
- Keep application GL errors distinct from internal diagnostics. Never mask a
  failed compile/link as success in qualification runs.
- Do not guess image formats or force loader feature macros. The Iris metadata
  bridge must describe real resources before ESSL compilation.
- Keep semantic fixes, toolchain changes, and performance experiments in
  separate commits. Review the staged diff; never commit unrelated user work.
- Record evidence, not claims. A completed stage needs its stated gate, and
  device/performance gates stay pending without target evidence.
