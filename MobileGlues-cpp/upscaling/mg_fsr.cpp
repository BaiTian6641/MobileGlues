// MobileGlues - upscaling/mg_fsr.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Versioned in-process FSR session API. The session validates the caller's
// frame contract, owns one backend per context, and returns a backend-owned
// output. It deliberately has no fallback that calls itself FSR3.

#include "mg_fsr_abi.h"
#include "fsr3/fsr3_backend.h"

#include "../egl/context.h"
#include "../gles/gles.h"

#include <EGL/egl.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

#if defined(__GNUC__)
#define MG_FSR_EXPORT __attribute__((visibility("default")))
#else
#define MG_FSR_EXPORT
#endif

namespace {

std::atomic<uint64_t> g_next_session_id{1};

struct Session {
    uint64_t id = 0;
    uint64_t context_id = 0;
    uint64_t generation = 0;
    uint64_t last_frame_id = 0;
    uint64_t last_generation = 0;
    uint64_t last_surface_id = 0;
    bool invalidate_requested = false;
    mg_fsr3::Backend backend;
    mg_fsr_resource_t output{};
    std::string error;
};

Session* session_from(void* p) { return static_cast<Session*>(p); }

bool current_context_matches(const Session& s) {
    if (s.context_id == 0) return true;
    if (!g_current_ctx) return true; // untracked launcher EGL path
    return s.context_id == g_current_ctx->id;
}

int fail(Session* s, int code, std::string message) {
    if (s) s->error = std::move(message);
    return code;
}

bool valid_dispatch_abi(const mg_fsr_dispatch_t& d) {
    return d.abi_version == MG_FSR_ABI_VERSION && d.struct_size >= sizeof(mg_fsr_dispatch_t);
}

} // namespace

extern "C" {

MG_FSR_EXPORT void* mg_fsr_create_session(void) {
    auto session = std::make_unique<Session>();
    session->id = g_next_session_id.fetch_add(1, std::memory_order_relaxed);
    session->context_id = g_current_ctx ? g_current_ctx->id : 0;
    return session.release();
}

MG_FSR_EXPORT void mg_fsr_destroy_session(void* raw) {
    Session* session = session_from(raw);
    if (!session) return;
    // Calling GL deletion in a different context would delete that context's
    // objects. If the owning context is gone, abandon only the handles.
    const bool gl_current = (GLES.glGetString && GLES.glGetString(GL_VERSION) != nullptr) ||
                            (eglGetCurrentContext() != EGL_NO_CONTEXT);
    if ((g_current_ctx && session->context_id == g_current_ctx->id) ||
        (!g_current_ctx && gl_current))
        session->backend.destroy();
    else
        session->backend.abandon();
    delete session;
}

MG_FSR_EXPORT int mg_fsr_query_caps(void* raw, mg_fsr_caps_t* caps) {
    if (!caps) return MG_FSR_ERROR_ABI;
    if (!g_current_ctx) return MG_FSR_ERROR_STATE;
    if (caps->abi_version != MG_FSR_ABI_VERSION || caps->struct_size < sizeof(mg_fsr_caps_t))
        return MG_FSR_ERROR_ABI;
    std::memset(caps, 0, sizeof(*caps));
    caps->abi_version = MG_FSR_ABI_VERSION;
    caps->struct_size = sizeof(*caps);
    const bool compute = GLES.glDispatchCompute != nullptr && GLES.glBindImageTexture != nullptr &&
                         GLES.glGetIntegeri_v != nullptr && GLES.glMemoryBarrier != nullptr;
    caps->gles_compute = compute ? 1 : 0;
    if (compute) caps->mode_mask = 1u << MG_FSR_MODE_FSR3_TEMPORAL;
    GLint v = 0;
    for (int i = 0; i < 3; ++i) {
        if (GLES.glGetIntegeri_v) {
            GLES.glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &v);
            caps->max_workgroup[i] = static_cast<uint32_t>(std::max(0, v));
        }
    }
    if (GLES.glGetIntegerv) {
        GLES.glGetIntegerv(GL_MAX_COMPUTE_IMAGE_UNIFORMS, &v);
        caps->max_image_units = static_cast<uint32_t>(std::max(0, v));
        GLES.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &v);
        caps->max_texture_units = static_cast<uint32_t>(std::max(0, v));
    }
    std::snprintf(caps->backend_name, sizeof(caps->backend_name), "%s", compute ? "fsr3-gles" : "unavailable");
    if (!compute) std::snprintf(caps->failure_reason, sizeof(caps->failure_reason),
                                "%s", "GLES compute/image entry points are unavailable");
    return MG_FSR_OK;
}

MG_FSR_EXPORT int mg_fsr_dispatch(void* raw, const mg_fsr_dispatch_t* desc) {
    Session* session = session_from(raw);
    if (!session) return MG_FSR_ERROR_SESSION;
    if (!desc || !valid_dispatch_abi(*desc)) return fail(session, MG_FSR_ERROR_ABI, "incompatible FSR ABI");
    if (!current_context_matches(*session))
        return fail(session, MG_FSR_ERROR_STATE, "FSR session belongs to a different context");
    // The launcher may create EGL outside MobileGlues' tracked wrapper. The
    // caller is on a render thread; the backend will truthfully fail if there is
    // no usable context. Do not require our internal context shadow here.

    if (session->context_id == 0)
        session->context_id = g_current_ctx ? g_current_ctx->id : 1;

    if (desc->color.handle == 0 || desc->depth.handle == 0 || desc->motion.handle == 0)
        return fail(session, MG_FSR_ERROR_RESOURCE, "color, depth and motion are required");
    if (desc->context_id != 0 && desc->context_id != session->context_id)
        return fail(session, MG_FSR_ERROR_STATE, "dispatch context identity mismatch");
    if (desc->frame_id != 0 && session->last_frame_id != 0 && desc->frame_id <= session->last_frame_id)
        return fail(session, MG_FSR_ERROR_STATE, "non-monotonic frame id");
    if (desc->depth_reversed || desc->depth_infinite_far)
        return fail(session, MG_FSR_ERROR_CAPABILITY, "inverted/infinite depth needs the inverted shader permutation");
    auto valid_tex = [](const mg_fsr_resource_t& r, bool required) {
        if (r.handle == 0) return !required;
        if (r.namespace_id != MG_FSR_NS_GL_FRONTEND) return false;
        if (r.gl_target != GL_TEXTURE_2D || r.mip != 0 || r.layer != 0) return false;
        if (r.handle > 0xFFFFFFFFull) return false;
        return GLES.glIsTexture(static_cast<GLuint>(r.handle)) != GL_FALSE;
    };
    if (!valid_tex(desc->color, true) || !valid_tex(desc->depth, true) || !valid_tex(desc->motion, true))
        return fail(session, MG_FSR_ERROR_RESOURCE, "invalid required texture resource");
    if ((desc->exposure.handle && !valid_tex(desc->exposure, false)) ||
        (desc->reactive.handle && !valid_tex(desc->reactive, false)) ||
        (desc->transparency.handle && !valid_tex(desc->transparency, false)))
        return fail(session, MG_FSR_ERROR_RESOURCE, "invalid optional texture resource");
    if (desc->render_width == 0 || desc->render_height == 0 || desc->output_width == 0 ||
        desc->output_height == 0)
        return fail(session, MG_FSR_ERROR_EXTENT, "zero-size FSR dispatch");
    if (desc->color.namespace_id != MG_FSR_NS_GL_FRONTEND ||
        desc->depth.namespace_id != MG_FSR_NS_GL_FRONTEND ||
        desc->motion.namespace_id != MG_FSR_NS_GL_FRONTEND)
        return fail(session, MG_FSR_ERROR_RESOURCE, "unsupported resource namespace");
    if (desc->output.handle != 0)
        return fail(session, MG_FSR_ERROR_CAPABILITY, "application-provided output is not qualified");
    if (desc->color_space == MG_FSR_COLOR_LINEAR_HDR)
        return fail(session, MG_FSR_ERROR_CAPABILITY, "HDR FSR profile is not qualified");
    if (desc->motion_units > 1)
        return fail(session, MG_FSR_ERROR_CAPABILITY, "unsupported motion-vector units");
    const bool full_color_active = (desc->color_active.x == 0 && desc->color_active.y == 0 &&
                                    (desc->color_active.width == 0 || desc->color_active.width == desc->render_width) &&
                                    (desc->color_active.height == 0 || desc->color_active.height == desc->render_height));
    const bool full_output_active = (desc->output_active.x == 0 && desc->output_active.y == 0 &&
                                     (desc->output_active.width == 0 || desc->output_active.width == desc->output_width) &&
                                     (desc->output_active.height == 0 || desc->output_active.height == desc->output_height));
    if (!full_color_active || !full_output_active)
        return fail(session, MG_FSR_ERROR_EXTENT, "active rectangles are not yet supported");
    const bool generation_reset = desc->resource_generation != 0 &&
                                   desc->resource_generation != session->last_generation;
    const bool surface_reset = desc->surface_id != 0 && session->last_surface_id != 0 &&
                               desc->surface_id != session->last_surface_id;
    session->last_generation = desc->resource_generation;
    session->last_surface_id = desc->surface_id;

    mg_fsr3::Inputs in{};
    in.frame_id = desc->frame_id;
    in.resource_generation = desc->resource_generation;
    in.color = desc->color;
    in.depth = desc->depth;
    in.motion = desc->motion;
    in.exposure = desc->exposure;
    in.reactive = desc->reactive;
    in.transparency = desc->transparency;
    in.output = desc->output;
    in.render_width = desc->render_width;
    in.render_height = desc->render_height;
    in.output_width = desc->output_width;
    in.output_height = desc->output_height;
    in.hdr_color = desc->color_space == MG_FSR_COLOR_LINEAR_HDR;
    in.alpha_premultiplied = desc->alpha_premultiplied != 0;
    in.pre_exposure = desc->pre_exposure > 0.0f ? desc->pre_exposure : 1.0f;
    in.depth_reversed = desc->depth_reversed != 0;
    in.depth_infinite_far = desc->depth_infinite_far != 0;
    in.depth_valid = desc->depth_valid != 0;
    in.camera_near = desc->camera_near;
    in.camera_far = desc->camera_far;
    in.camera_fov_y_radians = desc->camera_fov_y_radians;
    in.jitter_x = desc->jitter_x;
    in.jitter_y = desc->jitter_y;
    in.motion_scale_x = desc->motion_scale_x;
    in.motion_scale_y = desc->motion_scale_y;
    in.motion_valid = desc->motion_valid != 0;
    in.motion_includes_jitter = desc->motion_includes_jitter != 0;
    in.motion_units = desc->motion_units;
    in.frame_delta_seconds = desc->frame_delta_seconds;
    in.reset_history = desc->reset_history != 0 || session->invalidate_requested || generation_reset || surface_reset;
    in.reset_reason = desc->reset_reason;
    session->invalidate_requested = false;

    session->output = {};
    if (!session->backend.ready()) {
        if (!session->backend.initialize(in.render_width, in.render_height, in.output_width, in.output_height)) {
            return fail(session, MG_FSR_ERROR_BACKEND, session->backend.error());
        }
        ++session->generation;
    } else if (session->backend.output_width() != in.output_width ||
               session->backend.output_height() != in.output_height ||
               session->backend.render_width() != in.render_width ||
               session->backend.render_height() != in.render_height) {
        if (!session->backend.resize(in.render_width, in.render_height, in.output_width, in.output_height)) {
            return fail(session, MG_FSR_ERROR_BACKEND, session->backend.error());
        }
        ++session->generation;
    }

    if (!session->backend.dispatch(in)) return fail(session, MG_FSR_ERROR_BACKEND, session->backend.error());

    session->last_frame_id = desc->frame_id;
    session->output = {};
    session->output.handle = session->backend.output_texture();
    session->output.namespace_id = MG_FSR_NS_GL_FRONTEND;
    session->output.gl_target = GL_TEXTURE_2D;
    session->output.mip = 0;
    session->output.layer = 0;
    session->output.format = GL_RGBA8;
    session->output.generation = session->generation;
    session->error.clear();
    return MG_FSR_OK;
}

MG_FSR_EXPORT int mg_fsr_get_output(void* raw, mg_fsr_resource_t* output) {
    Session* session = session_from(raw);
    if (!session || !output) return MG_FSR_ERROR_ABI;
    if (session->output.handle == 0) return fail(session, MG_FSR_ERROR_STATE, "no FSR output yet");
    *output = session->output;
    return MG_FSR_OK;
}

MG_FSR_EXPORT int mg_fsr_invalidate(void* raw, uint32_t reason) {
    Session* session = session_from(raw);
    if (!session) return MG_FSR_ERROR_SESSION;
    session->invalidate_requested = true;
    session->error.clear();
    (void)reason;
    return MG_FSR_OK;
}

MG_FSR_EXPORT const char* mg_fsr_last_error(void* raw) {
    Session* session = session_from(raw);
    return session ? session->error.c_str() : "invalid FSR session";
}

} // extern "C"
