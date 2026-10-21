# Mapped buffer visibility probe

This probe checks persistent explicit-flush mappings, nonpersistent explicit-flush
mappings, coherent mappings, and invalid flag rejection. Each visibility case
writes a color through a nonzero-offset uniform-buffer range and positions through
a nonzero-offset vertex-buffer mapping. It draws twice, changing both values,
waits on a bounded GPU fence, and checks lit and unlit pixel samples. It also
checks storage and mapping flags as reported by the provider.

The explicit persistent access mask is `0x72`, used by Minecraft 26.2's
`GlBuffer.Direct`: write, explicit flush, unsynchronized, persistent. Flush offsets
are relative to the mapping, so both mappings begin at nonzero offsets while each
flush begins at zero. Caller synchronization is preserved: the first draw is
complete before the second write.

Two host executables distinguish the evidence levels:

- `provider` links platform EGL/GLES and never links MobileGlues.
- `real-buffer` uses the same provider for shader compilation and rendering, and
  links the actual `gl/buffer.cpp` for buffer entry points. Its build renames that
  translation unit's exported GL symbols in the object files so they cannot
  interpose unrelated provider operations. This exercises buffer translation;
  it is not a full MobileGlues/EGL/shader integration test.

Build and run on a Linux host with EGL/GLES development packages, a C++20 compiler,
GNU `nm` and `objcopy`:

```sh
sh MobileGlues-cpp/tools/buffer_visibility_probe/build-host.sh
/tmp/mg-buffer-visibility/provider
/tmp/mg-buffer-visibility/real-buffer
```

`OUT_DIR` changes the output directory. The build does not execute either probe.
Each execution prints JSON with the actual OS, vendor, renderer, GLES version,
case results, and overall status; failures produce a nonzero exit status. A Mesa
llvmpipe result is software-provider evidence, not physical-GPU qualification.

The Android build produces only the below-wrapper provider executable:

```sh
ANDROID_NDK="$HOME/Android/Sdk/ndk/28.2.13676358" \
  sh MobileGlues-cpp/tools/buffer_visibility_probe/build-arm64.sh
```

The default output is `/tmp/mg-buffer-visibility-arm64`; override `OUT` to change
it. It links only platform EGL/GLES libraries. It does not initialize MobileGlues,
change advertised capabilities, or modify device state. A device owner can run
it from `/data/local/tmp` during a separately coordinated qualification session.
The Android result includes `ro.build.fingerprint`. Build success is not runtime
qualification.

## Why the forwarding policy changed

The former `buffer_coherent_as_flush` policy added coherent/persistent permissions
to storage, stripped explicit flush from mapping access, and suppressed flush
calls. It did not request coherence in the mapping access flags. Storage
permissions and mapping coherence are separate contracts. The corrected wrapper
preserves caller flags and flush ranges, leaving validity and visibility to the
provider. It does not add an extension or raise a limit.

Khronos sources: [ARB_buffer_storage](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_buffer_storage.txt)
and [EXT_buffer_storage](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_buffer_storage.txt),
particularly the mapping access and client-write visibility rules. The caller
must still obey synchronization and flush requirements; passing a coherent
storage flag alone is insufficient to request a coherent mapping.

Removing the old provider-wide workaround can expose a driver defect that it
previously obscured. Such a defect needs a provider-specific reproducer and a
bounded workaround with demonstrated visibility. The 26.2 missing-terrain
symptom is not established as cured by this change.

The separate host test `tests/buffer_mapping_test.cpp` links the real translation
unit to a recording backend. It checks exact flags, ranges, returned pointers,
backend rejection propagation, and borrowing/restoration for
`GL_PARAMETER_BUFFER`. That test does not establish GPU visibility. To reproduce
the old-policy negative result, the pre-fix source must run with its old setting
enabled; a zero-initialized old setting takes a different path and is not a valid
negative control.
