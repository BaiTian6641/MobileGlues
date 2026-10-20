# X1E FSR3 backend multi-frame probe

Status: **PASS (three-frame reset/history/reset smoke; deterministic run JSON).**

- Device: HP Googlebook 14 `C1L63708PZ`, X1E80100 / Adreno X1-85.
- Provider: ANGLE Vulkan 1.4.295 over Adreno driver 512.886.6, GLES 3.2.
- Probe: `tools/fsr3_spike/backend_probe/backend_probe_arm64`.
- Core: `/data/local/tmp/libmobileglues_probe.so`, SHA-256
  `83372740ffceb7f2b0c95cc7daa42b2e9c73b8a02c393b42df40d8a3364c6a14`.
- Raw JSON: `2026-10-20-x1e-backend-multiframe.json`.
- JSON SHA-256: `cf71d4b5abb9b550ad208ccc4533844f6eb1ec563f749042933e862f4ff527cd`.
- Two independent probe runs produced byte-identical JSON.

Checks all PASS:

```text
context
caps
dispatch1
output
output_fbo
flat_output
gl_error
dispatch2_history
flat_output2
dispatch3_reset
```

Limits: the probe covers a 160x90 render / 320x180 output flat-frame
reset→history→reset sequence with synthetic depth/motion. It does not prove
non-flat temporal quality, per-pass timings, independent motion, or the Minecraft
adapter path. Per-pass cost remains NOT RUN.
