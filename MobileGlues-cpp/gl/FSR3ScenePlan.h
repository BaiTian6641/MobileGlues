// MobileGlues - gl/FSR3ScenePlan.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1.
// SPDX-License-Identifier: LGPL-2.1-only
//
// Pure scene-viewport planning shared by the GL wrapper and host tests.
#pragma once

#include <algorithm>
#include <cmath>

struct FSR3ViewportPlan {
    bool adjust = false;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// The bridge ABI consumes a native-resolution final color target. Reject an
// intermediate target before allocation or dispatch can apply scaling twice.
inline bool FSR3IsNativeOutputExtent(int surface_width, int surface_height,
                                    int color_width, int color_height) {
    return surface_width > 0 && surface_height > 0 &&
           color_width == surface_width && color_height == surface_height;
}

inline FSR3ViewportPlan FSR3PlanViewport(int surface_width, int surface_height,
                                         int target_width, int target_height,
                                         int x, int y, int width, int height) {
    FSR3ViewportPlan plan;
    plan.x = x;
    plan.y = y;
    plan.width = width;
    plan.height = height;
    if (surface_width <= 0 || surface_height <= 0 || target_width <= 0 || target_height <= 0 ||
        width <= 0 || height <= 0)
        return plan;
    if (width != surface_width || height != surface_height) return plan;
    if (target_width >= surface_width || target_height >= surface_height) return plan;
    const float sx = static_cast<float>(target_width) / static_cast<float>(surface_width);
    const float sy = static_cast<float>(target_height) / static_cast<float>(surface_height);
    if (sx >= 0.999f && sy >= 0.999f) return plan;
    plan.adjust = true;
    plan.x = static_cast<int>(std::lround(x * sx));
    plan.y = static_cast<int>(std::lround(y * sy));
    plan.width = std::max(1, static_cast<int>(std::lround(width * sx)));
    plan.height = std::max(1, static_cast<int>(std::lround(height * sy)));
    return plan;
}
