// MobileGlues - gl/FSR3Direct.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
// Focused host seam: substitutes GL query results, not a real provider.
#include <atomic>
#include <cstdio>
#include "fsr3_scene_plan_at_5d7ac39.h"
using GLint = int;
using GLenum = unsigned;
using GLuint = unsigned;
enum { GL_TEXTURE0=1, GL_DRAW_FRAMEBUFFER_BINDING, GL_DRAW_FRAMEBUFFER,
       GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
       GL_TEXTURE, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, GL_ACTIVE_TEXTURE,
       GL_TEXTURE_BINDING_2D, GL_TEXTURE_2D, GL_TEXTURE_WIDTH, GL_TEXTURE_HEIGHT };
enum class FSR3_Mode { Experimental };
struct { FSR3_Mode fsr3_setting = FSR3_Mode::Experimental; } global_settings;
std::atomic<int> g_native_width{2880}, g_native_height{1585};
bool surface_size(int&, int&) { return false; }
int target_width = 4096, target_height = 4096;
struct QuerySeam {
    void glGetIntegerv(int name, int* out) {
        *out = name == GL_ACTIVE_TEXTURE ? GL_TEXTURE0 : 1;
    }
    void glGetFramebufferAttachmentParameteriv(int, int, int name, int* out) {
        *out = name == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE ? GL_TEXTURE : 7;
    }
    void glActiveTexture(int) {}
    void glBindTexture(int, unsigned) {}
    void glGetTexLevelParameteriv(int, int, int name, int* out) {
        *out = name == GL_TEXTURE_WIDTH ? target_width : target_height;
    }
} GLES;
#define LOG_W_FORCE(...) ((void)0)
bool FSR3AdjustViewport(int& x, int& y, int& width, int& height) {
    if (global_settings.fsr3_setting != FSR3_Mode::Experimental) return false;
    if (width <= 0 || height <= 0) return false;
    int surface_w = g_native_width.load(std::memory_order_relaxed);
    int surface_h = g_native_height.load(std::memory_order_relaxed);
    if (surface_w <= 0 || surface_h <= 0) {
        if (!surface_size(surface_w, surface_h)) return false;
    }
    GLint fbo = 0;
    GLES.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo);
    if (fbo == 0) return false;
    GLint type = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
    if (type != GL_TEXTURE) return false;
    GLint tex = 0;
    GLES.glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                               GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex);
    if (tex == 0) return false;
    GLint prev_unit = GL_TEXTURE0, prev_tex = 0;
    GLES.glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
    GLES.glActiveTexture(GL_TEXTURE0);
    GLES.glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(tex));
    GLint target_w = 0, target_h = 0;
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &target_w);
    GLES.glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &target_h);
    GLES.glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
    GLES.glActiveTexture(static_cast<GLenum>(prev_unit));
    if (target_w >= surface_w && target_h >= surface_h) {
        if (width == surface_w && height == surface_h) return false;
        x = 0;
        y = 0;
        width = surface_w;
        height = surface_h;
        static bool logged_native = false;
        if (!logged_native) {
            logged_native = true;
            LOG_W_FORCE("FSR3 native-target viewport %dx%d", width, height);
        }
        return true;
    }
    if (width != surface_w || height != surface_h) return false;
    const FSR3ViewportPlan plan = FSR3PlanViewport(surface_w, surface_h, target_w, target_h,
                                                    x, y, width, height);
    if (!plan.adjust) return false;
    x = plan.x;
    y = plan.y;
    width = plan.width;
    height = plan.height;
    static bool logged = false;
    if (!logged) {
        logged = true;
        LOG_W_FORCE("FSR3 scene viewport %dx%d -> %dx%d", surface_w, surface_h, width, height);
    }
    return true;
}

int main() {
    int failed = 0;
    int x=0, y=0, w=4096, h=4096;
    bool adjusted = FSR3AdjustViewport(x,y,w,h);
    bool kept = !adjusted && x==0 && y==0 && w==4096 && h==4096;
    std::printf("4096 color shadow target: adjusted=%d viewport=%d,%d,%d,%d preserve=%s\n",
        adjusted,x,y,w,h,kept?"PASS":"FAIL");
    failed += !kept;
    target_width=2880; target_height=1585;
    x=100; y=50; w=640; h=360;
    adjusted = FSR3AdjustViewport(x,y,w,h);
    kept = !adjusted && x==100 && y==50 && w==640 && h==360;
    std::printf("native target deliberate subviewport: adjusted=%d viewport=%d,%d,%d,%d preserve=%s\n",
        adjusted,x,y,w,h,kept?"PASS":"FAIL");
    failed += !kept;
    return failed ? 1 : 0;
}
