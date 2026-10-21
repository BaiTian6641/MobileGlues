// MobileGlues - upscaling/fsr3_input_shader.h
// Copyright (c) 2025-2026 MobileGL-Dev
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

namespace mg_fsr3_depth {

inline constexpr const char* kInputShader = R"(#version 320 es
layout(local_size_x = 8, local_size_y = 8) in;
layout(rgba16f, binding = 0) uniform writeonly highp image2D uMotion;
layout(r32f, binding = 1) uniform writeonly highp image2D uDepthOut;
layout(binding = 0) uniform highp sampler2D uDepth;
uniform highp mat4 uInvVP;
uniform highp mat4 uPrevVP;
uniform highp vec2 uSize;
uniform highp vec3 uCameraDelta;
uniform int uHasPrev;
uniform int uReverseDepth;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= int(uSize.x) || p.y >= int(uSize.y)) return;
    float sampled = texelFetch(uDepth, p, 0).x;
    float d = uReverseDepth != 0 ? 1.0 - sampled : sampled;
    imageStore(uDepthOut, p, vec4(d, 0.0, 0.0, 1.0));
    if (uHasPrev == 0) {
        imageStore(uMotion, p, vec4(0.0));
        return;
    }
    vec2 uv = (vec2(p) + 0.5) / uSize;
    vec4 clip = uInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    if (abs(clip.w) < 1e-7) {
        imageStore(uMotion, p, vec4(0.0));
        return;
    }
    vec3 world = clip.xyz / clip.w;
    vec4 prev = uPrevVP * vec4(world + uCameraDelta, 1.0);
    if (abs(prev.w) < 1e-7) {
        imageStore(uMotion, p, vec4(0.0));
        return;
    }
    vec2 prev_uv = prev.xy / prev.w * 0.5 + 0.5;
    imageStore(uMotion, p, vec4((prev_uv - uv) * uSize, 0.0, 1.0));
}
)";

} // namespace mg_fsr3_depth
