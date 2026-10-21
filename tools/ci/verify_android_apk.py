#!/usr/bin/env python3
"""Verify FSR3 app controls, both ABI payloads and 16 KB ELF alignment in an APK."""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile


def verify(path):
    result = {"apk": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "libraries": {}}
    with zipfile.ZipFile(path) as archive:
        resources = archive.read("resources.arsc")
        # Check the packaged UI, not native strings: a valid core previously
        # shipped inside an older app that had no FSR3 switch or quality control.
        for value in ("Enable built-in FSR3", "FSR3 quality", "Native AA",
                      "Quality", "Balanced", "Performance", "Ultra performance"):
            if value.encode() not in resources and value.encode("utf-16-le") not in resources:
                raise ValueError(f"Missing FSR3 app resource: {value}")
        dex_files = [archive.read(name) for name in archive.namelist()
                     if name.startswith("classes") and name.endswith(".dex")]
        for key in (b"fsr3Setting", b"fsr3Quality"):
            if not any(key in dex for dex in dex_files):
                raise ValueError(f"Missing app config key: {key.decode()}")
        result["fsr3_app_controls"] = "present"
        for abi, machine in (("arm64-v8a", 183), ("x86_64", 62)):
            prefix = f"lib/{abi}/"
            core = prefix + "libmobileglues.so"
            if core not in archive.namelist():
                raise ValueError(f"Missing {core}")
            for name in archive.namelist():
                if not name.startswith(prefix) or not name.endswith(".so"):
                    continue
                data = archive.read(name)
                if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != machine:
                    raise ValueError(f"Wrong ELF class, endianness or machine: {name}")
                offset = struct.unpack_from("<Q", data, 32)[0]
                size, count = struct.unpack_from("<HH", data, 54)
                if size < 56 or offset + size * count > len(data):
                    raise ValueError(f"Invalid ELF program table: {name}")
                segments = []
                for index in range(count):
                    entry = offset + index * size
                    if struct.unpack_from("<I", data, entry)[0] != 1:
                        continue
                    file_offset, address = struct.unpack_from("<QQ", data, entry + 8)
                    alignment = struct.unpack_from("<Q", data, entry + 48)[0]
                    if alignment < 16384 or alignment & (alignment - 1) or file_offset % 16384 != address % 16384:
                        raise ValueError(f"ELF LOAD segment lacks 16 KB alignment: {name}")
                    segments.append(alignment)
                if not segments:
                    raise ValueError(f"No ELF LOAD segment: {name}")
                result["libraries"][name] = {"sha256": hashlib.sha256(data).hexdigest(), "load_alignment": segments}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apks", nargs="+", type=Path)
    args = parser.parse_args()
    print(json.dumps([verify(path) for path in args.apks], indent=2))


if __name__ == "__main__":
    main()
