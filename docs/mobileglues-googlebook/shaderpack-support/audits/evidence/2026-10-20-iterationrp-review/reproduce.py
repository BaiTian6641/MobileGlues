#!/usr/bin/env python3
"""Offline evidence for the iterationRP review. No device or product writes.

Requires Python, NumPy, Pillow and g++. The replay probe extracts the current
C++ function verbatim, compiles it against small state/driver doubles in a
temporary directory, and reports calls. It does not exercise EGL or a GPU.
Output goes to stdout; redirect it to a NEW evidence file when rerunning.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

import numpy as np
from PIL import Image


def sha(data):
    return hashlib.sha256(data).hexdigest()


def replay_probe(repo):
    path = repo / "MobileGlues-cpp/gl/drawing.cpp"
    source = path.read_text()
    start = source.index("void mg_materialize_image_bindings() {")
    # The function is at file scope; its next unindented closing brace ends it.
    end = source.index("\n}", start) + 2
    function = source[start:end]
    prefix = r'''
#include <array>
#include <iostream>
#include <unordered_map>
using GLuint = unsigned;
using GLint = int;
using GLenum = unsigned;
using GLboolean = bool;
constexpr GLuint MG_TRACKED_IMAGE_UNITS = 32;
struct mg_image_binding_t {
    GLuint texture = 0; GLint level = 0; GLboolean layered = false;
    GLint layer = 0; GLenum access = 0; GLenum format = 0;
    bool operator==(const mg_image_binding_t&) const = default;
};
struct MgProgramObject {
    std::unordered_map<GLint, GLuint> image_uniform_units;
    std::unordered_map<GLuint, GLuint> image_unit_remap;
} program;
struct State {
    GLuint current_program = 1;
    std::array<mg_image_binding_t, 32> image_bindings{};
    std::array<mg_image_binding_t, 32> image_unit_shadow{};
    std::array<bool, 32> image_unit_shadow_valid{};
} state;
State* gl_state = &state;
std::array<GLuint, 32> backend{};
struct Driver {
    void glBindImageTexture(GLuint unit, GLuint texture, GLint, GLboolean,
                            GLint, GLenum, GLenum) {
        backend[unit] = texture;
        std::cout << "bind " << unit << " " << texture << "\n";
    }
} GLES;
MgProgramObject* mg_find_program(GLuint) { return &program; }
GLenum image_unit_bind_format(GLenum f, GLuint) { return f; }
void mg_lower_2d_image_layering(GLuint, GLboolean&, GLint&) {}
bool image_binding_equal(const mg_image_binding_t& a,
                         const mg_image_binding_t& b) { return a == b; }
'''
    suffix = r'''
int main() {
    program.image_uniform_units[7] = 2;
    program.image_unit_remap[0] = 2;
    state.image_bindings[0].texture = 101;
    state.image_bindings[2].texture = 202; // inactive binding from another pass
    mg_materialize_image_bindings();
    std::cout << "active_unit=2 expected_texture=101 actual_texture="
              << backend[2] << "\n";
    // Pinned Iris normally rebinds active images after selecting the program.
    GLES.glBindImageTexture(2, 101, 0, false, 0, 0, 0);
    std::cout << "after_active_rebind=" << backend[2] << "\n";
}
'''
    with tempfile.TemporaryDirectory(prefix="mg-itrp-replay-") as tmp:
        cpp, exe = Path(tmp) / "probe.cpp", Path(tmp) / "probe"
        cpp.write_text(prefix + function + suffix)
        subprocess.run(["g++", "-std=c++20", "-O0", str(cpp), "-o", str(exe)], check=True)
        output = subprocess.check_output([str(exe)], text=True)
    return {
        "source": str(path.relative_to(repo)), "source_sha256": sha(path.read_bytes()),
        "function_sha256": sha(function.encode()),
        "evidence_level": "actual function with host state/driver doubles; not GPU execution",
        "stdout": output,
        "collision_reproduced": "expected_texture=101 actual_texture=202" in output,
        "explicit_active_rebind_restores": "after_active_rebind=101" in output,
    }


def geometry_probe(repo, pack):
    """Bounded compiler inspection, not the full Iris/Sodium runtime pipeline."""
    source_path = repo / "MobileGlues-cpp/gl/glsl/glsl_for_es.cpp"
    source = source_path.read_text()
    start = source.index("static std::string lower_const_transpose(")
    end = source.index("\n}", start) + 2
    lower = source[start:end]
    with zipfile.ZipFile(pack) as archive:
        def inline(name, stack=()):
            if name in stack:
                raise ValueError("recursive include: " + name)
            return re.sub(r'^\s*#include\s+"([^"]+)"\s*$',
                          lambda m: inline("shaders/" + m[1].lstrip("/"), stack + (name,)),
                          archive.read(name).decode("utf-8-sig"), flags=re.M)
        geom = inline("shaders/gbuffers_terrain.gsh")
    geom = geom.replace("#version 430 compatibility",
                        "#version 430 core\n#define IS_IRIS\n#define MC_VERSION 12101")
    cpp = ("#include <regex>\n#include <string>\n#include <iostream>\n#include <iterator>\n" + lower +
           "\nint main(){std::string s((std::istreambuf_iterator<char>(std::cin)),{});"
           "std::cout<<lower_const_transpose(s);}\n")
    with tempfile.TemporaryDirectory(prefix="mg-itrp-geom-") as tmp:
        tmp = Path(tmp)
        (tmp / "lower.cpp").write_text(cpp)
        subprocess.run(["g++", "-std=c++20", str(tmp / "lower.cpp"), "-o", str(tmp / "lower")], check=True)
        lowered = subprocess.check_output([str(tmp / "lower")], input=geom, text=True)
        (tmp / "terrain.geom").write_text(lowered)
        compiled = subprocess.run(["glslangValidator", "-G", "--auto-map-bindings", "--auto-map-locations",
                                   "-S", "geom", "-o", str(tmp / "terrain.spv"), str(tmp / "terrain.geom")],
                                  capture_output=True, text=True)
        result = {"compile_exit": compiled.returncode,
                  "preparation": "ZIP include expansion; 430 compatibility to core; IS_IRIS and MC_VERSION=12101; actual lower_const_transpose function",
                  "compiler": subprocess.check_output(["glslangValidator", "--version"], text=True).splitlines()[0],
                  "limitation": "standalone desktop-SPIR-V inspection; not actual runtime Iris/Sodium preprocessing, MobileGlues ESSL or target GPU",
                  "lower_function_sha256": sha(lower.encode()), "input_sha256": sha(lowered.encode())}
        if compiled.returncode:
            result["diagnostic"] = compiled.stdout + compiled.stderr
            return result
        asm = subprocess.check_output(["spirv-dis", str(tmp / "terrain.spv")], text=True)
        before, after = asm.split("OpLoopMerge", 1)
        result.update({"spirv_sha256": sha((tmp / "terrain.spv").read_bytes()),
                       "texture_resolution_stores_before_loop": len(re.findall(r"OpStore %v_textureResolution\b", before)),
                       "texture_resolution_stores_after_loop_header": len(re.findall(r"OpStore %v_textureResolution\b", after)),
                       "emit_vertex_in_loop": "OpEmitVertex" in after})
        return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[6])
    parser.add_argument("--pack", type=Path)
    parser.add_argument("--capture", action="append", type=Path, default=[])
    parser.add_argument("--geometry", action="store_true", help="also requires glslangValidator and spirv-dis")
    args = parser.parse_args()
    repo = args.repo.resolve()
    pack = args.pack or repo / "references/iterationRP Alpha 0.8.31.zip"
    report = {
        "scope": "offline source, CPU asset and saved-capture review; no device interaction",
        "head": subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip(),
        "pack": {"path": str(pack), "sha256": sha(pack.read_bytes())},
    }
    member = "shaders/texture/AtmoData_620_540_440_n4_16f.bin"
    with zipfile.ZipFile(pack) as archive:
        data = archive.read(member)
        report["source_members"] = {
            name: sha(archive.read(name)) for name in [
                "shaders/Lib/Programs/Gbuffers/Terrain_GS.glsl",
                "shaders/Lib/Programs/Gbuffers/Terrain_FS.glsl",
                "shaders/Lib/Programs/Composite/Exposure_CS.glsl",
                "shaders/Lib/Programs/Composite/Sky_Overworld_FS.glsl",
                "shaders/Lib/Programs/Composite/Image_Clear_Screen_CS.glsl",
                "shaders/Lib/Programs/Final_FS.glsl",
                "shaders/Lib/BasicFunctions/PrecomputedAtmosphere.glsl",
            ]
        }
    lut = np.frombuffer(data, dtype="<f2").astype(np.float32).reshape(33, 128, 512, 4)
    coords = [(0, 0, 0), (511, 127, 31), (0, 0, 32), (128, 32, 32), (32, 72, 32), (63, 79, 32)]
    report["lut"] = {
        "member": member, "sha256": sha(data), "bytes": len(data),
        "layout": "little-endian binary16 RGBA; x fastest, then y, then z",
        "components": int(lut.size), "finite": int(np.isfinite(lut).sum()),
        "negative": int((lut < 0).sum()), "nonzero": int(np.count_nonzero(lut)),
        "minimum": float(lut.min()), "maximum": float(lut.max()),
        "samples": [{"xyz": list(p), "rgba": lut[p[2], p[1], p[0]].tolist(),
                     "bytes_hex": data[((p[2]*128+p[1])*512+p[0])*8:((p[2]*128+p[1])*512+p[0])*8+8].hex()}
                    for p in coords],
    }
    report["manual_exposure_default_ev14"] = {
        "calculated": 150000 * 2**-14,
        "binary16_roundtrip": struct.unpack("<e", struct.pack("<e", 150000 * 2**-14))[0],
    }
    n = np.arange(65536, dtype=np.uint32)
    packed = n.astype(np.float32) / np.float32(65535)
    report["packed16_cpu_roundtrip"] = {
        "values": len(n),
        "float32_mismatches": int(((packed*np.float32(65535)).astype(np.uint32) != n).sum()),
        "float16_mismatches": int(((packed.astype(np.float16).astype(np.float32)*np.float32(65535)).astype(np.uint32) != n).sum()),
        "limitation": "CPU IEEE arithmetic only; no sampler, raster or translated-shader proof",
    }
    report["image_replay"] = replay_probe(repo)
    if args.geometry:
        report["geometry"] = geometry_probe(repo, pack)
    report["captures"] = []
    for path in args.capture:
        image = Image.open(path).convert("RGB")
        regions = []
        for label, box in [("terrain", (800, 1300, 1300, 1600)), ("sky", (650, 350, 1300, 650))]:
            pixels = np.asarray(image.crop(box)).reshape(-1, 3)
            color, count = Counter(map(tuple, pixels.tolist())).most_common(1)[0]
            regions.append({"label": label, "box_xyxy": box, "mode_rgb": color,
                            "mode_count": count, "pixels": len(pixels), "mode_fraction": count/len(pixels)})
        report["captures"].append({"path": str(path), "sha256": sha(path.read_bytes()),
                                   "dimensions": image.size, "regions": regions})
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
