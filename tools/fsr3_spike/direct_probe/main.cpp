// Experimental native FSR3 presentation probe. Drives the library's own
// EGL/GL frontend and eglSwapBuffers, so the code under test is the real
// presentSurface path selected by fsr3Setting=1.
#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <dlfcn.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {
void* lib = nullptr;
struct Api {
    PFNEGLGETDISPLAYPROC eglGetDisplay;
    PFNEGLINITIALIZEPROC eglInitialize;
    PFNEGLBINDAPIPROC eglBindAPI;
    PFNEGLCHOOSECONFIGPROC eglChooseConfig;
    PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface;
    PFNEGLCREATECONTEXTPROC eglCreateContext;
    PFNEGLMAKECURRENTPROC eglMakeCurrent;
    PFNEGLSWAPBUFFERSPROC eglSwapBuffers;
    PFNGLCLEARCOLORPROC glClearColor;
    PFNGLCLEARPROC glClear;
    PFNGLREADPIXELSPROC glReadPixels;
    PFNGLGETERRORPROC glGetError;
    PFNGLGETSTRINGPROC glGetString;
} api{};
template <typename T> T sym(const char* n) {
    void* p = dlsym(lib, n);
    if (!p) { std::fprintf(stderr, "missing %s\n", n); std::exit(2); }
    return reinterpret_cast<T>(p);
}
bool load(const char* path) {
    lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) { std::fprintf(stderr, "dlopen: %s\n", dlerror()); return false; }
    api.eglGetDisplay = sym<PFNEGLGETDISPLAYPROC>("eglGetDisplay");
    api.eglInitialize = sym<PFNEGLINITIALIZEPROC>("eglInitialize");
    api.eglBindAPI = sym<PFNEGLBINDAPIPROC>("eglBindAPI");
    api.eglChooseConfig = sym<PFNEGLCHOOSECONFIGPROC>("eglChooseConfig");
    api.eglCreatePbufferSurface = sym<PFNEGLCREATEPBUFFERSURFACEPROC>("eglCreatePbufferSurface");
    api.eglCreateContext = sym<PFNEGLCREATECONTEXTPROC>("eglCreateContext");
    api.eglMakeCurrent = sym<PFNEGLMAKECURRENTPROC>("eglMakeCurrent");
    api.eglSwapBuffers = sym<PFNEGLSWAPBUFFERSPROC>("eglSwapBuffers");
    api.glClearColor = sym<PFNGLCLEARCOLORPROC>("glClearColor");
    api.glClear = sym<PFNGLCLEARPROC>("glClear");
    api.glReadPixels = sym<PFNGLREADPIXELSPROC>("glReadPixels");
    api.glGetError = sym<PFNGLGETERRORPROC>("glGetError");
    api.glGetString = sym<PFNGLGETSTRINGPROC>("glGetString");
    return true;
}
} // namespace

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "/data/local/tmp/libmobileglues.so";
    if (!load(path)) return 2;
    EGLDisplay dpy = api.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj = 0, min = 0;
    if (dpy == EGL_NO_DISPLAY || !api.eglInitialize(dpy, &maj, &min)) return 1;
    api.eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint cfg_attr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                               EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE};
    EGLConfig cfg = nullptr; EGLint n = 0;
    if (!api.eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) return 1;
    const EGLint pb_attr[] = {EGL_WIDTH, 320, EGL_HEIGHT, 180, EGL_NONE};
    EGLSurface surf = api.eglCreatePbufferSurface(dpy, cfg, pb_attr);
    const EGLint ctx_attr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2, EGL_NONE};
    EGLContext ctx = api.eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
    if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !api.eglMakeCurrent(dpy, surf, surf, ctx)) return 1;

    api.glClearColor(96.0f / 255.0f, 128.0f / 255.0f, 192.0f / 255.0f, 1.0f);
    api.glClear(GL_COLOR_BUFFER_BIT);
    const GLenum pre_err = api.glGetError();
    const bool swapped = api.eglSwapBuffers(dpy, surf) == EGL_TRUE;
    const GLenum post_err = api.glGetError();
    std::vector<unsigned char> pixels(320 * 180 * 4, 0);
    api.glReadPixels(0, 0, 320, 180, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const size_t c = ((180 / 2) * 320 + 160) * 4;
    const int diff = std::abs((int)pixels[c] - 96) + std::abs((int)pixels[c + 1] - 128) + std::abs((int)pixels[c + 2] - 192);
    std::printf("{\"probe\":\"fsr3-direct-present\",\"swapped\":%s,\"pre_error\":%u,\"post_error\":%u,"
                "\"center\":[%u,%u,%u],\"diff\":%d,\"renderer\":\"%s\"}\n",
                swapped ? "true" : "false", pre_err, post_err, pixels[c], pixels[c + 1], pixels[c + 2], diff,
                api.glGetString(GL_RENDERER) ? reinterpret_cast<const char*>(api.glGetString(GL_RENDERER)) : "");
    return swapped && diff <= 12 ? 0 : 1;
}
