#!/usr/bin/env python3
"""Compile owned translation units, parse real Clang ASTs, and run lint checks.

The default host check is intentionally small. An optional Android compile
database adds the native FSR/lifecycle files and clang-tidy with an explicit
diagnostic baseline. Neither mode establishes device behavior.
"""

import argparse
import collections
import hashlib
import json
import pathlib
import re
import shlex
import shutil
import subprocess
import sys


ROOT = pathlib.Path(__file__).resolve().parents[2]
CPP = ROOT / "MobileGlues-cpp"
HOST_SOURCES = ("gl/shader_source.cpp", "gl/image_metadata.cpp", "gl/glsl/compat_image.cpp")
NATIVE_SOURCES = HOST_SOURCES + (
    "upscaling/mg_fsr.cpp", "upscaling/mg_fsr_jni.cpp", "gl/FSR3Direct.cpp", "egl/context.cpp"
)
TIDY_CHECKS = "-*,clang-analyzer-*,bugprone-*,cert-*,performance-*,readability-misleading-indentation"


def execute(command, output, cwd=ROOT):
    result = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True, check=False)
    output.with_suffix(".command.json").write_text(json.dumps(command, indent=2) + "\n")
    output.write_text(result.stdout)
    output.with_suffix(".stderr.log").write_text(result.stderr)
    if result.returncode:
        raise RuntimeError(f"{command[0]} exited {result.returncode}; see {output}")
    return result


def ast_summary(text):
    """Clang emits one JSON object per name-filtered declaration."""
    decoder = json.JSONDecoder()
    nodes = collections.Counter()
    forbidden = set()
    roots = 0

    def walk(node):
        if not isinstance(node, dict):
            return
        nodes[node.get("kind", "unknown")] += 1
        if node.get("kind") == "DeclRefExpr":
            name = node.get("referencedDecl", {}).get("name")
            if name in {"system", "popen", "execl", "execv", "execve"}:
                forbidden.add(name)
        for child in node.get("inner", []):
            walk(child)

    offset = 0
    while offset < len(text):
        while offset < len(text) and text[offset].isspace():
            offset += 1
        if offset == len(text):
            break
        node, offset = decoder.raw_decode(text, offset)
        walk(node)
        roots += 1
    if roots == 0:
        raise RuntimeError("Clang produced no AST declarations")
    if forbidden:
        raise RuntimeError(f"Unexpected process execution references: {sorted(forbidden)}")
    return {"scope": "Clang declarations matching mg_ (complete translation unit syntax checked)",
            "root_declarations": roots, "node_kinds": dict(nodes)}


def clean_compile_command(entry):
    original = entry.get("arguments") or shlex.split(entry["command"])
    cleaned = []
    skip = False
    for argument in original:
        if skip:
            skip = False
        elif argument == "-o":
            skip = True
        elif argument not in {"-c", "-w", "-flto=thin"}:
            cleaned.append(argument)
    return cleaned


def source_digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def host_checks(args, output):
    compiler = shutil.which(args.compiler)
    if not compiler:
        raise RuntimeError(f"Clang C++ compiler not found: {args.compiler}")
    version = subprocess.check_output([compiler, "--version"], text=True)
    if "clang" not in version.lower():
        raise RuntimeError("AST checks require Clang, not a compiler that ignores Clang flags")
    (output / "compiler.txt").write_text(version)
    flags = [compiler, "-std=gnu++20", f"-I{CPP}", f"-I{CPP / 'includes'}",
             f"-I{CPP / 'include'}", "-Wall", "-Wextra", "-Werror"]
    if args.target:
        flags += [f"--target={args.target}"]
    result = []
    for relative in HOST_SOURCES:
        source = CPP / relative
        log = output / (source.stem + ".ast.json")
        command = flags + ["-fsyntax-only", "-Xclang", "-ast-dump=json",
                           "-Xclang", "-ast-dump-filter=mg_", str(source)]
        parsed = execute(command, log)
        result.append({"file": str(source.relative_to(ROOT)), "sha256": source_digest(source),
                       "lint": "zero compiler warnings", "ast": ast_summary(parsed.stdout)})
    # Link the real session API to a recording backend to test the teardown
    # ownership seam. Section GC removes unrelated rendering functions.
    executable = output / "fsr_lifecycle_test"
    execute(flags + ["-Wno-unused-parameter", "-ffunction-sections", "-fdata-sections",
                     "-Wl,--gc-sections", str(CPP / "tests/fsr_lifecycle_test.cpp"),
                     str(CPP / "upscaling/mg_fsr.cpp"), "-o", str(executable)],
            output / "fsr-lifecycle-build.log")
    execute([str(executable)], output / "fsr-lifecycle.log")
    return result


def native_checks(args, output):
    database = pathlib.Path(args.compile_db).resolve()
    entries = {pathlib.Path(e["file"]).resolve(): e for e in json.loads(database.read_text())}
    baseline = json.loads((pathlib.Path(__file__).parent / "native-baseline.json").read_text())
    results = []
    for relative in NATIVE_SOURCES:
        source = CPP / relative
        entry = entries.get(source)
        if not entry:
            raise RuntimeError(f"Compile database has no entry for {source}")
        command = clean_compile_command(entry)
        name = relative.replace("/", "_")
        # Parse the complete Android translation unit, including JNI guarded by
        # __ANDROID__, using its actual build target and generated include paths.
        execute(command + ["-fsyntax-only"], output / (name + ".parse.log"), entry["directory"])
        tidy = pathlib.Path(command[0]).with_name("clang-tidy")
        if not tidy.is_file():
            found = shutil.which("clang-tidy")
            if not found:
                raise RuntimeError("clang-tidy is required with --compile-db")
            tidy = pathlib.Path(found)
        audit = execute([str(tidy), str(source), f"-p={database.parent}",
                         f"--checks={TIDY_CHECKS}", "--quiet"], output / (name + ".tidy.log"))
        counts = collections.Counter(re.findall(r"warning: .*?\[([^\]]+)\]", audit.stdout))
        allowed = baseline.get(relative, {})
        excess = {name: count for name, count in counts.items() if count > allowed.get(name, 0)}
        results.append({"file": str(source.relative_to(ROOT)), "sha256": source_digest(source),
                        "diagnostics": dict(counts), "allowed_existing_diagnostics": allowed,
                        "new_diagnostics": excess})
        if excess:
            (output / "native-partial-summary.json").write_text(json.dumps(results, indent=2) + "\n")
            raise RuntimeError(f"New native diagnostics in {relative}: {excess}")
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("--target", help="Optional host target, e.g. x86_64-linux-gnu for NDK Clang")
    parser.add_argument("--compile-db", help="Optional Android compile_commands.json for native audit")
    parser.add_argument("--output", default="build/quality")
    args = parser.parse_args()
    output = pathlib.Path(args.output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    try:
        summary = {"host": host_checks(args, output), "evidence_level": "host/static only"}
        if args.compile_db:
            summary["native"] = native_checks(args, output)
        (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"quality audit failed: {error}", file=sys.stderr)
        return 1
    print(f"quality audit passed for scoped files; evidence: {output / 'summary.json'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
