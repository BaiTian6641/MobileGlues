#!/usr/bin/env python3
"""Prepare or explicitly execute a hash-bound offscreen Android full-DSO probe.

Without --execute this performs local validation only. Execution requires an
explicit serial and writes only to a new directory under /data/local/tmp.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
import uuid


EXPECTED_CASES = {"persistent_explicit_0x72", "nonpersistent_explicit",
                  "persistent_coherent", "invalid_flags", "invalid_flush_range"}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_result(result, exit_code, marker):
    if not isinstance(result, dict):
        return False
    cases = result.get("results", [])
    if not isinstance(cases, list):
        return False
    return (exit_code == 0 and result.get("pass") is True
            and len(cases) == len(EXPECTED_CASES)
            and all(isinstance(case, dict) and case.get("pass") is True for case in cases)
            and {case.get("name") for case in cases} == EXPECTED_CASES
            and result.get("build_marker") == marker
            and result.get("egl_api_requested") == "EGL_OPENGL_ES_API"
            and result.get("egl_context_client_version_requested") == 3
            and result.get("backend_identity_available") is True
            and all(isinstance(result.get(key), str) and result[key] for key in
                    ("os", "vendor", "renderer", "version", "backend_vendor",
                     "backend_renderer", "backend_version")))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--probe-dir", type=Path, required=True)
    parser.add_argument("--expected-core-sha256", required=True)
    parser.add_argument("--expected-marker", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-f0-9]{64}", args.expected_core_sha256):
        parser.error("Expected core SHA-256 must be 64 lowercase hex digits")
    if not re.fullmatch(r"src-[a-f0-9]{16,64}", args.expected_marker):
        parser.error("Expected marker must be the exact src- hexadecimal build marker")
    if args.output.exists():
        parser.error("Output directory must be new; existing evidence is not overwritten")
    manifest_path = args.probe_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    if manifest.get("target") != "android-arm64":
        parser.error("Device execution requires an Android ARM64 build")
    binary = args.probe_dir / "buffer-full-wrapper"
    config = args.probe_dir / "config.json"
    for path in (binary, config):
        if digest(path) != manifest["artifacts"][path.name]:
            parser.error("Probe artifact differs from its build manifest: " + path.name)
    core_hash = digest(args.library)
    if core_hash != args.expected_core_sha256:
        parser.error("Core differs from the explicitly expected SHA-256")
    files = {"libmobileglues.so": args.library, "buffer-full-wrapper": binary,
             "config.json": config}
    hashes = {name: digest(path) for name, path in files.items()}
    plan = {"level": "android-full-wrapper-readback", "core_sha256": core_hash,
            "probe_sha256": hashes["buffer-full-wrapper"],
            "config_sha256": hashes["config.json"], "expected_marker": args.expected_marker,
            "build_manifest_sha256": digest(manifest_path), "executed": False}
    if not args.execute:
        print(json.dumps(plan, indent=2))
        return

    args.output.mkdir(parents=True, exist_ok=False)
    (args.output / "harness-manifest.json").write_bytes(manifest_path.read_bytes())
    remote = ("/data/local/tmp/mg-buffer-" + core_hash[:12] + "-"
              + hashes["buffer-full-wrapper"][:12] + "-" + uuid.uuid4().hex[:8])
    adb = ["adb", "-s", args.serial]

    def adb_run(arguments, *, check=True, timeout=75):
        return subprocess.run(adb + arguments, check=check, capture_output=True,
                              text=True, timeout=timeout)

    # No -p: an existing remote directory is never reused or overwritten.
    adb_run(["shell", "mkdir", remote])
    for name, path in files.items():
        adb_run(["push", str(path), remote + "/" + name])
    verified = adb_run(["shell", "sha256sum", *[remote + "/" + n for n in files]])
    actual = {Path(line.split()[1]).name: line.split()[0]
              for line in verified.stdout.splitlines()}
    (args.output / "remote-hashes.txt").write_text(verified.stdout)
    if actual != hashes:
        raise RuntimeError("Remote core/probe/config hashes do not match local manifest")
    adb_run(["shell", "chmod", "755", remote + "/buffer-full-wrapper"])
    command = ("cd " + shlex.quote(remote) + " && timeout 60 env MG_DIR_PATH="
               + shlex.quote(remote) + " ./buffer-full-wrapper "
               + shlex.quote(remote + "/libmobileglues.so") + " "
               + shlex.quote(remote + "/result.json") + " "
               + shlex.quote(args.expected_marker))
    try:
        completed = adb_run(["shell", command], check=False)
    except subprocess.TimeoutExpired as error:
        def captured(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else (value or "")
        (args.output / "stdout.log").write_text(captured(error.stdout))
        (args.output / "stderr.log").write_text(captured(error.stderr))
        plan.update({"executed": True, "pass": False,
                     "failure": "ADB execution exceeded outer75-second bound; no successful run inferred"})
        (args.output / "evidence.json").write_text(json.dumps(plan, indent=2) + "\n")
        raise SystemExit(1) from error
    (args.output / "stdout.log").write_text(completed.stdout)
    (args.output / "stderr.log").write_text(completed.stderr)
    (args.output / "exit-code.txt").write_text(str(completed.returncode) + "\n")
    raw = adb_run(["exec-out", "cat", remote + "/result.json"], check=False)
    (args.output / "result.json").write_text(raw.stdout)
    plan.update({"executed": True, "exit_code": completed.returncode,
                 "result_sha256": digest(args.output / "result.json"), "pass": False})
    try:
        result = json.loads(raw.stdout)
        plan["pass"] = raw.returncode == 0 and validate_result(result, completed.returncode, args.expected_marker)
    except (ValueError, TypeError, AttributeError):
        plan["failure"] = "Missing or invalid result JSON; no successful run inferred"
    (args.output / "evidence.json").write_text(json.dumps(plan, indent=2) + "\n")
    print(json.dumps(plan, indent=2))
    raise SystemExit(0 if plan["pass"] else 1)


if __name__ == "__main__":
    main()
