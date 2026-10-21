// MobileGlues - upscaling/fsr3_depth_contract.h
// Copyright (c) 2025-2026 MobileGL-Dev
// SPDX-License-Identifier: LGPL-2.1-only
#pragma once

#include <cmath>
#include <initializer_list>
#include <limits>

namespace mg_fsr3_depth {

// Iris exposes a forward [-1,1] gbuffer projection in both profiles. Newer
// Iris undoes the GPU's reversed/zero-to-one projection before capturing it;
// only the sampled depth texture still needs reversal here.
constexpr int kLegacyForward = 0;
constexpr int kReversedSample = 1;
enum class SampleOrder { Forward, Reversed };

inline bool supported_convention(int convention) {
    return convention == kLegacyForward || convention == kReversedSample;
}

inline bool history_allowed(int convention, bool requested) {
    return convention == kLegacyForward && requested;
}

struct Projection {
    float near_plane = 0.0f;
    float far_plane = 0.0f;
    float fov_y = 0.0f;
    bool infinite_far = false;
};

// Decode only the perspective form used by the pinned Iris camera contract.
// The pinned adapter establishes the [-1,1] range; matrix entries alone cannot
// distinguish it from a forward [0,1] projection. Off-center/jitter terms m[8:9]
// are allowed; oblique, orthographic, reversed and nonfinite forms are rejected.
inline bool forward_projection(const float m[16], Projection& result) {
    if (!m) return false;
    for (int i = 0; i < 16; ++i) if (!std::isfinite(m[i])) return false;
    for (const int index : {1, 2, 3, 4, 6, 7, 12, 13, 15}) {
        if (m[index] != 0.0f) return false;
    }
    if (!(m[0] > 0.0f && m[5] > 0.0f && m[11] == -1.0f &&
          m[10] <= -1.0f && m[14] < 0.0f)) return false;
    Projection parsed;
    parsed.near_plane = m[14] / (m[10] - 1.0f);
    parsed.infinite_far = m[10] == -1.0f;
    parsed.far_plane = parsed.infinite_far ? std::numeric_limits<float>::max()
                                         : m[14] / (m[10] + 1.0f);
    parsed.fov_y = 2.0f * std::atan(1.0f / m[5]);
    if (!(std::isfinite(parsed.near_plane) && parsed.near_plane > 0.0f &&
          std::isfinite(parsed.far_plane) && parsed.far_plane > parsed.near_plane &&
          parsed.fov_y > 0.0f && parsed.fov_y < 3.14159265f)) return false;
    result = parsed;
    return true;
}

// The shipped shaders use forward depth ordering. Infinite far changes only
// FSR's existing reconstruction constants, not its shader permutation. Limit
// newly accepted infinite inputs to spatial dispatches with no history/motion.
inline bool supported_dispatch(bool reversed, bool infinite_far, bool reset_history,
                               bool has_motion) {
    return !reversed && (!infinite_far || (reset_history && !has_motion));
}

} // namespace mg_fsr3_depth
