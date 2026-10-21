#!/usr/bin/env python3
"""Run bounded wrapper tests on an explicitly selected Android device.

Only writes to a hash-named directory under /data/local/tmp. Each evidence file
binds OS/provider observations to the exact core and harness hashes. This does
not certify a Minecraft process or modify launcher data/settings.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("cases", nargs="+")
    args = parser.parse_args()
    if any(not re.fullmatch(r"[a-z0-9_]+", case) for case in args.cases):
        parser.error("Probe cases must contain only lowercase letters, digits and underscores")
    adb = ["adb", "-s", args.serial]

    def run(arguments, **kwargs):
        return subprocess.run(adb + arguments, timeout=120, **kwargs)

    def property_value(name):
        return run(["shell", "getprop", name], check=True, capture_output=True, text=True).stdout.strip()

    identity = {
        "serial": args.serial,
        "fingerprint": property_value("ro.build.fingerprint"),
        "product": property_value("ro.product.model"),
        "soc": property_value("ro.soc.model"),
        "egl_property": property_value("ro.hardware.egl"),
        "abi": property_value("ro.product.cpu.abi"),
        "page_size": run(["shell", "getconf", "PAGESIZE"], check=True, capture_output=True, text=True).stdout.strip(),
    }
    library_hash = digest(args.library)
    probe_hash = digest(args.probe)
    remote = f"/data/local/tmp/mg-qualification-{library_hash[:16]}-{probe_hash[:8]}"
    run(["shell", "mkdir", "-p", remote], check=True)
    run(["push", str(args.library), remote + "/libmobileglues.so"], check=True)
    run(["push", str(args.probe), remote + "/wrapper_probe"], check=True)
    run(["shell", "chmod", "755", remote + "/wrapper_probe"], check=True)
    args.output.mkdir(parents=True, exist_ok=True)
    passed = True
    for case in args.cases:
        result_path = remote + "/" + case + ".json"
        command = (f"cd {shlex.quote(remote)} && "
                   f"MG_LIBRARY={shlex.quote(remote + '/libmobileglues.so')} "
                   f"MG_PROBE_OUTPUT={shlex.quote(result_path)} ./wrapper_probe {shlex.quote(case)}")
        result = run(["shell", command], capture_output=True, text=True)
        (args.output / f"{case}.log").write_text(result.stdout + result.stderr)
        raw = run(["exec-out", "cat", result_path], check=True, capture_output=True, text=True)
        evidence = {
            "level": "android-wrapper-readback",
            "device": identity,
            "core_sha256": library_hash,
            "probe_sha256": probe_hash,
            "case": case,
            "exit_code": result.returncode,
            "results": json.loads(raw.stdout),
        }
        (args.output / f"{case}.json").write_text(json.dumps(evidence, indent=2) + "\n")
        checks = [value["pass"] for value in evidence["results"].values()
                  if isinstance(value, dict) and isinstance(value.get("pass"), bool)]
        case_passed = result.returncode == 0 and bool(checks) and all(checks)
        passed = passed and case_passed
        print(f"{case}: {'PASS' if case_passed else 'FAIL'} ({len(checks)} checks)")
    raise SystemExit(0 if passed else 1)


if __name__ == "__main__":
    main()
