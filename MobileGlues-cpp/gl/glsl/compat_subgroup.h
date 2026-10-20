// MobileGlues - gl/glsl/compat_subgroup.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#ifndef MOBILEGLUES_GLSL_COMPAT_SUBGROUP_H
#define MOBILEGLUES_GLSL_COMPAT_SUBGROUP_H

#include <GL/gl.h>
#include <string>

// <GL/gl.h> on some host headers does not carry GL 4.3 enums.
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif

// Result of lowering desktop GLSL subgroup constructs to constructs that ES
// 3.2 / glslang accept. When `unsupported` is true the returned `source`
// contains an `#error MG_SUBGROUP_UNSUPPORTED: ...` line so the shader fails
// to compile instead of silently changing meaning.
struct MgSubgroupLoweringResult {
    std::string source;
    bool transformed;
    bool unsupported;
    std::string reason;
};

// Rewrite subgroup constructs in a compute shader. Other stages are returned
// untouched. The NV partitioned rewrite owned by compat_subgroup_partition is
// applied before the generic subgroup lowering; if it rejects the source the
// result is the loud #error form described above.
MgSubgroupLoweringResult mg_lower_subgroup_constructs(const std::string& source, GLenum shader_type);

#endif // MOBILEGLUES_GLSL_COMPAT_SUBGROUP_H
