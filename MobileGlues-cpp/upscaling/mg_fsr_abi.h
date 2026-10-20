// MobileGlues - upscaling/mg_fsr_abi.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Versioned in-process contract between the Minecraft/Iris adapter and the
// MobileGlues upscaling subsystem. This header is C-compatible; it is not a GL
// extension and does not expose backend-private resources.
#pragma once

#include <stdint.h>

#define MG_FSR_ABI_VERSION 1u

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mg_fsr_mode_t {
    MG_FSR_MODE_OFF = 0,
    MG_FSR_MODE_FSR1_SPATIAL = 1,
    MG_FSR_MODE_FSR3_TEMPORAL = 2,
} mg_fsr_mode_t;

typedef enum mg_fsr_color_space_t {
    MG_FSR_COLOR_LDR_PERCEPTUAL = 0,
    MG_FSR_COLOR_LINEAR_HDR = 1,
} mg_fsr_color_space_t;

typedef enum mg_fsr_resource_ns_t {
    MG_FSR_NS_NONE = 0,
    MG_FSR_NS_GL_FRONTEND = 1,
} mg_fsr_resource_ns_t;

typedef enum mg_fsr_reset_reason_t {
    MG_FSR_RESET_NONE = 0,
    MG_FSR_RESET_SESSION = 1,
    MG_FSR_RESET_SURFACE = 2,
    MG_FSR_RESET_RESIZE = 3,
    MG_FSR_RESET_SHADER_RELOAD = 4,
    MG_FSR_RESET_WORLD = 5,
    MG_FSR_RESET_CAMERA_CUT = 6,
    MG_FSR_RESET_PERSPECTIVE = 7,
    MG_FSR_RESET_RESOURCE = 8,
    MG_FSR_RESET_ALGORITHM = 9,
} mg_fsr_reset_reason_t;

typedef enum mg_fsr_motion_confidence_t {
    MG_FSR_MOTION_NONE = 0,
    MG_FSR_MOTION_CAMERA_ONLY = 1,
    MG_FSR_MOTION_MIXED = 2,
    MG_FSR_MOTION_FULL = 3,
} mg_fsr_motion_confidence_t;

typedef enum mg_fsr_error_t {
    MG_FSR_OK = 0,
    MG_FSR_ERROR_ABI = -1,
    MG_FSR_ERROR_SESSION = -2,
    MG_FSR_ERROR_RESOURCE = -3,
    MG_FSR_ERROR_EXTENT = -4,
    MG_FSR_ERROR_CAPABILITY = -5,
    MG_FSR_ERROR_BACKEND = -6,
    MG_FSR_ERROR_STATE = -7,
} mg_fsr_error_t;

typedef struct mg_fsr_resource_t {
    uint64_t handle;       // frontend GL name; 0 is invalid except output handle 0
    uint32_t namespace_id; // mg_fsr_resource_ns_t
    uint32_t gl_target;    // GL_TEXTURE_2D, GL_TEXTURE_2D_ARRAY, ...
    uint32_t mip;          // first sampled level
    uint32_t layer;        // first sampled layer
    uint32_t format;       // logical GL internal format
    uint32_t reserved;
    uint64_t generation;   // producer generation at handoff
} mg_fsr_resource_t;

typedef struct mg_fsr_extent_t {
    uint32_t x, y, width, height; // top-left origin
} mg_fsr_extent_t;

typedef struct mg_fsr_dispatch_t {
    uint32_t abi_version;
    uint32_t struct_size;

    uint64_t frame_id;
    uint64_t session_id;
    uint64_t context_id;
    uint64_t surface_id;
    uint64_t resource_generation;

    mg_fsr_resource_t color;
    mg_fsr_resource_t depth;
    mg_fsr_resource_t motion;
    mg_fsr_resource_t exposure;
    mg_fsr_resource_t reactive;
    mg_fsr_resource_t transparency;
    mg_fsr_resource_t output;

    mg_fsr_extent_t color_active;
    mg_fsr_extent_t output_active;

    uint32_t render_width;
    uint32_t render_height;
    uint32_t output_width;
    uint32_t output_height;

    uint32_t color_space;
    uint32_t alpha_premultiplied;
    uint32_t output_alpha_mode;
    float pre_exposure;

    uint32_t depth_reversed;
    uint32_t depth_infinite_far;
    uint32_t depth_valid;
    float camera_near;
    float camera_far;
    float camera_fov_y_radians;

    float jitter_x;
    float jitter_y;
    float motion_scale_x;
    float motion_scale_y;
    uint32_t motion_units; // 0 = render pixels, 1 = normalized UV
    uint32_t motion_valid;
    uint32_t motion_includes_jitter;
    uint32_t motion_confidence;
    uint32_t depth_includes_jitter;

    float frame_delta_seconds;
    uint32_t reset_history;
    uint32_t reset_reason;
    uint32_t render_stage_id;
    uint32_t flags;

    uint64_t reserved[8];
} mg_fsr_dispatch_t;

typedef struct mg_fsr_caps_t {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t mode_mask;
    uint32_t gles_compute;
    uint32_t fp16;
    uint32_t max_workgroup[3];
    uint32_t max_image_units;
    uint32_t max_texture_units;
    char backend_name[64];
    char failure_reason[256];
} mg_fsr_caps_t;

// All functions return MG_FSR_OK or a negative mg_fsr_error_t. Dispatch is
// synchronous on the calling render thread; GPU completion may be asynchronous.
void* mg_fsr_create_session(void);
void mg_fsr_destroy_session(void* session);
int mg_fsr_query_caps(void* session, mg_fsr_caps_t* caps);
int mg_fsr_dispatch(void* session, const mg_fsr_dispatch_t* desc);
int mg_fsr_get_output(void* session, mg_fsr_resource_t* output);
int mg_fsr_invalidate(void* session, uint32_t reason);
const char* mg_fsr_last_error(void* session);

#ifdef __cplusplus
} // extern "C"

#include <cstddef>
static_assert(MG_FSR_ABI_VERSION == 1, "contract revision changed");
static_assert(offsetof(mg_fsr_dispatch_t, abi_version) == 0, "ABI must start with version");
static_assert(offsetof(mg_fsr_dispatch_t, struct_size) == sizeof(uint32_t), "size must follow version");
static_assert(sizeof(mg_fsr_resource_t) == 40, "resource ABI changed");
static_assert(sizeof(mg_fsr_dispatch_t) % alignof(uint64_t) == 0, "dispatch must stay 8-byte aligned");
#endif
