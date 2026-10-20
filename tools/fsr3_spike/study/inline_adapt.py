#!/usr/bin/env python3
# Inline GL_GOOGLE_include_directive includes and adapt the pinned Vulkan GLSL
# to a candidate GLES 3.2 source. Compilation study only.
import re, sys, pathlib
roots = [pathlib.Path(p) for p in sys.argv[2:]]
seen = []
extensions = []
def resolve(name):
    for r in roots:
        p = r / name
        if p.is_file(): return p
    raise FileNotFoundError(name)
def expand(path):
    out = []
    for line in path.read_text().splitlines(True):
        m = re.match(r'\s*#include\s+"([^"]+)"', line)
        if m:
            out.append(expand(resolve(m.group(1))))
        else:
            out.append(line)
    return ''.join(out)
src = expand(pathlib.Path(sys.argv[1]))
# Vulkan descriptor sets do not exist in GLSL ES.
src = re.sub(r'\bset\s*=\s*0\s*,\s*', '', src)
# ES 3.2 has no R8/R16F/RG16F storage-image formats. Expand to a supported
# format; the used components and stored values are unchanged.
def fmt(m):
    q = m.group(2)
    q = {'r8':'rgba8','r16f':'rgba16f','rg16f':'rgba16f'}.get(q, q)
    return m.group(1) + q + m.group(3)
src = re.sub(r'(layout\s*\([^)]*?\b)(r8|r16f|rg16f)(\b[^)]*\))', fmt, src)
# App-controlled output image: declare a concrete ES format. The production
# backend must pick this from the declared output format; the study defaults
# to rgba8 and can be invoked with an explicit format argument.
out_fmt = sys.argv[4] if len(sys.argv) > 4 else 'rgba8'
src = re.sub(r'layout\s*\(\s*binding\s*=\s*([A-Za-z0-9_]+|\d+)\s*(?:/\*\s*app controlled format\s*\*/)?\s*\)\s*writeonly\s+uniform\s+(?:highp\s+)?image2D\s+rw_upscaled_output',
             lambda m: f'layout(binding = {m.group(1)}, {out_fmt}) writeonly uniform highp image2D rw_upscaled_output', src)
src = re.sub(r'^#extension GL_EXT_shader_image_load_formatted.*$', '', src, flags=re.M)

# ES 3.2 only allows r32f/r32i/r32ui as formatted read-write images. Split
# other read-write images into readonly + writeonly aliases on distinct image
# units (same texture at runtime). Values and bindings are otherwise unchanged.
def split_rw(src, name, fmt, bind_expr, alias_bind):
    decl = re.compile(r'layout\s*\(\s*binding\s*=\s*' + re.escape(bind_expr) + r'\s*,\s*' + fmt + r'\s*\)\s*(?:coherent\s+)?uniform\s+(?:highp\s+)?image2D\s+' + name + r'\s*;')
    m = decl.search(src)
    if not m:
        return src
    rep = (f'layout(binding = {bind_expr}, {fmt}) coherent readonly uniform highp image2D r_{name};\n'
           f'layout(binding = {alias_bind}, {fmt}) coherent writeonly uniform highp image2D {name};')
    src = src[:m.start()] + rep + src[m.end():]
    src = src.replace('imageLoad(' + name + ',', 'imageLoad(r_' + name + ',')
    return src

src = split_rw(src, 'rw_new_locks', 'rgba8', 'FSR3UPSCALER_BIND_UAV_NEW_LOCKS', 23)
src = split_rw(src, 'rw_frame_info', 'rgba32f', 'FSR3UPSCALER_BIND_UAV_FRAME_INFO', 22)
for i in range(6):
    src = split_rw(src, f'rw_spd_mip{i}', 'rgba16f', f'FSR3UPSCALER_BIND_UAV_SPD_MIPS_LEVEL_{i}', 24 + i)
src = src.replace('imageLoad(rw_spd_mip##idx', 'imageLoad(r_rw_spd_mip##idx')
# FSR3.1.4 GLSL declares the luma-history UAV as rgba8 although the host
# allocates R16G16B16A16_FLOAT (HLSL has no format). Match the allocation.
src = src.replace('binding = FSR3UPSCALER_BIND_UAV_LUMA_HISTORY, rgba8',
                  'binding = FSR3UPSCALER_BIND_UAV_LUMA_HISTORY, rgba16f')

sys.stdout.write(src)
