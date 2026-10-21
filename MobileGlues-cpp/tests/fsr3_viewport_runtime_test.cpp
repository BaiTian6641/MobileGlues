// Link the real viewport adapter against a recording GLES/EGL provider. In
// particular, a native-sized attachment does not authorize replacing a caller's
// viewport or scissor rectangle with the whole output surface.
#include "gl/FSR3Direct.h"
#include "config/settings.h"
#include "gles/gles.h"
#include <EGL/egl.h>
#include <array>
#include <cstdio>

gles_func_t g_gles_func{};
global_settings_t global_settings{};
int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" void write_log(const char*, ...) {}

namespace {
int failures = 0;
bool current_surface = true;
bool surface_query_ok = true;
int surface_width = 2880, surface_height = 1585;
int target_width = 2880, target_height = 1585;
GLint framebuffer = 7, attachment_type = GL_TEXTURE, attachment = 13;
GLenum active_unit = GL_TEXTURE3;
std::array<GLuint, 4> bindings{41, 42, 43, 44};
std::array<int, 4> driver_viewport{};
unsigned viewport_calls = 0;

void check(bool value, const char* description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); ++failures; }
}
void get_integer(GLenum pname, GLint* result) {
    switch (pname) {
        case GL_DRAW_FRAMEBUFFER_BINDING: *result = framebuffer; break;
        case GL_ACTIVE_TEXTURE: *result = static_cast<GLint>(active_unit); break;
        case GL_TEXTURE_BINDING_2D: *result = static_cast<GLint>(bindings.at(active_unit - GL_TEXTURE0)); break;
        default: check(false, "unexpected integer query"); *result = 0;
    }
}
void get_attachment(GLenum target, GLenum slot, GLenum pname, GLint* result) {
    check(target == GL_DRAW_FRAMEBUFFER && slot == GL_COLOR_ATTACHMENT0, "query current draw color attachment");
    if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE) *result = attachment_type;
    else if (pname == GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME) *result = attachment;
    else { check(false, "unexpected attachment query"); *result = 0; }
}
void active_texture(GLenum unit) { active_unit = unit; }
void bind_texture(GLenum target, GLuint texture) {
    check(target == GL_TEXTURE_2D, "borrow 2D texture binding only");
    bindings.at(active_unit - GL_TEXTURE0) = texture;
}
void get_texture(GLenum target, GLint level, GLenum pname, GLint* result) {
    check(target == GL_TEXTURE_2D && level == 0 && bindings.at(active_unit - GL_TEXTURE0) ==
          static_cast<GLuint>(attachment), "inspect attached texture at level zero");
    if (pname == GL_TEXTURE_WIDTH) *result = target_width;
    else if (pname == GL_TEXTURE_HEIGHT) *result = target_height;
    else { check(false, "unexpected texture query"); *result = 0; }
}
void viewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    driver_viewport = {x, y, w, h};
    ++viewport_calls;
}
void request(const char* name, std::array<int, 4> rectangle, bool adjusted,
             std::array<int, 4> expected) {
    const auto saved_bindings = bindings;
    const auto saved_unit = active_unit;
    const auto saved_viewport_calls = viewport_calls;
    const bool result = FSR3AdjustViewport(rectangle[0], rectangle[1], rectangle[2], rectangle[3]);
    check(result == adjusted && rectangle == expected, name);
    check(bindings == saved_bindings && active_unit == saved_unit, "texture units and bindings restored");
    check(viewport_calls == saved_viewport_calls, "adjustment does not issue a hidden viewport call");
}
}

extern "C" EGLDisplay eglGetCurrentDisplay() {
    return current_surface ? reinterpret_cast<EGLDisplay>(1) : EGL_NO_DISPLAY;
}
extern "C" EGLSurface eglGetCurrentSurface(EGLint which) {
    check(which == EGL_DRAW, "query draw surface");
    return current_surface ? reinterpret_cast<EGLSurface>(2) : EGL_NO_SURFACE;
}
extern "C" EGLBoolean eglQuerySurface(EGLDisplay, EGLSurface, EGLint attr, EGLint* result) {
    if (!surface_query_ok) return EGL_FALSE;
    if (attr == EGL_WIDTH) *result = surface_width;
    else if (attr == EGL_HEIGHT) *result = surface_height;
    else { check(false, "unexpected EGL attribute"); return EGL_FALSE; }
    return EGL_TRUE;
}

int main() {
    GLES.glGetIntegerv = get_integer;
    GLES.glGetFramebufferAttachmentParameteriv = get_attachment;
    GLES.glActiveTexture = active_texture;
    GLES.glBindTexture = bind_texture;
    GLES.glGetTexLevelParameteriv = get_texture;
    GLES.glViewport = viewport;
    global_settings.fsr3_setting = FSR3_Mode::Experimental;
    request("native partial viewport remains caller-owned", {16, 24, 640, 360}, false, {16, 24, 640, 360});
    request("native full viewport unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    request("native small scissor remains caller-owned", {100, 200, 80, 20}, false, {100, 200, 80, 20});
    target_width = target_height = 4096;
    request("large atlas partial viewport unchanged", {512, 1024, 512, 512}, false, {512, 1024, 512, 512});
    request("large atlas full viewport unchanged", {0, 0, 4096, 4096}, false, {0, 0, 4096, 4096});
    target_width = target_height = 2048;
    request("square shadow extent unchanged", {0, 0, 2048, 2048}, false, {0, 0, 2048, 2048});
    target_width = 1694; target_height = 932;
    request("Balanced scene scales once", {0, 0, 2880, 1585}, true, {0, 0, 1694, 932});
    request("already scaled scene unchanged", {0, 0, 1694, 932}, false, {0, 0, 1694, 932});
    request("offset scene request unchanged", {1, 2, 2880, 1585}, false, {1, 2, 2880, 1585});
    framebuffer = 0;
    request("default framebuffer unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    framebuffer = 7; attachment_type = GL_RENDERBUFFER;
    request("renderbuffer unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    attachment_type = GL_TEXTURE; attachment = 0;
    request("empty attachment unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    attachment = 13; current_surface = false;
    request("absent EGL surface unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    FSR3NativeViewport();
    check(viewport_calls == 0, "absent surface prevents native viewport reset");
    current_surface = true; surface_query_ok = false;
    request("failed EGL size query unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    surface_query_ok = true;
    global_settings.fsr3_setting = FSR3_Mode::Disabled;
    request("disabled upscaler leaves scene unchanged", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    global_settings.fsr3_setting = FSR3_Mode::Experimental;
    surface_width = 1920; surface_height = 1080;
    target_width = 1280; target_height = 720;
    request("current surface resize used immediately", {0, 0, 1920, 1080}, true, {0, 0, 1280, 720});
    request("stale output extent does not scale", {0, 0, 2880, 1585}, false, {0, 0, 2880, 1585});
    FSR3NativeViewport();
    check(viewport_calls == 1 && driver_viewport == std::array<int, 4>{0, 0, 1920, 1080},
          "explicit final reset sets current native extent");
    std::printf("fsr3_viewport_runtime: %d failures\n", failures);
    return failures ? 1 : 0;
}
