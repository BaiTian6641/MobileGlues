# Mapped buffer visibility probe

This probe checks persistent explicit-flush mappings, nonpersistent explicit-flush
mappings, coherent mappings, invalid flag rejection, and invalid flush ranges. Each visibility case
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
  links the actual `gl/buffer.cpp` for buffer entry points and `gl/getter.cpp`
  for the application-visible error queue. Its build renames that
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

The original Android build produces the below-wrapper provider executable:

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


## Complete MobileGlues library lane

The separate full-library harness resolves every EGL/GL entry point with
`dlsym` on one explicitly supplied core and verifies symbol ownership with
`dladdr`. It links no EGL/GLES library and has no provider-resolution fallback.
The build extracts the actual provider probe's shader, mapping, fence, readback
and invalid-flag bodies into a generated include; only the shader dialect
(GLSL 330 core) and dispatch through the DSO differ. Extraction boundaries and
shader patterns are checked so changes require review instead of silently
selecting different tests. `main.cpp` remains the provider lane's source.

```sh
python3 MobileGlues-cpp/tools/buffer_visibility_probe/build_full_wrapper.py \
  --output-dir /tmp/mg-full-buffer-build
```

This builds Android ARM64 with pinned-default NDK r28c and strict warnings,
writing the binary, isolated test config and a source/artifact manifest.
`--target host` builds the same harness for path/error-output tests; this is
not a host MobileGlues runtime result. The build does not run a device.

The runner requires an exact expected core SHA-256 and source marker. Without
`--execute`, it performs local checks and prints the proposed binding only:

```text
python3 MobileGlues-cpp/tools/buffer_visibility_probe/run_full_wrapper.py \
  --serial <explicit-device> --library <absolute-core-path> \
  --probe-dir /tmp/mg-full-buffer-build \
  --expected-core-sha256 <64-hex-digest> --expected-marker <src-marker> \
  --output <new-evidence-directory>
```

The coordinated device owner can add `--execute`. The runner creates a new
hash-named directory under `/data/local/tmp`, verifies pushed core/probe/config
hashes, sets `MG_DIR_PATH` there before loading MobileGlues, and applies a
60-second remote timeout. The harness rejects an absent/outside/bare-root
configuration path, and its isolated config keeps error reporting enabled and
FSR disabled. Shared renderer settings, logs and caches are not selected.
The runtime getter must confirm `IgnoreError: None` and the exact expected
source marker before any test case runs.

The EGL request is explicitly GLES3 without fallback; desktop GLSL still enters
MobileGlues' shader compiler. The initial development harness's GLES-major4
request failed context setup before any test case; that historical failure is
separate from the corrected harness. OS identity is recorded before context
creation. Actual backend identity is recorded only after successful creation;
it remains empty with an unavailable flag when no context exists.

Any missing capability/symbol, compile/link failure, invalid-flag acceptance,
wrong pixel/flag, synchronization failure, unexpected error or failed context
cleanup yields a failure. The runner preserves the raw result and diagnostics,
requires all six distinct cases to pass, and rejects missing/invalid JSON,
timeouts, wrong markers or missing provider identity. A completed build or
three passing visibility cases with a failing error case cannot satisfy that
overall gate.

For the narrower host adapter, shader/draw operations still call the provider.
It checks that raw queue before invoking the actual frontend getter, so raw
rendering failures cannot disappear as internal wrapper diagnostics. Buffer
errors promoted by the real buffer entry points are then checked through the
real frontend. Unexpected simultaneous errors in both queues fail the test. Expected buffer
rejection checks additionally require the actual frontend error and reject any
residual provider error, so an unpromoted backend error cannot satisfy them.
This preserves the distinction from the full-library lane, where every call,
including `glGetError`, is a MobileGlues frontend call.


The fifth case, `invalid_flush_range`, maps 32 bytes at buffer offset 16 with
explicit flush access. It requires `GL_INVALID_VALUE` for a negative relative
flush offset and for a range exceeding the mapping, then verifies that a legal
flush and unmap succeed. It performs no image-visibility comparison. Earlier
four-case reports remain four-case evidence and are not relabelled as this
expanded suite.

The sixth case, `dsa_persistent_explicit_0x72`, exercises the DSA entry points
used by Minecraft 26.2: `glNamedBufferStorage`, `glMapNamedBufferRange`,
`glFlushMappedNamedBufferRange`, `glCopyNamedBufferSubData`, and
`glUnmapNamedBuffer`. It keeps the source mapped while copying, then fences and
reads the destination through a legal read mapping. This closes the prior gap
where only the target-bound `glMapBufferRange` path was tested. It does not
cover `glGetNamedBufferSubData`, whose underlying `glGetBufferSubData` entry
point is currently a stub.

Host-only validation of the full-wrapper guard and result parser can be run
without any device or graphics library:

```sh
python3 MobileGlues-cpp/tools/buffer_visibility_probe/build_full_wrapper.py \
  --target host --output-dir /tmp/mg-full-buffer-host
python3 MobileGlues-cpp/tools/buffer_visibility_probe/test_full_wrapper.py \
  --host-probe /tmp/mg-full-buffer-host/buffer-full-wrapper
```
