#!/usr/bin/env python3
"""Build the complete-DSO lane without changing the provider probe.

The generated include reuses the provider probe's actual shader, map, fence,
readback and invalid-flag bodies. Only shader dialect and API dispatch change.
This script never contacts a device or executes a graphics test.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


SOURCE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def shared_cases(source):
    text = source.read_text()
    start = "GLuint shader(GLenum stage, const char* source) {"
    end = "\n}\n\nint main() {"
    if text.count(start) != 1 or text.count(end) != 1:
        raise ValueError("Provider case boundaries changed; review extraction before building")
    body = text[text.index(start):text.index(end)]
    if body.count("#version 310 es\\n") != 2 or body.count("precision highp float;\\n") != 1:
        raise ValueError("Provider shader dialect changed; review full-wrapper shader adaptation")
    body = body.replace("#version 310 es\\n", "#version 330 core\\n")
    body = body.replace("precision highp float;\\n", "")
    # Every GL call must compile against the explicit DSO table. A new provider
    # call with no matching table member fails the strict C++ build.
    body = re.sub(r"\b(gl[A-Z]\w*)\(", r"full.\1(", body)
    return ("// Generated shared cases; provider main.cpp SHA-256: " + digest(source)
            + "\n" + body + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--target", choices=["android-arm64", "host"], default="android-arm64")
    parser.add_argument("--ndk", type=Path, default=Path(os.environ.get(
        "ANDROID_NDK", str(Path.home() / "Android/Sdk/ndk/28.2.13676358"))))
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output == SOURCE or SOURCE in output.parents:
        parser.error("Generated output must be outside the probe source directory")
    output.mkdir(parents=True, exist_ok=True)
    include = output / "full_wrapper_cases.inc"
    include.write_text(shared_cases(SOURCE / "main.cpp"))
    compiler = (str(args.ndk / "toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android26-clang++")
                if args.target == "android-arm64" else os.environ.get("CXX", "c++"))
    binary = output / "buffer-full-wrapper"
    command = [compiler, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror",
               "-I" + str(SOURCE.parents[1] / "include"), "-I" + str(output),
               str(SOURCE / "full_wrapper.cpp"), "-o", str(binary), "-ldl"]
    if args.target == "android-arm64":
        command[1:1] = ["-fPIE", "-pie", "-static-libstdc++"]
    subprocess.run(command, check=True)
    # This file is used only in a fresh temporary device directory selected by
    # MG_DIR_PATH. It does not change launcher/shared MobileGlues settings.
    config = output / "config.json"
    config.write_text(json.dumps({"enableNoError": 1, "enableANGLE": 0,
                                 "fsr1Setting": 0, "fsr3Setting": 0,
                                 "hideMGEnvLevel": 0, "maxGlslCacheSize": 0}, indent=2) + "\n")
    manifest = {
        "lane": "full MobileGlues DSO",
        "target": args.target,
        "build_command": command,
        "sources": {name: digest(SOURCE / name) for name in
                    ("main.cpp", "api.h", "full_wrapper.cpp", "build_full_wrapper.py")},
        "artifacts": {path.name: digest(path) for path in (binary, config, include)},
        "egl_api_requested": "EGL_OPENGL_ES_API",
        "egl_context_client_version_requested": 3,
        "shader_dialect": "GLSL 330 core",
        "resolution": "same-DSO dlsym plus dladdr validation; no provider fallback or EGL/GLES link",
        "cases": ["persistent_explicit_0x72", "nonpersistent_explicit", "persistent_coherent", "invalid_flags", "invalid_flush_range", "dsa_persistent_explicit_0x72"],
        "runtime": "not run by this build",
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps({"binary": str(binary), "sha256": digest(binary), "target": args.target}))


if __name__ == "__main__":
    main()
