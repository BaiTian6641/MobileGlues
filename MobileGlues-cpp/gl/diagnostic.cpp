// MobileGlues - gl/diagnostic.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header

// Experiment A trace. This file is only linked into the diagnostic core. It
// observes; it never routes targets, creates resources, reads pixels or calls
// glFinish. Queries are bounded to role snapshots and the first/last draw of a
// role, and all changed active-texture/renderbuffer state is restored.

#include "diagnostic.h"

#ifdef MG_FSR3_DIAGNOSTICS

#include "diagnostic_format.h"
#include "framebuffer.h"
#include "texture.h"
#include "mg.h"
#include "../egl/context.h"

#include <EGL/egl.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

std::atomic<bool> g_enabled{false};

struct DrawSummary {
    bool valid = false;
    int draw_fbo = 0;
    int read_fbo = 0;
    int viewport[4]{};
    int program = 0;
};

struct RoleState {
    bool active = false;
    int role = -1;
    int frame = -1;
    int event = -1;
    uint64_t draws = 0;
    std::string first_draw;
    DrawSummary last_draw;
};

thread_local RoleState g_role;
std::atomic<uint64_t> g_dropped_roles{0};

int current_thread_id() {
#ifdef __linux__
    return static_cast<int>(syscall(SYS_gettid));
#else
    return 0;
#endif
}

void append_field(std::string& out, const char* name, bool& first) {
    if (!first) out.push_back(',');
    first = false;
    mg_diag::json_escape(out, name);
    out.push_back(':');
}

void append_int(std::string& out, const char* name, int64_t value, bool& first) {
    append_field(out, name, first);
    mg_diag::json_int(out, value);
}

void append_bool(std::string& out, const char* name, bool value, bool& first) {
    append_field(out, name, first);
    mg_diag::json_bool(out, value);
}

void append_unknown(std::string& out, const char* name, bool& first) {
    append_field(out, name, first);
    mg_diag::json_null(out);
}

void append_int_array(std::string& out, const char* name, const int* values, size_t count, bool& first) {
    append_field(out, name, first);
    mg_diag::json_int_array(out, values, count);
}

void append_string(std::string& out, const char* name, const std::string& value, bool& first) {
    append_field(out, name, first);
    mg_diag::json_escape(out, value);
}

const char* attach_kind_name(attach_kind_t kind) {
    switch (kind) {
    case attach_kind_t::Texture2D: return "texture2d";
    case attach_kind_t::TextureLayer: return "texture_layer";
    case attach_kind_t::TextureAll: return "texture_all";
    case attach_kind_t::Renderbuffer: return "renderbuffer";
    case attach_kind_t::None:
    default: return "none";
    }
}

const char* gl_object_kind(GLint type) {
    switch (type) {
    case GL_TEXTURE: return "texture";
    case GL_RENDERBUFFER: return "renderbuffer";
    case GL_NONE: return "none";
    default: return "unknown";
    }
}

struct AttachmentInfo {
    bool present = false;
    GLint object_type = GL_NONE;
    GLuint name = 0;
    GLuint texture = 0;
    GLuint renderbuffer = 0;
    GLint level = 0;
    GLint layer = 0;
    attach_kind_t kind = attach_kind_t::None;
    GLint format = 0;
    GLint width = 0;
    GLint height = 0;
    GLint depth = 0;
    GLint samples = 0;
    GLint target = 0;
    bool extent_known = false;
    bool format_known = false;
    bool samples_known = false;
};

AttachmentInfo query_attachment(GLuint fbo, GLenum attachment, GLuint wrapper_slot) {
    AttachmentInfo info;
    if (fbo == 0) return info;

    GLint type = GL_NONE;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type == GL_NONE) return info;
    info.present = true;
    info.object_type = type;

    GLint name = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
    info.name = static_cast<GLuint>(name);

    if (attachment == GL_COLOR_ATTACHMENT0 + wrapper_slot) {
        GLuint wrapper_texture = 0;
        GLint wrapper_level = 0, wrapper_layer = 0;
        if (mg_framebuffer_color_attachment_record(fbo, wrapper_slot, info.kind, wrapper_texture,
                                                   wrapper_level, wrapper_layer)) {
            info.level = wrapper_level;
            info.layer = wrapper_layer;
            if (info.kind == attach_kind_t::Texture2D || info.kind == attach_kind_t::TextureLayer ||
                info.kind == attach_kind_t::TextureAll) {
                info.texture = wrapper_texture;
                info.name = wrapper_texture;
            } else if (info.kind == attach_kind_t::Renderbuffer) {
                info.renderbuffer = wrapper_texture;
                info.name = wrapper_texture;
            }
        }
    }

    if (type == GL_TEXTURE) {
        if (info.texture == 0) info.texture = static_cast<GLuint>(name);
        const TextureObject* texture = mgGetTexObjectByID(info.texture);
        if (texture) {
            info.target = static_cast<GLint>(ConvertTextureTargetToGLEnum(texture->target));
            info.format = static_cast<GLint>(texture->internal_format);
            info.width = texture->width;
            info.height = texture->height;
            info.depth = texture->depth;
            info.extent_known = info.width > 0 && info.height > 0;
            info.format_known = info.format != 0;
        }
    } else if (type == GL_RENDERBUFFER) {
        if (info.renderbuffer == 0) info.renderbuffer = static_cast<GLuint>(name);
        GLint previous = 0;
        GLES.glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
        GLES.glBindRenderbuffer(GL_RENDERBUFFER, info.renderbuffer);
        GLES.glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &info.width);
        GLES.glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &info.height);
        GLES.glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &info.samples);
        GLES.glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &info.format);
        GLES.glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(previous));
        info.extent_known = info.width > 0 && info.height > 0;
        info.format_known = info.format != 0;
        info.samples_known = true;
    }
    return info;
}

void append_attachment(std::string& out, const char* name, const AttachmentInfo& info, bool& first) {
    append_field(out, name, first);
    if (!info.present) {
        mg_diag::json_null(out);
        return;
    }
    out.push_back('{');
    bool af = true;
    append_string(out, "kind", gl_object_kind(info.object_type), af);
    append_string(out, "attachment_kind", attach_kind_name(info.kind), af);
    append_int(out, "name", info.name, af);
    if (info.object_type == GL_TEXTURE) {
        append_int(out, "texture", info.texture, af);
    } else if (info.object_type == GL_RENDERBUFFER) {
        append_int(out, "renderbuffer", info.renderbuffer, af);
    }
    append_int(out, "target", info.target, af);
    append_int(out, "level", info.level, af);
    append_int(out, "layer", info.layer, af);
    if (info.format_known) append_int(out, "format", info.format, af);
    else append_unknown(out, "format", af);
    if (info.extent_known) {
        int extent[2] = {info.width, info.height};
        append_int_array(out, "extent", extent, 2, af);
    } else {
        append_unknown(out, "extent", af);
    }
    if (info.samples_known) append_int(out, "samples", info.samples, af);
    else append_unknown(out, "samples", af);
    append_unknown(out, "generation", af);
    append_unknown(out, "mips", af);
    out.push_back('}');
}

void append_uniform_int(std::string& out, const char* name, GLuint program, bool& first) {
    append_field(out, name, first);
    GLint location = program ? GLES.glGetUniformLocation(program, name) : -1;
    if (location < 0) {
        mg_diag::json_escape(out, "inactive");
        return;
    }
    GLint value = 0;
    GLES.glGetUniformiv(program, location, &value);
    mg_diag::json_int(out, value);
}

void append_sampler(std::string& out, const char* name, GLuint program, bool& first) {
    append_field(out, name, first);
    GLint location = program ? GLES.glGetUniformLocation(program, name) : -1;
    if (location < 0) {
        mg_diag::json_escape(out, "inactive");
        return;
    }
    GLint unit = -1;
    GLES.glGetUniformiv(program, location, &unit);
    GLint max_units = 0;
    GLES.glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_units);
    if (unit < 0 || (max_units > 0 && unit >= max_units)) {
        mg_diag::json_escape(out, "inactive");
        return;
    }
    GLint old_active = GL_TEXTURE0;
    GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &old_active);
    GLES.glActiveTexture(GL_TEXTURE0 + unit);
    GLint texture = 0, sampler = 0;
    GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    GLES.glGetIntegerv(GL_SAMPLER_BINDING, &sampler);
    GLES.glActiveTexture(static_cast<GLenum>(old_active));
    out.push_back('{');
    bool sf = true;
    append_int(out, "unit", unit, sf);
    append_int(out, "texture", texture, sf);
    append_int(out, "sampler", sampler, sf);
    out.push_back('}');
}

std::string draw_summary_json(const DrawSummary& summary) {
    if (!summary.valid) return "null";
    std::string out;
    out.push_back('{');
    bool first = true;
    append_int(out, "draw_fbo", summary.draw_fbo, first);
    append_int(out, "read_fbo", summary.read_fbo, first);
    append_int_array(out, "viewport", summary.viewport, 4, first);
    append_int(out, "program", summary.program, first);
    out.push_back('}');
    return out;
}

std::string snapshot_json(bool include_role) {
    std::string out;
    out.reserve(2048);
    out.push_back('{');
    bool first = true;

    append_int(out, "thread", current_thread_id(), first);
    unsigned long long context = g_current_ctx ? g_current_ctx->id : 0;
    append_int(out, "context", static_cast<int64_t>(context), first);
    EGLSurface surface = eglGetCurrentSurface(EGL_DRAW);
    if (surface != EGL_NO_SURFACE) {
        append_int(out, "surface", static_cast<int64_t>(reinterpret_cast<uintptr_t>(surface)), first);
    } else {
        append_unknown(out, "surface", first);
    }

    GLint draw_fbo = 0, read_fbo = 0;
    GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fbo);
    GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_fbo);
    append_int(out, "draw_fbo", draw_fbo, first);
    append_int(out, "read_fbo", read_fbo, first);
    GLuint tracked_draw = 0, tracked_read = 0;
    mg_framebuffer_binding_ids(tracked_draw, tracked_read);
    append_int(out, "tracked_draw_fbo", tracked_draw, first);
    append_int(out, "tracked_read_fbo", tracked_read, first);
    append_bool(out, "default_framebuffer", draw_fbo == 0, first);

    int draw_buffers[8] = {0};
    for (int i = 0; i < 8; ++i) {
        GLES.glGetIntegerv(GL_DRAW_BUFFER0 + i, &draw_buffers[i]);
    }
    append_int_array(out, "draw_buffers", draw_buffers, 8, first);
    GLint read_buffer = 0;
    GLES.glGetIntegerv(GL_READ_BUFFER, &read_buffer);
    append_int(out, "read_buffer", read_buffer, first);

    GLint viewport[4] = {0};
    GLES.glGetIntegerv(GL_VIEWPORT, viewport);
    append_int_array(out, "viewport", viewport, 4, first);
    GLint scissor[4] = {0};
    GLES.glGetIntegerv(GL_SCISSOR_BOX, scissor);
    bool scissor_first = true;
    append_field(out, "scissor", first);
    out.push_back('{');
    append_bool(out, "enabled", GLES.glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE, scissor_first);
    append_int_array(out, "box", scissor, 4, scissor_first);
    out.push_back('}');

    GLint program = 0, blend = 0, depth_test = 0, stencil_test = 0, depth_mask = 0, depth_func = 0;
    GLboolean color_mask[4] = {0};
    GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    GLES.glGetIntegerv(GL_BLEND, &blend);
    GLES.glGetIntegerv(GL_DEPTH_TEST, &depth_test);
    GLES.glGetIntegerv(GL_STENCIL_TEST, &stencil_test);
    GLES.glGetIntegerv(GL_DEPTH_WRITEMASK, &depth_mask);
    GLES.glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    GLES.glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);
    append_int(out, "program", program, first);
    bool state_first = true;
    append_field(out, "state", first);
    out.push_back('{');
    append_bool(out, "blend", blend != 0, state_first);
    append_bool(out, "depth_test", depth_test != 0, state_first);
    append_bool(out, "depth_mask", depth_mask != 0, state_first);
    append_int(out, "depth_func", depth_func, state_first);
    append_bool(out, "stencil_test", stencil_test != 0, state_first);
    {
        int mask[4] = {color_mask[0], color_mask[1], color_mask[2], color_mask[3]};
        append_int_array(out, "color_mask", mask, 4, state_first);
    }
    out.push_back('}');

    bool attachments_first = true;
    append_field(out, "attachments", first);
    out.push_back('{');
    append_field(out, "color", attachments_first);
    out.push_back('[');
    bool emitted = false;
    if (draw_fbo != 0) {
        for (GLuint i = 0; i < 8; ++i) {
            AttachmentInfo info = query_attachment(static_cast<GLuint>(draw_fbo),
                                                   GL_COLOR_ATTACHMENT0 + i, i);
            if (!info.present) continue;
            if (emitted) out.push_back(',');
            emitted = true;
            bool wrapper_first = true;
            std::string item;
            item.push_back('{');
            append_int(item, "slot", i, wrapper_first);
            append_attachment(item, "info", info, wrapper_first);
            item.push_back('}');
            // The nested item has its own field list; splice it without quotes.
            out += item;
        }
    }
    out.push_back(']');
    AttachmentInfo depth_attachment = query_attachment(static_cast<GLuint>(draw_fbo), GL_DEPTH_ATTACHMENT, 0);
    AttachmentInfo stencil_attachment = query_attachment(static_cast<GLuint>(draw_fbo), GL_STENCIL_ATTACHMENT, 0);
    append_attachment(out, "depth", depth_attachment, attachments_first);
    append_attachment(out, "stencil", stencil_attachment, attachments_first);
    out.push_back('}');

    bool uniforms_first = true;
    append_field(out, "uniforms", first);
    out.push_back('{');
    append_uniform_int(out, "viewWidth", static_cast<GLuint>(program), uniforms_first);
    append_uniform_int(out, "viewHeight", static_cast<GLuint>(program), uniforms_first);
    append_uniform_int(out, "viewWidthOverride", static_cast<GLuint>(program), uniforms_first);
    append_uniform_int(out, "viewHeightOverride", static_cast<GLuint>(program), uniforms_first);
    out.push_back('}');

    bool samplers_first = true;
    append_field(out, "samplers", first);
    out.push_back('{');
    append_sampler(out, "depthtex0", static_cast<GLuint>(program), samplers_first);
    append_sampler(out, "depthtex1", static_cast<GLuint>(program), samplers_first);
    append_sampler(out, "depthtex2", static_cast<GLuint>(program), samplers_first);
    append_sampler(out, "colortex0", static_cast<GLuint>(program), samplers_first);
    append_sampler(out, "gcolor", static_cast<GLuint>(program), samplers_first);
    out.push_back('}');

    if (include_role) {
        append_field(out, "role", first);
        if (!g_role.active) {
            mg_diag::json_null(out);
        } else {
            out.push_back('{');
            bool rf = true;
            append_int(out, "id", g_role.role, rf);
            append_int(out, "frame", g_role.frame, rf);
            append_int(out, "event", g_role.event, rf);
            append_int(out, "draw_calls", static_cast<int64_t>(g_role.draws), rf);
            append_bool(out, "draw_executed", g_role.draws > 0, rf);
            append_field(out, "first_draw", rf);
            out += g_role.first_draw.empty() ? "null" : g_role.first_draw;
            append_field(out, "last_draw", rf);
            out += draw_summary_json(g_role.last_draw);
            out.push_back('}');
        }
    }
    append_int(out, "dropped_roles", static_cast<int64_t>(g_dropped_roles.load()), first);
    out.push_back('}');
    return out;
}

DrawSummary lightweight_draw_summary() {
    DrawSummary summary;
    GLint draw = 0, read = 0, program = 0;
    GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
    GLES.glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
    GLES.glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    GLES.glGetIntegerv(GL_VIEWPORT, summary.viewport);
    summary.draw_fbo = draw;
    summary.read_fbo = read;
    summary.program = program;
    summary.valid = true;
    return summary;
}

// The JSON field emitter above writes a nested object for color items; this
// helper is declared separately so the splice stays explicit rather than
// relying on a string formatter.
} // namespace

bool mg_diag_enabled() {
    return g_enabled.load(std::memory_order_relaxed);
}

void mg_diag_set_enabled(bool enabled) {
    g_enabled.store(enabled, std::memory_order_relaxed);
    if (!enabled) {
        g_role = RoleState{};
        g_dropped_roles.store(0);
    }
}

void mg_diag_reset() {
    mg_diag_set_enabled(false);
}

void mg_diag_role_start(int role, int frame, int event) {
    if (!g_enabled.load(std::memory_order_relaxed)) return;
    if (g_role.active && g_role.draws > 0) {
        g_dropped_roles.fetch_add(1, std::memory_order_relaxed);
    }
    g_role = RoleState{};
    g_role.active = true;
    g_role.role = role;
    g_role.frame = frame;
    g_role.event = event;
}

std::string mg_diag_snapshot_json() {
    if (!g_enabled.load(std::memory_order_relaxed)) return {};
    return snapshot_json(true);
}

std::string mg_diag_role_end_json() {
    if (!g_enabled.load(std::memory_order_relaxed)) return {};
    std::string result = snapshot_json(true);
    g_role = RoleState{};
    return result;
}

void mg_diag_on_draw() {
    if (!g_enabled.load(std::memory_order_relaxed)) return;
    if (!g_role.active) return;
    ++g_role.draws;
    if (g_role.first_draw.empty()) {
        g_role.first_draw = snapshot_json(false);
    }
    g_role.last_draw = lightweight_draw_summary();
}

uint64_t mg_diag_draw_count_for_test() {
    return g_enabled.load(std::memory_order_relaxed) ? g_role.draws : 0;
}

#endif // MG_FSR3_DIAGNOSTICS
