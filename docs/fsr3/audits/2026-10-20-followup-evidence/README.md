# Evidence for the follow-up review

This directory contains host review evidence and a recalculation of an existing
device log. **No device command was run by this review.** Concurrent device
records are linked from the [review](../2026-10-20-followup-review.md), with
their own limitations.

| File | Meaning |
| --- | --- |
| `camera_capture.cpp`, `.log` | Exact camera state/capture source extracted from `d72acc0`, with substituted JNI array transport. Second/third captures never acquire previous history: expected FAIL, executable exit 1. |
| `camera_capture_after.cpp`, `.log` | Same host seam after the concurrent continuity correction, later committed in `41a58f7`: captures produce `0,1,1`, PASS, exit 0. |
| `viewport_capture.cpp`, `.log` | Exact `FSR3AdjustViewport` from `5d7ac39`, with substituted GL query results. A 4096² color target and deliberate subviewport are both overwritten: expected FAIL, executable exit 1. |
| `fsr3_scene_plan_at_5d7ac39.h` | Pinned planner included by the viewport reproduction. The failing branch precedes this planner. |
| `host-suite.log` | Initial 11-executable host semantic suite, during the review starting at `d72acc0`. This is a captured run, not a fully archived checkout. |
| `host-suite-final.log` | Repeat of the existing host suite at the `5d7ac39` checkpoint, exit 0. Includes the concurrently modified framebuffer test's new viewport stub. |
| `host-suite-final-test.patch` | That one-line test change, retained so the final suite is not misrepresented as a pristine-commit run. |
| `contract-examples.log` | 21 checks of the original documentation examples. Does not execute product paths. |
| `timing-recalculation.json` | Arithmetic over the three complete rows of the saved per-pass log. Includes the source-log hash and validity limitation. |
| `manifest.json` | Source revisions, evidence levels, command outcomes and SHA-256 hashes. |

The JNI transport seam replaces Java arrays with local C++ storage. It does
not validate JNI type safety, Java class/mixin loading, game frames or GPU
motion. The viewport seam supplies legal query results without a real GL
context. It demonstrates the control-flow defect, not a captured shadow
artifact. The existing host suite does not execute the full Android JNI path.

## Reproduction commands

Run from this directory with a Java 21 JDK installed. The shown JDK path is
the path used on the review host:

```sh
g++ -std=c++20 -O0 \
  -I /usr/lib/jvm/java-21-openjdk-amd64/include \
  -I /usr/lib/jvm/java-21-openjdk-amd64/include/linux \
  camera_capture.cpp -o /tmp/mg-fsr3-camera-before
/tmp/mg-fsr3-camera-before
# Expected exit 1: reproduces the reviewed defect.

g++ -std=c++20 -O0 \
  -I /usr/lib/jvm/java-21-openjdk-amd64/include \
  -I /usr/lib/jvm/java-21-openjdk-amd64/include/linux \
  camera_capture_after.cpp -o /tmp/mg-fsr3-camera-after
/tmp/mg-fsr3-camera-after
# Expected exit 0: the focused startup case recovers.

g++ -std=c++20 -Wall -Wextra viewport_capture.cpp \
  -o /tmp/mg-fsr3-viewport-review
/tmp/mg-fsr3-viewport-review
# Expected exit 1: both legitimate viewports are overwritten.
```

Use explicit expected exit codes when automating these reproductions. A shell
that stops on the first nonzero exit needs separate invocations.

For the actual host suite and the illustrative contract checks, use the
commands in [implementation-notes.md](../../implementation-notes.md#9-build-reproduce-and-hand-off).
The final suite was repeated because source changed during this review; the
new viewport reproduction fails even though the general suite passes.

The timing JSON is an arithmetic check, not new timing evidence. The captured
GPU timestamps did not establish disjoint/counter validity and omit work
outside their boundaries. Keep those limitations attached when citing it.
