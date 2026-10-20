// MobileGlues FSR3 production-backend probe.
//
// Loads the built libmobileglues.so, creates a GLES 3.2 context through the
// library's EGL frontend, calls the public mg_fsr_* ABI, and reads the
// backend-owned output through an FBO. Readback is test-only; the production
// path never uses it.
//
// Usage: backend_probe [libmobileglues.so]
// Output: one JSON object on stdout.

#include <EGL/egl.h>
#include <GLES3/gl32.h>

#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "upscaling/mg_fsr_abi.h"

namespace {

struct Api {
    PFNEGLGETDISPLAYPROC eglGetDisplay;
    PFNEGLINITIALIZEPROC eglInitialize;
    PFNEGLBINDAPIPROC eglBindAPI;
    PFNEGLCHOOSECONFIGPROC eglChooseConfig;
    PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface;
    PFNEGLCREATECONTEXTPROC eglCreateContext;
    PFNEGLMAKECURRENTPROC eglMakeCurrent;
    PFNEGLDESTROYCONTEXTPROC eglDestroyContext;
    PFNEGLDESTROYSURFACEPROC eglDestroySurface;
    PFNEGLTERMINATEPROC eglTerminate;

    PFNGLGENTEXTURESPROC glGenTextures;
    PFNGLBINDTEXTUREPROC glBindTexture;
    PFNGLTEXIMAGE2DPROC glTexImage2D;
    PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;
    PFNGLTEXPARAMETERIPROC glTexParameteri;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
    PFNGLREADPIXELSPROC glReadPixels;
    PFNGLFINISHPROC glFinish;
    PFNGLGETERRORPROC glGetError;
    PFNGLGETSTRINGPROC glGetString;
    PFNGLGETINTEGERVPROC glGetIntegerv;
    PFNGLACTIVETEXTUREPROC glActiveTexture;
    PFNGLVIEWPORTPROC glViewport;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLDELETETEXTURESPROC glDeleteTextures;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;

    void* (*mg_fsr_create_session)();
    void (*mg_fsr_destroy_session)(void*);
    int (*mg_fsr_query_caps)(void*, mg_fsr_caps_t*);
    int (*mg_fsr_dispatch)(void*, const mg_fsr_dispatch_t*);
    int (*mg_fsr_get_output)(void*, mg_fsr_resource_t*);
    const char* (*mg_fsr_last_error)(void*);
};

Api api{};
void* lib = nullptr;

template <typename T>
T sym(const char* name) {
    void* p = dlsym(lib, name);
    if (!p) {
        std::fprintf(stderr, "missing symbol %s: %s\n", name, dlerror());
        std::exit(2);
    }
    return reinterpret_cast<T>(p);
}

bool load(const char* path) {
    lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        std::fprintf(stderr, "dlopen %s: %s\n", path, dlerror());
        return false;
    }
    api.eglGetDisplay = sym<PFNEGLGETDISPLAYPROC>("eglGetDisplay");
    api.eglInitialize = sym<PFNEGLINITIALIZEPROC>("eglInitialize");
    api.eglBindAPI = sym<PFNEGLBINDAPIPROC>("eglBindAPI");
    api.eglChooseConfig = sym<PFNEGLCHOOSECONFIGPROC>("eglChooseConfig");
    api.eglCreatePbufferSurface = sym<PFNEGLCREATEPBUFFERSURFACEPROC>("eglCreatePbufferSurface");
    api.eglCreateContext = sym<PFNEGLCREATECONTEXTPROC>("eglCreateContext");
    api.eglMakeCurrent = sym<PFNEGLMAKECURRENTPROC>("eglMakeCurrent");
    api.eglDestroyContext = sym<PFNEGLDESTROYCONTEXTPROC>("eglDestroyContext");
    api.eglDestroySurface = sym<PFNEGLDESTROYSURFACEPROC>("eglDestroySurface");
    api.eglTerminate = sym<PFNEGLTERMINATEPROC>("eglTerminate");

    api.glGenTextures = sym<PFNGLGENTEXTURESPROC>("glGenTextures");
    api.glBindTexture = sym<PFNGLBINDTEXTUREPROC>("glBindTexture");
    api.glTexImage2D = sym<PFNGLTEXIMAGE2DPROC>("glTexImage2D");
    api.glTexSubImage2D = sym<PFNGLTEXSUBIMAGE2DPROC>("glTexSubImage2D");
    api.glTexParameteri = sym<PFNGLTEXPARAMETERIPROC>("glTexParameteri");
    api.glGenFramebuffers = sym<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    api.glBindFramebuffer = sym<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    api.glFramebufferTexture2D = sym<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    api.glCheckFramebufferStatus = sym<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus");
    api.glReadPixels = sym<PFNGLREADPIXELSPROC>("glReadPixels");
    api.glFinish = sym<PFNGLFINISHPROC>("glFinish");
    api.glGetError = sym<PFNGLGETERRORPROC>("glGetError");
    api.glGetString = sym<PFNGLGETSTRINGPROC>("glGetString");
    api.glGetIntegerv = sym<PFNGLGETINTEGERVPROC>("glGetIntegerv");
    api.glActiveTexture = sym<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    api.glViewport = sym<PFNGLVIEWPORTPROC>("glViewport");
    api.glGenBuffers = sym<PFNGLGENBUFFERSPROC>("glGenBuffers");
    api.glBindBuffer = sym<PFNGLBINDBUFFERPROC>("glBindBuffer");
    api.glBufferData = sym<PFNGLBUFFERDATAPROC>("glBufferData");
    api.glDeleteBuffers = sym<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
    api.glDeleteTextures = sym<PFNGLDELETETEXTURESPROC>("glDeleteTextures");
    api.glDeleteFramebuffers = sym<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");

    api.mg_fsr_create_session = sym<void* (*)()>("mg_fsr_create_session");
    api.mg_fsr_destroy_session = sym<void (*)(void*)>("mg_fsr_destroy_session");
    api.mg_fsr_query_caps = sym<int (*)(void*, mg_fsr_caps_t*)>("mg_fsr_query_caps");
    api.mg_fsr_dispatch = sym<int (*)(void*, const mg_fsr_dispatch_t*)>("mg_fsr_dispatch");
    api.mg_fsr_get_output = sym<int (*)(void*, mg_fsr_resource_t*)>("mg_fsr_get_output");
    api.mg_fsr_last_error = sym<const char* (*)(void*)>("mg_fsr_last_error");
    return true;
}

GLuint make_tex(GLenum internal, int w, int h, GLenum format, GLenum type, const void* data) {
    GLuint t = 0;
    api.glGenTextures(1, &t);
    api.glBindTexture(GL_TEXTURE_2D, t);
    api.glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal), w, h, 0, format, type, data);
    api.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    api.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    api.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    api.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    api.glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

void add_checks(std::string& out, const std::vector<std::pair<std::string, std::string>>& checks) {
    out += "\"checks\":[";
    for (size_t i = 0; i < checks.size(); ++i) {
        if (i) out += ",";
        out += "{\"name\":\"" + checks[i].first + "\",\"status\":\"" + checks[i].second + "\"}";
    }
    out += "]";
}

} // namespace

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/data/local/tmp/libmobileglues.so";
    int rw = 160, rh = 90, ow = 320, oh = 180;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--render") == 0 && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%dx%d", &rw, &rh) != 2) return 2;
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            if (std::sscanf(argv[++i], "%dx%d", &ow, &oh) != 2) return 2;
        }
    }
    if (rw < 64 || rh < 64 || ow < rw || oh < rh || ow > 4096 || oh > 4096) return 2;
    if (!load(path)) return 2;

    std::fprintf(stderr, "probe: egl display\n");
    EGLDisplay display = api.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint major = 0, minor = 0;
    if (display == EGL_NO_DISPLAY || !api.eglInitialize(display, &major, &minor)) {
        std::printf("{\"error\":\"eglInitialize\"}\n");
        return 1;
    }
    api.eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint count = 0;
    if (!api.eglChooseConfig(display, cfg_attribs, &config, 1, &count) || count < 1) {
        std::printf("{\"error\":\"eglChooseConfig\"}\n");
        return 1;
    }
    const EGLint pb_attribs[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    EGLSurface surface = api.eglCreatePbufferSurface(display, config, pb_attribs);
    const EGLint ctx_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    EGLContext context = api.eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT ||
        !api.eglMakeCurrent(display, surface, surface, context)) {
        std::printf("{\"error\":\"eglMakeCurrent\"}\n");
        return 1;
    }

    std::fprintf(stderr, "probe: context current\n");
    std::vector<uint8_t> color(static_cast<size_t>(rw) * rh * 4);
    for (size_t i = 0; i < color.size(); i += 4) {
        color[i + 0] = 96;
        color[i + 1] = 128;
        color[i + 2] = 192;
        color[i + 3] = 255;
    }
    std::vector<float> depth(static_cast<size_t>(rw) * rh, 0.5f);
    std::vector<float> motion(static_cast<size_t>(rw) * rh * 2, 0.0f);

    GLuint color_tex = make_tex(GL_RGBA8, rw, rh, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
    GLuint depth_tex = make_tex(GL_R32F, rw, rh, GL_RED, GL_FLOAT, depth.data());
    GLuint motion_tex = make_tex(GL_RG16F, rw, rh, GL_RG, GL_FLOAT, motion.data());

    std::vector<std::pair<std::string, std::string>> checks;
    checks.emplace_back("context", "PASS");

    std::fprintf(stderr, "probe: inputs created\n");
    void* session = api.mg_fsr_create_session();
    mg_fsr_caps_t caps{};
    caps.abi_version = MG_FSR_ABI_VERSION;
    caps.struct_size = sizeof(caps);
    std::fprintf(stderr, "probe: session created\n");
    const int caps_rc = api.mg_fsr_query_caps(session, &caps);
    std::fprintf(stderr, "probe: caps rc=%d mask=%x\n", caps_rc, caps.mode_mask);
    checks.emplace_back("caps", caps_rc == 0 && (caps.mode_mask & (1u << MG_FSR_MODE_FSR3_TEMPORAL)) ? "PASS" : "FAIL");

    mg_fsr_dispatch_t desc{};
    desc.abi_version = MG_FSR_ABI_VERSION;
    desc.struct_size = sizeof(desc);
    desc.frame_id = 1;
    desc.session_id = 1;
    desc.context_id = 1;
    desc.surface_id = 1;
    desc.resource_generation = 1;
    desc.color = {color_tex, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RGBA8, 0, 1};
    desc.depth = {depth_tex, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_R32F, 0, 1};
    desc.motion = {motion_tex, MG_FSR_NS_GL_FRONTEND, GL_TEXTURE_2D, 0, 0, GL_RG16F, 0, 1};
    desc.render_width = rw;
    desc.render_height = rh;
    desc.output_width = ow;
    desc.output_height = oh;
    desc.color_active = {0, 0, static_cast<uint32_t>(rw), static_cast<uint32_t>(rh)};
    desc.output_active = {0, 0, static_cast<uint32_t>(ow), static_cast<uint32_t>(oh)};
    desc.color_space = MG_FSR_COLOR_LDR_PERCEPTUAL;
    desc.output_alpha_mode = 0;
    desc.pre_exposure = 1.0f;
    desc.depth_valid = 1;
    desc.camera_near = 0.1f;
    desc.camera_far = 1000.0f;
    desc.camera_fov_y_radians = 1.0471975511965976f;
    desc.jitter_x = 0.25f;
    desc.jitter_y = -0.25f;
    desc.motion_valid = 1;
    desc.motion_confidence = MG_FSR_MOTION_CAMERA_ONLY;
    desc.frame_delta_seconds = 1.0f / 60.0f;
    desc.reset_history = 1;
    desc.reset_reason = MG_FSR_RESET_SESSION;

    // Application state that backend initialization/dispatch must preserve.
    GLuint app_fbo = 0, app_ubo = 0;
    api.glGenFramebuffers(1, &app_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, app_fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
    api.glGenBuffers(1, &app_ubo);
    api.glBindBuffer(GL_UNIFORM_BUFFER, app_ubo);
    const float ubo_seed[4] = {1, 2, 3, 4};
    api.glBufferData(GL_UNIFORM_BUFFER, sizeof(ubo_seed), ubo_seed, GL_STATIC_DRAW);
    api.glActiveTexture(GL_TEXTURE0);
    api.glBindTexture(GL_TEXTURE_2D, color_tex);
    api.glViewport(1, 2, 17, 19);

    std::fprintf(stderr, "probe: dispatch1 start\n");
    const int dispatch_rc = api.mg_fsr_dispatch(session, &desc);
    std::fprintf(stderr, "probe: dispatch1 rc=%d\n", dispatch_rc);
    checks.emplace_back("dispatch1", dispatch_rc == 0 ? "PASS" : "FAIL");
    GLint state_tex = 0, state_ubo = 0, state_fbo = 0, state_viewport[4]{};
    api.glGetIntegerv(GL_TEXTURE_BINDING_2D, &state_tex);
    api.glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &state_ubo);
    api.glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state_fbo);
    api.glGetIntegerv(GL_VIEWPORT, state_viewport);
    checks.emplace_back("state_texture", state_tex == static_cast<GLint>(color_tex) ? "PASS" : "FAIL");
    checks.emplace_back("state_ubo", state_ubo == static_cast<GLint>(app_ubo) ? "PASS" : "FAIL");
    checks.emplace_back("state_fbo", state_fbo == static_cast<GLint>(app_fbo) ? "PASS" : "FAIL");
    checks.emplace_back("state_viewport", state_viewport[0] == 1 && state_viewport[1] == 2 &&
                        state_viewport[2] == 17 && state_viewport[3] == 19 ? "PASS" : "FAIL");
    if (dispatch_rc != 0) {
        std::printf("{\"error\":\"dispatch1\",\"message\":\"%s\"}\n", api.mg_fsr_last_error(session));
        return 1;
    }
    mg_fsr_resource_t out{};
    const int out_rc = api.mg_fsr_get_output(session, &out);
    checks.emplace_back("output", out_rc == 0 && out.handle != 0 ? "PASS" : "FAIL");

    GLuint fbo = 0;
    api.glGenFramebuffers(1, &fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, static_cast<GLuint>(out.handle), 0);
    checks.emplace_back("output_fbo", api.glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE ? "PASS" : "FAIL");
    std::vector<uint8_t> pixels(static_cast<size_t>(ow) * oh * 4, 0);
    api.glReadPixels(0, 0, ow, oh, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    api.glFinish();
    const size_t center = (static_cast<size_t>(oh / 2) * ow + ow / 2) * 4;
    const int diff = std::abs(static_cast<int>(pixels[center]) - 96) +
                     std::abs(static_cast<int>(pixels[center + 1]) - 128) +
                     std::abs(static_cast<int>(pixels[center + 2]) - 192);
    checks.emplace_back("flat_output", diff <= 9 ? "PASS" : "FAIL");
    checks.emplace_back("gl_error", api.glGetError() == GL_NO_ERROR ? "PASS" : "FAIL");

    // Second frame: no reset, changed color. The backend must accept history.
    for (size_t i = 0; i < color.size(); i += 4) color[i + 0] = 120;
    api.glBindTexture(GL_TEXTURE_2D, color_tex);
    api.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, rw, rh, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
    desc.frame_id = 2;
    desc.reset_history = 0;
    desc.jitter_x = -0.25f;
    desc.jitter_y = 0.25f;
    const int dispatch2 = api.mg_fsr_dispatch(session, &desc);
    checks.emplace_back("dispatch2_history", dispatch2 == 0 ? "PASS" : "FAIL");
    if (dispatch2 == 0) {
        api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        api.glReadPixels(0, 0, ow, oh, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        const int diff2 = std::abs(static_cast<int>(pixels[center]) - 120) +
                          std::abs(static_cast<int>(pixels[center + 1]) - 128) +
                          std::abs(static_cast<int>(pixels[center + 2]) - 192);
        checks.emplace_back("flat_output2", diff2 <= 12 ? "PASS" : "FAIL");
    }

    desc.frame_id = 3;
    desc.reset_history = 1;
    const int dispatch3 = api.mg_fsr_dispatch(session, &desc);
    checks.emplace_back("dispatch3_reset", dispatch3 == 0 ? "PASS" : "FAIL");

    std::string json = "{\"probe\":\"fsr3-backend\",\"renderer\":\"";
    json += "loaded\","; // keep the JSON valid without a GL string dependency
    add_checks(json, checks);
    json += ",\"gl_renderer\":\"" + std::string(reinterpret_cast<const char*>(api.glGetString(GL_RENDERER))) + "\"}";
    std::printf("%s\n", json.c_str());

    api.mg_fsr_destroy_session(session);
    api.glDeleteFramebuffers(1, &fbo);
    api.glDeleteFramebuffers(1, &app_fbo);
    api.glDeleteBuffers(1, &app_ubo);
    api.glDeleteTextures(1, &color_tex);
    api.glDeleteTextures(1, &depth_tex);
    api.glDeleteTextures(1, &motion_tex);
    api.eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    api.eglDestroyContext(display, context);
    api.eglDestroySurface(display, surface);
    api.eglTerminate(display);
    return 0;
}
