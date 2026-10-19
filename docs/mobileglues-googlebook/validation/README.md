# Validation Artifacts

Store machine-readable results and reproduction metadata here. Binaries, shader archives, and captured
images belong outside Git; record their path and checksum instead.

| Artifact | Date | Level | Command | Result |
| --- | --- | --- | --- | --- |
| `2026-10-18-x1e-provider-probe.json` | 2026-10-18 | target device, below wrapper | see below | provider limits/format/behavioral probe on `C1L63708PZ` |
| `2026-10-18-x1e-device-identity.txt` | 2026-10-18 | target device | `adb -s C1L63708PZ shell 'getprop …'` | HP Googlebook 14, X1E80100, Android 17/API 37, 4096-byte pages |
| `2026-10-19-x1e-wrapper-stage1-stage2.json` | 2026-10-19 | target device, wrapper level | see below | shader-source, status, limit, frag-data and image-binding contract results |

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
implemented. Stored result hash:
`385e8dab0120ad164b051e7608ea553d5dad9060a41df30a037deae467928964`.

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
