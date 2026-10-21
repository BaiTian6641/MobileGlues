# Scoped code quality checks

Run `python3 tools/quality/audit.py` with Clang installed. The default check
compiles the owned shader-source/image translation units with `-Wall -Wextra
-Werror`, emits and parses Clang's JSON AST for declarations matching `mg_`,
rejects process-execution references in that AST inventory, and runs the real FSR session ownership API against a recording
backend to verify teardown after a context replacement. The recording backend
is a host fixture and does not qualify GPU behavior.
The real image-uniform validation helper also compiles with strict warnings and
runs its scalar/array/error-state contract tests.

The compiler parses each complete translation unit. The smaller emitted AST
inventory deliberately excludes unrelated vendor/header declarations and is
not a whole-program call graph or an exhaustive security scan.

For Android/JNI coverage, configure the ARM64 CMake build first, then add
`--compile-db build/android-arm64-release/compile_commands.json`. This parses
the actual Android compile commands and runs clang-tidy analyzer, bugprone,
CERT, performance and misleading-indentation checks. It requires clang-tidy
from the compiler directory or PATH. Native diagnostics may not exceed the
per-file/per-rule baseline in `native-baseline.json`. Any baseline adjustment
requires a human finding review; do not regenerate it to make CI green.

Reports retain commands, source hashes, ASTs and diagnostic counts in
`build/quality/`. Warnings are **not globally disabled**. A successful run means
only that the scoped checks passed; it does not mean the repository has no
defects. The Android build still has older warning suppression outside this
audit. Existing tolerated diagnostics include dimension conversions,
adjacent dimension parameters, bounded `snprintf`
results and camera-state static initialization. These are recorded review debt,
not dismissed as automatically harmless.

The context-release `shared_ptr` value parameter is intentional: the function
erases the map entry that can own its input. Changing it to a reference, as the
performance checker suggests, would invalidate the input during release. The
three adjacent EGL argument warnings reflect the existing EGL API and wrapper
signatures. The direct FSR path's four int-to-float conversions and three
dimension parameter warnings predate the lifecycle correction. These exact
rules and counts are the baseline for those two added audit files.

The image-uniform correction adds `drawing.cpp` and `image_bindings.cpp` to the
Android audit. Their seven existing warnings cover one static sampler cache,
two adjacent-parameter pairs, two unchecked diagnostic writes, the existing
base-vertex fallback switch, and `atoi` on driver-reported array indices. They
were reviewed against the unchanged source sections, not introduced by the
null-unbind or image-uniform-write corrections. The complete nine-file native
baseline now contains 26 diagnostics; new rules or increased counts fail CI.

With the Android NDK compiler on an x86_64 Linux host, the host check can use:

```sh
python3 tools/quality/audit.py \
  --compiler "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++" \
  --target x86_64-linux-gnu \
  --compile-db build/android-arm64-release/compile_commands.json
```

Comment/readability review is a human gate. Check that comments describe the
current resource ownership, limits and failure policy, remove stale experiment
claims, and keep host, build, device and product acceptance evidence distinct.
Compiler/AST success cannot establish that prose is accurate.
