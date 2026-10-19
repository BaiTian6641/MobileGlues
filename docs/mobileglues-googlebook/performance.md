# Performance track

Status: first measured X1E microbenchmark pass, 2026-10-19. These are CPU-submission
microbenchmarks, not in-game FPS/power results.

## Target interpretation

The Snapdragon X1E80100 / Adreno X1-85 sits in the Radeon 780M class. The goal is not to beat a
desktop dGPU; it is to remove wrapper-induced CPU/GPU work while preserving standard GL behavior.
WSR 128 is the supported advanced target. WSR 256 remains blocked by the 128 MiB SSBO limit and is
future X2E work. Player reflections now render but are performance/power-unqualified.

## Implemented this pass

| Change | Why | Expected effect | Measurement |
|---|---|---|---|
| Skip the depth-guard scan when no sampler object is bound; skip units with no sampler before texture lookup; remove the per-draw 1.5 KB `Moved` zero-fill | The guard runs on every draw/dispatch; Iris binds samplers on many passes, but compute and sampler-free phases paid a 128-unit scan and memset for no work | Lower CPU per draw/dispatch; exact, no visual change | X1E, 20k dispatches, best of 3 after warmup: plain **780.4 → 588.6 ns** (−24.6%); sampler+depth guard **862.6 → 713.0 ns** (−17.3%). `validation/2026-10-19-x1e-perf-guard-{before,after}.json` |
| Per-physical-unit image-binding shadow; skip identical `glBindImageTexture` calls from `glBindImageTexture`, `mg_materialize_image_bindings` and program switches | Each program switch and image-uniform write replayed every image; reflection scenes have several live images and frequent switches | Removes redundant ANGLE/Vulkan descriptor binds; no deferral, so ordering is unchanged | X1E, 100k identical binds: **54.74 → 23.80 ns/bind** (−56.5%). `validation/2026-10-19-x1e-perf-image-rebind-{before,after}.json` |
| Probe backend `glGetError` only when a format was lowered or is unknown | The common R32UI/RGBA image bind paid a backend error round trip per call | Removes one round trip from the common image-bind path | Included in the image-rebind measurement; not separately isolated |
| Fast path for `glUniform1i`/`glUniform1iv` when the current program has no image uniforms; per-program `has_image_uniforms` flag | Sampler/scalar writes were doing share-group and location hash lookups for a feature the program does not use | Removes two hash probes from the common uniform path | Not separately measured yet; `perfetto`/`simpleperf` pending |
| 3D image clear: level-0 recorded extents, four x-spans per invocation, dispatch groups ÷4 | Iris clears voxel/WSR volumes every frame; the old path issued one invocation per voxel and three driver queries per clear | Fewer invocations/groups and no bind/query on the dominant level-0 path | X1E 128×64×128 R32UI: **151.1 → 88.9 µs** per clear (−41.1%); `validation/2026-10-19-x1e-perf-clear-{before,after}.json`; 128×64×128 canary clears last x voxel (`...-clear-volume.json`) |

Source review: `PerformanceAudit` agent, 2026-10-19. The audit ranked image replay first, the
unconditional image-bind error probe second, and the uniform hash path third. It explicitly marked
the shadow-pass voxelization and player raytrace as inherent pack GPU work; no quality reduction is
proposed.

## Next candidates (ranked, not yet implemented)

1. Measure the shadow pass with `GL_EXT_disjoint_timer_query`/`GL_AMD_performance_monitor`, with
   `WORLD_SPACE_REFLECTIONS` 0 vs 1, to separate wrapper CPU from pack GPU work.
2. Batch image-binding materialization once per draw/dispatch instead of replaying on each image
   uniform write. Only after the dedupe numbers show remaining duplicate work.
3. 3D image clear: use recorded extents, clear several voxels per invocation, and replace the 2D
   image-unit scan with tracked state. Measure with a 128×64×128 R32UI clear timer.
4. Redundant `glBindVertexArray`/`glBindBuffer` elision after making the depth-clear triangle restore
   the application's VAO and `GL_ARRAY_BUFFER`.

## Measurement procedure

```sh
# Wrapper CPU microbenchmarks
adb -s C1L63708PZ push build/android-arm64-release/libmobileglues.so /data/local/tmp/
adb -s C1L63708PZ push MobileGlues-cpp/tools/wrapper_probe/wrapper_probe_arm64 /data/local/tmp/
adb -s C1L63708PZ shell 'cd /data/local/tmp && MG_DIR_PATH=/data/local/tmp/mgconfig \
  MG_PROBE_OUTPUT=/data/local/tmp/guard.json MG_LIBRARY=/data/local/tmp/libmobileglues.so \
  LD_LIBRARY_PATH=/data/local/tmp ./wrapper_probe_arm64 guard_bench'
adb -s C1L63708PZ shell 'cd /data/local/tmp && MG_DIR_PATH=/data/local/tmp/mgconfig \
  MG_PROBE_OUTPUT=/data/local/tmp/rebind.json MG_LIBRARY=/data/local/tmp/libmobileglues.so \
  LD_LIBRARY_PATH=/data/local/tmp ./wrapper_probe_arm64 image_rebind_bench'
```

In-game A/B: same world, camera route, resolution, pack profile and warmup; capture F3 frame time
and logcat. Correctness suite on the clear-optimized build is **56/56**
(`validation/2026-10-19-x1e-perf-all-56.json`). In-game FPS, power and thermal results remain
pending; these microbenchmarks do not establish a playable profile.
