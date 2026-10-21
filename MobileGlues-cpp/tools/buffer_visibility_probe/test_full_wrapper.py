#!/usr/bin/env python3
"""Host-only guard checks; never creates EGL contexts or contacts a device."""

import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile

from run_full_wrapper import validate_result, EXPECTED_CASES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-probe", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mg-full-guard-") as directory:
        output = Path(directory) / "result.json"
        for config, may_load in [(None, False), ("/sdcard/MG", False),
                                 ("/data/local/tmp/", False),
                                 ("/data/local/tmp/a/../b", False),
                                 ("/data/local/tmp/mg-host-guard", True)]:
            env = dict(os.environ)
            env.pop("MG_DIR_PATH", None)
            if config is not None:
                env["MG_DIR_PATH"] = config
            completed = subprocess.run([str(args.host_probe.resolve()), "/nonexistent.so",
                                        str(output), "src-0000000000000000"],
                                       env=env, capture_output=True, text=True, timeout=10)
            result = json.loads(output.read_text())
            assert completed.returncode == 1 and result["pass"] is False
            assert result["os"] and result["backend_identity_available"] is False
            detail = result["results"][0]["detail"]
            if may_load:
                assert "/nonexistent.so" in detail and "MG_DIR_PATH" not in detail
            else:
                assert detail == "MG_DIR_PATH must select isolated /data/local/tmp directory"

    # These are result-validator fixtures, not simulated graphics results.
    marker = "src-0123456789abcdef"
    fixture = {key: "fixture" for key in ("os", "vendor", "renderer", "version",
                                        "backend_vendor", "backend_renderer", "backend_version")}
    fixture.update({"pass": True, "build_marker": marker, "backend_identity_available": True,
                    "egl_api_requested": "EGL_OPENGL_ES_API",
                    "egl_context_client_version_requested": 3,
                    "results": [{"name": name, "pass": True} for name in sorted(EXPECTED_CASES)]})
    assert validate_result(fixture, 0, marker)
    assert not validate_result(fixture, 1, marker)
    assert not validate_result(fixture, 0, "src-ffffffffffffffff")
    for key, bad in [("pass", False), ("backend_identity_available", False),
                     ("os", ""), ("egl_context_client_version_requested", 4),
                     ("results", []), ("results", "invalid")]:
        changed = copy.deepcopy(fixture); changed[key] = bad
        assert not validate_result(changed, 0, marker)
    changed = copy.deepcopy(fixture); changed["results"][0] = changed["results"][1]
    assert not validate_result(changed, 0, marker)
    changed = copy.deepcopy(fixture); changed["results"][0]["pass"] = False
    assert not validate_result(changed, 0, marker)
    assert not validate_result([], 0, marker)
    print("PASS: five host isolation checks and strict result-validator rejection fixtures; no graphics/device run")


if __name__ == "__main__":
    main()
