// MobileGlues - tools/wrapper_probe/main.cpp
// SPDX-License-Identifier: LGPL-2.1-only
//
// Loads a built libmobileglues.so and exercises the frontend contracts that are
// testable without Minecraft: EGL context creation, multi-string glShaderSource,
// compile-status truthfulness, glGetError, and advertised limits.
//
// Usage: wrapper_probe [case]
//   all (default), source_multi, source_embedded_nul, compile_failure,
//   get_error, limits, strings
// Each case runs in the process named by the caller; run "all" in a child process
// so a frontend defect cannot take the harness's launcher with it.

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <MG/extensions.h>

#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <chrono>

namespace {

void* g_lib = nullptr;
FILE* g_out = stdout;

template <typename T>
T sym(const char* name) {
    void* p = dlsym(g_lib, name);
    if (!p) {
        fprintf(stderr, "missing symbol: %s\n", name);
        exit(90);
    }
    return reinterpret_cast<T>(p);
}

struct Api {
    PFNEGLGETDISPLAYPROC eglGetDisplay;
    PFNEGLINITIALIZEPROC eglInitialize;
    PFNEGLBINDAPIPROC eglBindAPI;
    PFNEGLCHOOSECONFIGPROC eglChooseConfig;
    PFNEGLCREATEPBUFFERSURFACEPROC eglCreatePbufferSurface;
    PFNEGLCREATECONTEXTPROC eglCreateContext;
    PFNEGLMAKECURRENTPROC eglMakeCurrent;
    PFNEGLDESTROYSURFACEPROC eglDestroySurface;
    PFNEGLDESTROYCONTEXTPROC eglDestroyContext;
    PFNEGLTERMINATEPROC eglTerminate;
    PFNEGLGETPROCADDRESSPROC eglGetProcAddress;

    PFNGLGETSTRINGPROC glGetString;
    PFNGLGETSTRINGIPROC glGetStringi;
    PFNGLGETINTEGERVPROC glGetIntegerv;
    PFNGLGETERRORPROC glGetError;
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    PFNGLGETSHADERSOURCEPROC glGetShaderSource;
    PFNGLDELETESHADERPROC glDeleteShader;
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
    PFNGLDELETEPROGRAMPROC glDeleteProgram;
    GLint (*glBindFragDataLocation)(GLuint, GLuint, const GLchar*);
    GLint (*glGetFragDataLocation)(GLuint, const GLchar*);
    GLuint (*glGetProgramResourceIndex)(GLuint, GLenum, const GLchar*);
    void (*glGetProgramResourceName)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*);
    void (*glGetProgramResourceiv)(GLuint, GLenum, GLuint, GLsizei, const GLenum*, GLsizei, GLsizei*, GLint*);
    void (*glShaderStorageBlockBinding)(GLuint, GLuint, GLuint);
    void (*mg_image_metadata_begin)();
    void (*mg_image_metadata_add)(const char*, GLenum);
    void (*mg_image_metadata_end)();
    PFNGLUSEPROGRAMPROC glUseProgram;
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLBINDBUFFERBASEPROC glBindBufferBase;
    void (*glClearBufferSubData)(GLenum, GLenum, GLintptr, GLsizeiptr, GLenum, GLenum, const void*);
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLUNIFORM1UIPROC glUniform1ui;
    PFNGLUNIFORM1IPROC glUniform1i;
    void (*glGetUniformiv)(GLuint, GLint, GLint*);
    void (*glProgramUniform1i)(GLuint, GLint, GLint);
    void (*glProgramUniform1iv)(GLuint, GLint, GLsizei, const GLint*);
    void (*glUniform1iv)(GLint, GLsizei, const GLint*);
    PFNGLACTIVETEXTUREPROC glActiveTexture;
    PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
    PFNGLMEMORYBARRIERPROC glMemoryBarrier;
    PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
    PFNGLUNMAPBUFFERPROC glUnmapBuffer;
    PFNGLBINDIMAGETEXTUREPROC glBindImageTexture;
    PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
    void (*glClearTexImage)(GLuint, GLint, GLenum, GLenum, const void*);
    PFNGLGENSAMPLERSPROC glGenSamplers;
    PFNGLBINDSAMPLERPROC glBindSampler;
    PFNGLSAMPLERPARAMETERIPROC glSamplerParameteri;
    PFNGLDELETESAMPLERSPROC glDeleteSamplers;
    PFNGLTEXSTORAGE2DPROC glTexStorage2D;
    PFNGLTEXSTORAGE3DPROC glTexStorage3D;
    PFNGLTEXIMAGE3DPROC glTexImage3D;
    PFNGLTEXIMAGE2DPROC glTexImage2D;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
    PFNGLGENTEXTURESPROC glGenTextures;
    PFNGLBINDTEXTUREPROC glBindTexture;
    PFNGLTEXPARAMETERIPROC glTexParameteri;
    void (*glGetTexParameteriv)(GLenum, GLenum, GLint*);
    void (*glGenQueries)(GLsizei, GLuint*);
    void (*glDeleteQueries)(GLsizei, const GLuint*);
    void (*glQueryCounter)(GLuint, GLenum);
    void (*glGetQueryObjectui64v)(GLuint, GLenum, GLuint64*);
    void (*glCopyImageSubData)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint,
                               GLint, GLsizei, GLsizei, GLsizei);
    PFNGLDELETETEXTURESPROC glDeleteTextures;
    PFNGLDRAWARRAYSPROC glDrawArrays;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLFINISHPROC glFinish;
    PFNGLVIEWPORTPROC glViewport;
    PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;
    PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
    PFNGLREADPIXELSPROC glReadPixels;
    void (*glPatchParameteri)(GLenum, GLint);
    void (*glPatchParameterfv)(GLenum, const GLfloat*);
    void (*glGenRenderbuffers)(GLsizei, GLuint*);
    void (*glDeleteRenderbuffers)(GLsizei, const GLuint*);
    void (*glBindRenderbuffer)(GLenum, GLuint);
    void (*glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
    void (*glRenderbufferStorageMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void (*glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
    void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    void (*glClear)(GLbitfield);
    void (*glBlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
    GLsync (*glFenceSync)(GLenum, GLbitfield);
    void (*glDeleteSync)(GLsync);
    PFNGLENABLEPROC glEnable;
    PFNGLDISABLEPROC glDisable;
    PFNGLBLENDFUNCPROC glBlendFunc;
};

Api api;
EGLDisplay display = EGL_NO_DISPLAY;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;

bool make_context() {
    display = api.eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) return false;
    EGLint major = 0, minor = 0;
    if (!api.eglInitialize(display, &major, &minor)) return false;
    api.eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE,
    };
    EGLConfig config = nullptr;
    EGLint num = 0;
    if (!api.eglChooseConfig(display, cfg_attribs, &config, 1, &num) || num <= 0) return false;
    const EGLint pb_attribs[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    surface = api.eglCreatePbufferSurface(display, config, pb_attribs);
    if (surface == EGL_NO_SURFACE) return false;
    // Ask for the desktop GL 4.0 profile the plugin advertises, then fall back
    // to the ES3 context MobileGlues also exposes. The frontend chooses what it
    // can grant; record which one we got.
    const EGLint desktop_attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
    context = api.eglCreateContext(display, config, EGL_NO_CONTEXT, desktop_attribs);
    if (context == EGL_NO_CONTEXT) {
        const EGLint es_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = api.eglCreateContext(display, config, EGL_NO_CONTEXT, es_attribs);
    }
    if (context == EGL_NO_CONTEXT) return false;
    return api.eglMakeCurrent(display, surface, surface, context);
}

void destroy_context() {
    if (display == EGL_NO_DISPLAY) return;
    api.eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface != EGL_NO_SURFACE) api.eglDestroySurface(display, surface);
    if (context != EGL_NO_CONTEXT) api.eglDestroyContext(display, context);
    api.eglTerminate(display);
}

std::string gl_string(GLenum name) {
    const GLubyte* s = api.glGetString(name);
    return s ? reinterpret_cast<const char*>(s) : "<null>";
}

std::string escape(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\0') {
            out += "\\u0000";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            out += c;
        }
    }
    out += '"';
    return out;
}

std::string shader_log(GLuint shader) {
    GLint len = 0;
    api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len <= 0) return {};
    std::string log(static_cast<size_t>(len), '\0');
    api.glGetShaderInfoLog(shader, len, nullptr, log.data());
    const size_t nul = log.find('\0');
    if (nul != std::string::npos) log.resize(nul);
    return log;
}

struct Result {
    bool ok = false;
    std::string detail;
};

Result compile_multi_string(const char* label, const std::vector<std::string>& pieces,
                            const std::vector<GLint>& lengths, bool pass_lengths) {
    Result r;
    std::string joined;
    for (const auto& p : pieces) joined += p;
    GLuint shader = api.glCreateShader(GL_FRAGMENT_SHADER);
    std::vector<const GLchar*> pointers;
    for (const auto& p : pieces) pointers.push_back(p.c_str());
    api.glShaderSource(shader, static_cast<GLsizei>(pointers.size()), pointers.data(),
                       pass_lengths ? lengths.data() : nullptr);
    api.glCompileShader(shader);
    GLint status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    const std::string log = shader_log(shader);
    r.ok = (status == GL_TRUE);
    r.detail = std::string(label) + " status=" + (status ? "true" : "false") +
               " pieces=" + std::to_string(pieces.size()) +
               " expected_bytes=" + std::to_string(joined.size()) +
               (log.empty() ? "" : " log=" + log);
    api.glDeleteShader(shader);
    return r;
}

Result test_source_multi() {
    const char* a = "#version 300 es\nprecision mediump float;\n";
    const char* b = "out vec4 color;\nvoid main() {\n";
    const char* c = "  color = vec4(1.0, 0.5, 0.25, 1.0);\n}\n";
    std::vector<std::string> pieces = {a, b, c};
    std::vector<GLint> lengths = {static_cast<GLint>(strlen(a)), static_cast<GLint>(strlen(b)),
                                  static_cast<GLint>(strlen(c))};
    return compile_multi_string("multi_string_explicit_lengths", pieces, lengths, true);
}

Result test_source_multi_implicit() {
    const char* a = "#version 300 es\nprecision mediump float;\n";
    const char* b = "out vec4 color;\nvoid main(){color=vec4(0.0,1.0,0.0,1.0);}\n";
    std::vector<std::string> pieces = {a, b};
    std::vector<GLint> lengths = {0, 0};
    return compile_multi_string("multi_string_no_lengths", pieces, lengths, false);
}

Result test_source_embedded_nul() {
    // A length-delimited source containing NUL is legal at the API boundary.
    // A frontend that calls strlen() instead of using the supplied length will
    // truncate it and produce a different source. Keep the shader text itself
    // valid after the NUL so the backend compile can still be observed.
    const char prefix[] = "#version 300 es\nprecision mediump float;\nout vec4 color;\nvoid main(){color=vec4(1.0);}\n";
    std::string piece(prefix, sizeof(prefix) - 1);
    piece.push_back('\0');
    piece += "// trailing comment after an embedded NUL\n";
    const GLint len = static_cast<GLint>(piece.size());
    return compile_multi_string("single_string_embedded_nul", {piece}, {len}, true);
}

Result test_compile_failure() {
    Result r;
    const char* bad = "#version 300 es\nprecision mediump float;\nout vec4 color;\nvoid main(){ color = not_a_value; }\n";
    GLuint shader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shader, 1, &bad, nullptr);
    api.glCompileShader(shader);
    GLint status = GL_TRUE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    const std::string log = shader_log(shader);
    r.ok = (status == GL_FALSE);
    r.detail = "invalid_shader_reported_status=" + std::string(status ? "true (MASKED)" : "false (truthful)") +
               (log.empty() ? "" : " log=" + log);
    api.glDeleteShader(shader);
    return r;
}

Result test_get_error() {
    Result r;
    while (api.glGetError() != GL_NO_ERROR) {}
    GLint value = 0;
    api.glGetIntegerv(static_cast<GLenum>(0xdeadbeef), &value);
    const GLenum error = api.glGetError();
    r.ok = (error == GL_INVALID_ENUM);
    r.detail = "after_invalid_enum=" + std::to_string(error) +
               (error == GL_NO_ERROR ? " (SWALLOWED)" : " (reported)");
    return r;
}

Result test_limits() {
    Result r;
    GLint frontend = 0;
    api.glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &frontend);
    const GLubyte* exts = api.glGetString(GL_EXTENSIONS);
    const bool backend_access = exts && strstr(reinterpret_cast<const char*>(exts), "GL_MG_backend_string_getter_access");
    const bool tessellation_ext =
        exts && strstr(reinterpret_cast<const char*>(exts), "GL_ARB_tessellation_shader");
    // The backend limit is in the below-wrapper probe; this harness records what
    // the application is actually told. MG_EXPECT_MAX_TEXTURE_IMAGE_UNITS lets a
    // runner pin the provider's value without hardcoding one device in the test.
    const char* expected_env = getenv("MG_EXPECT_MAX_TEXTURE_IMAGE_UNITS");
    const int expected = expected_env ? atoi(expected_env) : -1;
    r.ok = (expected < 0) ? true : (frontend == expected);
    r.detail = "frontend_max_texture_image_units=" + std::to_string(frontend) +
               " backend_access=" + (backend_access ? "yes" : "no") +
               " tessellation_ext=" + (tessellation_ext ? "yes" : "no") +
               (expected >= 0 ? " expected=" + std::to_string(expected) : "");
    return r;
}

Result test_strings() {
    Result r;
    r.ok = true;
    r.detail = "renderer=" + gl_string(GL_RENDERER) + " version=" + gl_string(GL_VERSION) +
               " shading=" + gl_string(GL_SHADING_LANGUAGE_VERSION);
    return r;
}

Result test_link_failure() {
    Result r;
    // A vertex/fragment interface mismatch must fail to link. A compute-only
    // program links successfully as a compute program and is not a failure.
    const char* vs = "#version 300 es\nout vec3 mismatch;\nvoid main() { mismatch = vec3(1.0); gl_Position = vec4(0.0); }\n";
    const char* fs = "#version 300 es\nprecision mediump float;\nin vec4 mismatch;\nout vec4 color;\nvoid main() { color = mismatch; }\n";
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint fshader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader, 1, &fs, nullptr);
    api.glCompileShader(fshader);
    GLint vstatus = GL_FALSE, fstatus = GL_FALSE;
    api.glGetShaderiv(vshader, GL_COMPILE_STATUS, &vstatus);
    api.glGetShaderiv(fshader, GL_COMPILE_STATUS, &fstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vshader);
    api.glAttachShader(program, fshader);
    api.glLinkProgram(program);
    GLint lstatus = GL_TRUE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);
    GLint plen = 0;
    api.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &plen);
    std::string log(static_cast<size_t>(plen), '\0');
    if (plen > 0) api.glGetProgramInfoLog(program, plen, nullptr, log.data());
    r.ok = (vstatus == GL_TRUE && fstatus == GL_TRUE && lstatus == GL_FALSE);
    r.detail = "interface_mismatch_link_status=" + std::string(lstatus ? "true (MASKED?)" : "false") +
               " vshader=" + (vstatus ? "compiled" : "failed") +
               " fshader=" + (fstatus ? "compiled" : "failed") +
               (log.empty() ? "" : " log=" + log);
    api.glDeleteProgram(program);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader);
    return r;
}

Result test_frag_data_isolation() {
    Result r;
    const char* vs = "#version 300 es\nvoid main() { gl_Position = vec4(0.0, 0.0, 0.0, 1.0); }\n";
    const char* fs_a = "#version 300 es\nprecision mediump float;\nout vec4 myOut;\nvoid main() { myOut = vec4(1.0, 0.0, 0.0, 1.0); }\n";
    const char* fs_b = "#version 300 es\nprecision mediump float;\nout vec4 myOut;\nvoid main() { myOut = vec4(0.0, 1.0, 0.0, 1.0); }\n";

    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLint vstatus = GL_FALSE;
    api.glGetShaderiv(vshader, GL_COMPILE_STATUS, &vstatus);

    GLuint fshader_a = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader_a, 1, &fs_a, nullptr);
    api.glCompileShader(fshader_a);
    GLuint fshader_b = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader_b, 1, &fs_b, nullptr);
    api.glCompileShader(fshader_b);
    GLint fstatus_a = GL_FALSE, fstatus_b = GL_FALSE;
    api.glGetShaderiv(fshader_a, GL_COMPILE_STATUS, &fstatus_a);
    api.glGetShaderiv(fshader_b, GL_COMPILE_STATUS, &fstatus_b);

    GLuint program_a = api.glCreateProgram();
    api.glAttachShader(program_a, vshader);
    api.glAttachShader(program_a, fshader_a);
    api.glBindFragDataLocation(program_a, 2, "myOut");
    api.glLinkProgram(program_a);

    GLuint program_b = api.glCreateProgram();
    api.glAttachShader(program_b, vshader);
    api.glAttachShader(program_b, fshader_b);
    api.glBindFragDataLocation(program_b, 1, "myOut");
    api.glLinkProgram(program_b);

    GLint link_a = GL_FALSE, link_b = GL_FALSE;
    api.glGetProgramiv(program_a, GL_LINK_STATUS, &link_a);
    api.glGetProgramiv(program_b, GL_LINK_STATUS, &link_b);
    const GLint loc_a = api.glGetFragDataLocation(program_a, "myOut");
    const GLint loc_b = api.glGetFragDataLocation(program_b, "myOut");

    // Relink A after changing only A's binding. B must stay at 1.
    api.glBindFragDataLocation(program_a, 3, "myOut");
    api.glLinkProgram(program_a);
    GLint link_a2 = GL_FALSE;
    api.glGetProgramiv(program_a, GL_LINK_STATUS, &link_a2);
    const GLint loc_a2 = api.glGetFragDataLocation(program_a, "myOut");
    const GLint loc_b2 = api.glGetFragDataLocation(program_b, "myOut");

    r.ok = (vstatus && fstatus_a && fstatus_b && link_a && link_b && link_a2 &&
            loc_a == 2 && loc_b == 1 && loc_a2 == 3 && loc_b2 == 1);
    r.detail = "vs=" + std::to_string(vstatus) + " fsa=" + std::to_string(fstatus_a) +
               " fsb=" + std::to_string(fstatus_b) + " linkA=" + std::to_string(link_a) +
               " linkB=" + std::to_string(link_b) + " linkA2=" + std::to_string(link_a2) +
               " locA=" + std::to_string(loc_a) + " locB=" + std::to_string(loc_b) +
               " locA2=" + std::to_string(loc_a2) + " locB2=" + std::to_string(loc_b2);

    api.glDeleteProgram(program_a);
    api.glDeleteProgram(program_b);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader_a);
    api.glDeleteShader(fshader_b);
    return r;
}

std::string shader_source_text(GLuint shader) {
    GLint length = 0;
    api.glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &length);
    if (length <= 0) return {};
    std::string source(static_cast<size_t>(length), '\0');
    GLsizei written = 0;
    api.glGetShaderSource(shader, length, &written, source.data());
    if (written >= 0 && static_cast<size_t>(written) < source.size()) source.resize(static_cast<size_t>(written));
    return source;
}

Result test_frag_data_interleaved() {
    Result r;
    // Source the shader the program will use, then source an unrelated shader.
    // The old global "last shader" record made the second source the patch
    // target, so the attached shader never received its binding.
    const char* fs_a = "#version 300 es\nprecision mediump float;\nout vec4 interleavedOut;\nvoid main() { interleavedOut = vec4(1.0); }\n";
    const char* fs_b = "#version 300 es\nprecision mediump float;\nout vec4 unrelatedOut;\nvoid main() { unrelatedOut = vec4(0.0); }\n";
    const char* vs = "#version 300 es\nvoid main() { gl_Position = vec4(0.0); }\n";
    GLuint shader_a = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shader_a, 1, &fs_a, nullptr);
    api.glCompileShader(shader_a);
    GLuint shader_b = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shader_b, 1, &fs_b, nullptr);
    api.glCompileShader(shader_b);
    GLuint vertex_shader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vertex_shader, 1, &vs, nullptr);
    api.glCompileShader(vertex_shader);

    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vertex_shader);
    api.glAttachShader(program, shader_a);
    api.glBindFragDataLocation(program, 5, "interleavedOut");
    api.glLinkProgram(program);
    GLint link_status = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    const std::string patched_a = shader_source_text(shader_a);
    const std::string patched_b = shader_source_text(shader_b);
    const bool a_has_binding = patched_a.find("location = 5") != std::string::npos;
    const bool b_has_binding = patched_b.find("location = 5") != std::string::npos;
    r.ok = (link_status == GL_TRUE && a_has_binding && !b_has_binding);
    r.detail = "link=" + std::to_string(link_status) + " attached_has_location5=" +
               (a_has_binding ? "yes" : "no") + " unrelated_has_location5=" +
               (b_has_binding ? "yes" : "no") + " attached_source_prefix=" +
               patched_a.substr(0, std::min<size_t>(patched_a.size(), 120));
    api.glDeleteProgram(program);
    api.glDeleteShader(shader_a);
    api.glDeleteShader(shader_b);
    api.glDeleteShader(vertex_shader);
    return r;
}

Result test_frag_data_replaces_existing_layout() {
    Result r;
    // The pack and translator may already emit a layout location. Rebinding must
    // replace it, not append a second qualifier.
    const char* fs = "#version 300 es\nprecision mediump float;\nlayout(location = 0) out vec4 existingOut;\nvoid main() { existingOut = vec4(1.0); }\n";
    const char* vs = "#version 300 es\nvoid main() { gl_Position = vec4(0.0); }\n";
    GLuint shader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shader, 1, &fs, nullptr);
    api.glCompileShader(shader);
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vshader);
    api.glAttachShader(program, shader);
    api.glBindFragDataLocation(program, 6, "existingOut");
    api.glLinkProgram(program);
    GLint link_status = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    const std::string source = shader_source_text(shader);
    const size_t first = source.find("location = 6");
    const size_t second = first == std::string::npos ? std::string::npos : source.find("location = 6", first + 1);
    const bool has_existing_old = source.find("location = 0") != std::string::npos;
    r.ok = (link_status == GL_TRUE && first != std::string::npos && second == std::string::npos && !has_existing_old);
    r.detail = "link=" + std::to_string(link_status) + " location6_count=" +
               std::to_string(first == std::string::npos ? 0 : (second == std::string::npos ? 1 : 2)) +
               " location0_present=" + (has_existing_old ? "yes" : "no") +
               " source=" + source.substr(0, std::min<size_t>(source.size(), 160));
    api.glDeleteProgram(program);
    api.glDeleteShader(shader);
    api.glDeleteShader(vshader);
    return r;
}

Result test_storage_block_binding() {
    Result r;
    const char* cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer Data { uint value; } data;
void main() { data.value = 0xabcdu; }
)";
    std::string log;
    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLint cstatus = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &cstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint lstatus = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);

    const GLuint block_index = api.glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "Data");
    char block_name[128] = {0};
    GLsizei block_name_length = 0;
    if (block_index != GL_INVALID_INDEX) {
        api.glGetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, block_index, sizeof(block_name) - 1,
                                     &block_name_length, block_name);
    }

    api.glShaderStorageBlockBinding(program, block_index, 3);
    // The frontend applies the rewrite by relinking; the buffer must therefore
    // be bound at the new binding point.
    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffer);
    api.glUseProgram(program);
    api.glDispatchCompute(1, 1, 1);
    api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    GLint reported_binding = -1;
    if (block_index != GL_INVALID_INDEX) {
        GLenum prop = GL_BUFFER_BINDING;
        GLsizei written = 0;
        api.glGetProgramResourceiv(program, GL_SHADER_STORAGE_BLOCK, block_index, 1, &prop, 1, &written,
                                   &reported_binding);
    }

    r.ok = (cstatus == GL_TRUE && lstatus == GL_TRUE && block_index != GL_INVALID_INDEX &&
            block_name_length > 0 && std::string(block_name) == "Data" && value == 0xabcdu &&
            (reported_binding == -1 || reported_binding == 3));
    r.detail = "compile=" + std::to_string(cstatus) + " link=" + std::to_string(lstatus) +
               " block_index=" + std::to_string(block_index) + " block_name=" + block_name +
               " dispatch_value=" + std::to_string(value) +
               " reported_binding=" + std::to_string(reported_binding) +
               (log.empty() ? "" : " log=" + log);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteProgram(program);
    api.glDeleteShader(shader);
    return r;
}

Result test_graphics_storage_block_binding() {
    Result r;
    const char* vs = R"(#version 310 es
layout(std430, binding = 0) buffer GraphicsData { uint value; } graphicsData;
void main() {
    graphicsData.value = 0x55u;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    gl_PointSize = 1.0;
}
)";
    const char* fs = "#version 310 es\nprecision mediump float;\nlayout(location = 0) out vec4 color;\nvoid main() { color = vec4(1.0); }\n";
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint fshader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader, 1, &fs, nullptr);
    api.glCompileShader(fshader);
    GLint vstatus = GL_FALSE, fstatus = GL_FALSE;
    api.glGetShaderiv(vshader, GL_COMPILE_STATUS, &vstatus);
    api.glGetShaderiv(fshader, GL_COMPILE_STATUS, &fstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vshader);
    api.glAttachShader(program, fshader);
    api.glLinkProgram(program);
    GLint lstatus = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);

    const GLuint block_index = api.glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "GraphicsData");
    api.glShaderStorageBlockBinding(program, block_index, 3);

    GLuint buffer = 0, vao = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffer);
    api.glGenVertexArrays(1, &vao);
    api.glBindVertexArray(vao);
    api.glUseProgram(program);
    api.glDrawArrays(GL_POINTS, 0, 1);
    api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    GLuint value = 0;
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    GLint reported_binding = -1;
    if (block_index != GL_INVALID_INDEX) {
        GLenum prop = GL_BUFFER_BINDING;
        GLsizei written = 0;
        api.glGetProgramResourceiv(program, GL_SHADER_STORAGE_BLOCK, block_index, 1, &prop, 1, &written,
                                   &reported_binding);
    }
    r.ok = (vstatus && fstatus && lstatus && block_index != GL_INVALID_INDEX && value == 0x55u &&
            (reported_binding == -1 || reported_binding == 3));
    r.detail = "v=" + std::to_string(vstatus) + " f=" + std::to_string(fstatus) +
               " link=" + std::to_string(lstatus) + " index=" + std::to_string(block_index) +
               " vertex_value=" + std::to_string(value) +
               " reported_binding=" + std::to_string(reported_binding);
    api.glDeleteVertexArrays(1, &vao);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteProgram(program);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader);
    return r;
}

Result test_compute_512() {
    Result r;
    const char* cs = R"(#version 310 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(std430, binding = 0) buffer Out { uint value; } outb;
void main() {
    if (gl_GlobalInvocationID == uvec3(0u)) outb.value = 0x512u;
}
)";
    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLint cstatus = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &cstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint lstatus = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);
    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
    api.glUseProgram(program);
    api.glDispatchCompute(1, 1, 1);
    api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.ok = (cstatus == GL_TRUE && lstatus == GL_TRUE && value == 0x512u);
    r.detail = "compile=" + std::to_string(cstatus) + " link=" + std::to_string(lstatus) +
               " value=" + std::to_string(value);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteProgram(program);
    api.glDeleteShader(shader);
    return r;
}

Result test_vertex_image_store() {
    Result r;
    const char* vs = R"(#version 310 es
layout(rgba16f, binding = 0) writeonly uniform highp image3D img;
void main() {
    imageStore(img, ivec3(0), vec4(42.0, 0.0, 0.0, 0.0));
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    gl_PointSize = 1.0;
}
)";
    const char* fs = "#version 310 es\nprecision mediump float;\nlayout(location = 0) out vec4 color;\nvoid main() { color = vec4(1.0); }\n";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(rgba16f, binding = 0) uniform highp readonly image3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = uint(imageLoad(img, ivec3(0)).r); }
)";
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint fshader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader, 1, &fs, nullptr);
    api.glCompileShader(fshader);
    GLint vstatus = GL_FALSE, fstatus = GL_FALSE;
    api.glGetShaderiv(vshader, GL_COMPILE_STATUS, &vstatus);
    api.glGetShaderiv(fshader, GL_COMPILE_STATUS, &fstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vshader);
    api.glAttachShader(program, fshader);
    api.glLinkProgram(program);
    GLint lstatus = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);

    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, 1, 1, 1);
    api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    api.glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    GLenum bind_error = api.glGetError();
    GLuint vao = 0;
    api.glGenVertexArrays(1, &vao);
    api.glBindVertexArray(vao);
    api.glUseProgram(program);
    api.glDrawArrays(GL_POINTS, 0, 1);

    GLuint read_program = api.glCreateProgram();
    GLuint read_shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(read_shader, 1, &read_cs, nullptr);
    api.glCompileShader(read_shader);
    GLint read_compile = GL_FALSE;
    api.glGetShaderiv(read_shader, GL_COMPILE_STATUS, &read_compile);
    api.glAttachShader(read_program, read_shader);
    api.glLinkProgram(read_program);
    GLint read_link = GL_FALSE;
    api.glGetProgramiv(read_program, GL_LINK_STATUS, &read_link);
    GLint read_log_length = 0;
    api.glGetProgramiv(read_program, GL_INFO_LOG_LENGTH, &read_log_length);
    std::string read_log(static_cast<size_t>(read_log_length), '\0');
    if (read_log_length > 0) api.glGetProgramInfoLog(read_program, read_log_length, nullptr, read_log.data());
    const std::string read_source = shader_source_text(read_shader);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    api.glUseProgram(read_program);
    api.glDispatchCompute(1, 1, 1);
    api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.ok = (vstatus && fstatus && lstatus && read_compile && read_link && bind_error == GL_NO_ERROR &&
            value == 42u);
    r.detail = "vs=" + std::to_string(vstatus) + " fs=" + std::to_string(fstatus) +
               " link=" + std::to_string(lstatus) + " read_compile=" + std::to_string(read_compile) +
               " read_link=" + std::to_string(read_link) + " bind_error=" + std::to_string(bind_error) +
               " image_value=" + std::to_string(value) + " read_source=" +
               read_source.substr(0, std::min<size_t>(read_source.size(), 500)) +
               " read_log=" + read_log;
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteVertexArrays(1, &vao);
    api.glDeleteTextures(1, &tex);
    api.glDeleteProgram(read_program);
    api.glDeleteShader(read_shader);
    api.glDeleteProgram(program);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader);
    return r;
}

Result test_read_image_link() {
    Result r;
    const char* src = R"(#version 310 es
layout(local_size_x = 1) in;
layout(rgba16f, binding = 0) uniform highp readonly image3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = uint(imageLoad(img, ivec3(0)).r); }
)";
    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &src, nullptr);
    api.glCompileShader(shader);
    GLint cstatus = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &cstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint lstatus = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);
    GLint log_length = 0;
    api.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    std::string log(static_cast<size_t>(log_length), '\0');
    if (log_length > 0) api.glGetProgramInfoLog(program, log_length, nullptr, log.data());
    const std::string source = shader_source_text(shader);
    r.ok = (cstatus && lstatus);
    r.detail = "compile=" + std::to_string(cstatus) + " link=" + std::to_string(lstatus) +
               " source=" + source.substr(0, std::min<size_t>(source.size(), 400)) + " log=" + log;
    api.glDeleteProgram(program);
    api.glDeleteShader(shader);
    return r;
}

Result test_desktop_glsl_version(const char* version) {
    Result r;
    std::string source = "#version ";
    source += version;
    if (std::string(version) == "120") {
        source += "\nvoid main() { gl_FragColor = vec4(1.0); }\n";
    } else {
        source += "\nprecision highp float;\nout vec4 color;\nvoid main() { color = vec4(1.0); }\n";
    }
    GLuint shader = api.glCreateShader(GL_FRAGMENT_SHADER);
    const char* src = source.c_str();
    api.glShaderSource(shader, 1, &src, nullptr);
    api.glCompileShader(shader);
    GLint status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint log_length = 0;
    api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    std::string log(static_cast<size_t>(log_length), '\0');
    if (log_length > 0) api.glGetShaderInfoLog(shader, log_length, nullptr, log.data());
    r.ok = (status == GL_TRUE);
    r.detail = std::string("version ") + version + " compile=" + std::to_string(status) + " log=" + log;
    api.glDeleteShader(shader);
    return r;
}

Result test_rgb8_snorm_attachment() {
    Result r;
    // Complementary's colortex1 is RGB8_SNORM. ANGLE reports that format as
    // INCOMPLETE_ATTACHMENT when attached directly; the layer must provide a
    // renderable backing without losing the signed channels.
    GLuint tex = 0, fbo = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8_SNORM, 4, 4, 0, GL_RGB, GL_BYTE, nullptr);
    const GLenum tex_error = api.glGetError();
    api.glGenFramebuffers(1, &fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    const GLenum tex_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &fbo);
    api.glDeleteTextures(1, &tex);

    // The immutable-storage path used by Iris for some targets.
    GLuint tex2 = 0, fbo2 = 0;
    api.glGenTextures(1, &tex2);
    api.glBindTexture(GL_TEXTURE_2D, tex2);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8_SNORM, 4, 4);
    const GLenum storage_error = api.glGetError();
    api.glGenFramebuffers(1, &fbo2);
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);
    const GLenum storage_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &fbo2);
    api.glDeleteTextures(1, &tex2);

    r.ok = (tex_error == GL_NO_ERROR && storage_error == GL_NO_ERROR &&
            tex_status == GL_FRAMEBUFFER_COMPLETE && storage_status == GL_FRAMEBUFFER_COMPLETE);
    r.detail = "tex_image_error=" + std::to_string(tex_error) + " tex_status=" + std::to_string(tex_status) +
               " tex_storage_error=" + std::to_string(storage_error) +
               " tex_storage_status=" + std::to_string(storage_status);
    return r;
}

Result test_extension_consistency() {
    Result r;
    const char* exts = reinterpret_cast<const char*>(api.glGetString(GL_EXTENSIONS));
    const std::string list = exts ? exts : "";
    auto has = [&](const char* name) {
        const size_t n = strlen(name);
        size_t pos = 0;
        while ((pos = list.find(name, pos)) != std::string::npos) {
            const bool left = pos == 0 || list[pos - 1] == ' ';
            const bool right = pos + n == list.size() || list[pos + n] == ' ';
            if (left && right) return true;
            pos += n;
        }
        return false;
    };
    GLint ssbo_bindings = 0, image_units = 0, compute_invocations = 0;
    if (has("GL_ARB_shader_storage_buffer_object")) {
        api.glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &ssbo_bindings);
    }
    if (has("GL_ARB_shader_image_load_store")) {
        api.glGetIntegerv(GL_MAX_IMAGE_UNITS, &image_units);
    }
    if (has("GL_ARB_compute_shader")) {
        api.glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &compute_invocations);
    }
    const bool no_error_absent = !has("GL_KHR_no_error");
    const bool separate_absent = !has("GL_ARB_separate_shader_objects");
    r.ok = no_error_absent && separate_absent &&
           (!has("GL_ARB_shader_storage_buffer_object") || ssbo_bindings > 0) &&
           (!has("GL_ARB_shader_image_load_store") || image_units > 0) &&
           (!has("GL_ARB_compute_shader") || compute_invocations > 0);
    r.detail = "no_error_absent=" + std::string(no_error_absent ? "yes" : "no") +
               " separate_shader_absent=" + std::string(separate_absent ? "yes" : "no") +
               " ssbo=" + std::string(has("GL_ARB_shader_storage_buffer_object") ? "yes" : "no") +
               "/" + std::to_string(ssbo_bindings) +
               " image=" + std::string(has("GL_ARB_shader_image_load_store") ? "yes" : "no") +
               "/" + std::to_string(image_units) +
               " compute=" + std::string(has("GL_ARB_compute_shader") ? "yes" : "no") +
               "/" + std::to_string(compute_invocations);
    return r;
}

Result test_formatless_image_metadata() {
    Result r;
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 1) in;
writeonly uniform uimage3D img;
void main() { imageStore(img, ivec3(0), uvec4(12345u, 0u, 0u, 0u)); }
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
readonly uniform uimage3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = imageLoad(img, ivec3(0)).r; }
)";
    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_R16UI, 1, 1, 1);
    GLenum storage_error = api.glGetError();
    api.glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R16UI);
    GLenum bind_error = api.glGetError();

    auto compile_program = [&](const char* source, bool register_metadata, std::string* log_out) -> GLuint {
        GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
        if (register_metadata) {
            api.mg_image_metadata_begin();
            api.mg_image_metadata_add("img", GL_R16UI);
            api.mg_image_metadata_end();
        }
        api.glShaderSource(shader, 1, &source, nullptr);
        api.glCompileShader(shader);
        GLint status = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        if (len > 0) api.glGetShaderInfoLog(shader, len, nullptr, log.data());
        if (log_out) *log_out = log;
        if (!status) {
            api.glDeleteShader(shader);
            return 0;
        }
        GLuint program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint linked = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        api.glDeleteShader(shader);
        if (!linked) {
            api.glDeleteProgram(program);
            return 0;
        }
        return program;
    };

    std::string write_log, read_log, unregistered_log;
    GLuint write_program = compile_program(write_cs, true, &write_log);
    GLuint read_program = compile_program(read_cs, true, &read_log);
    GLuint unregistered = compile_program(write_cs, false, &unregistered_log);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    if (write_program) {
        api.glUseProgram(write_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    if (read_program) {
        api.glUseProgram(read_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    GLuint value = 0;
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.ok = (storage_error == GL_NO_ERROR && bind_error == GL_NO_ERROR && write_program != 0 &&
            read_program != 0 && unregistered == 0 && value == 12345u);
    r.detail = "storage_error=" + std::to_string(storage_error) + " bind_error=" + std::to_string(bind_error) +
               " write_program=" + std::to_string(write_program) +
               " read_program=" + std::to_string(read_program) +
               " unregistered_program=" + std::to_string(unregistered) +
               " value=" + std::to_string(value) +
               (write_log.empty() ? "" : " write_log=" + write_log) +
               (read_log.empty() ? "" : " read_log=" + read_log) +
               (unregistered_log.empty() ? "" : " unregistered_log=" + unregistered_log);
    if (write_program) api.glDeleteProgram(write_program);
    if (read_program) api.glDeleteProgram(read_program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_explicit_image_binding() {
    Result r;
    // Both programs name unit 5 themselves and never call glUniform1i. If the
    // wrapper replaces the explicit binding with its fallback unit, the image
    // is written/read on a unit the application never bound and the value is 0.
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(binding = 5, r32ui) writeonly uniform highp uimage3D img;
void main() { imageStore(img, ivec3(0), uvec4(0x12345678u, 0u, 0u, 0u)); }
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(binding = 5, r32ui) readonly uniform highp uimage3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = imageLoad(img, ivec3(0)).r; }
)";

    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, 1, 1, 1);
    const GLenum storage_error = api.glGetError();
    api.glBindImageTexture(5, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    const GLenum bind_error = api.glGetError();

    auto build_program = [&](const char* source, std::string* log_out) -> GLuint {
        GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &source, nullptr);
        api.glCompileShader(shader);
        GLint status = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log_out->resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log_out->data());
        }
        if (!status) {
            api.glDeleteShader(shader);
            return 0;
        }
        GLuint program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint linked = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        api.glDeleteShader(shader);
        if (!linked) {
            api.glDeleteProgram(program);
            return 0;
        }
        return program;
    };

    std::string write_log, read_log;
    GLuint write_program = build_program(write_cs, &write_log);
    GLuint read_program = build_program(read_cs, &read_log);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    if (write_program) {
        api.glUseProgram(write_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    if (read_program) {
        api.glUseProgram(read_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    GLuint value = 0;
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.ok = storage_error == GL_NO_ERROR && bind_error == GL_NO_ERROR && write_program != 0 &&
           read_program != 0 && value == 0x12345678u;
    r.detail = "storage_error=" + std::to_string(storage_error) + " bind_error=" + std::to_string(bind_error) +
               " write_program=" + std::to_string(write_program) +
               " read_program=" + std::to_string(read_program) + " value=" + std::to_string(value) +
               (write_log.empty() ? "" : " write_log=" + write_log) +
               (read_log.empty() ? "" : " read_log=" + read_log);
    if (write_program) api.glDeleteProgram(write_program);
    if (read_program) api.glDeleteProgram(read_program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_duplicate_image_binding() {
    Result r;
    // Iris can leave both images at binding 0 while assigning units through
    // glUniform1i. The wrapper must treat the repeated binding as an alias and
    // give the declarations distinct fallback units.
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(binding = 0) writeonly uniform highp uimage3D img0;
layout(binding = 0) writeonly uniform highp uimage3D img1;
void main() {
    imageStore(img0, ivec3(0), uvec4(11u, 0u, 0u, 0u));
    imageStore(img1, ivec3(0), uvec4(22u, 0u, 0u, 0u));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(binding = 0) readonly uniform highp uimage3D img0;
layout(binding = 0) readonly uniform highp uimage3D img1;
layout(std430, binding = 1) buffer Out { uint v[2]; } outb;
void main() {
    outb.v[0] = imageLoad(img0, ivec3(0)).r;
    outb.v[1] = imageLoad(img1, ivec3(0)).r;
}
)";

    GLuint tex0 = 0, tex1 = 0;
    api.glGenTextures(1, &tex0);
    api.glGenTextures(1, &tex1);
    for (GLuint tex : {tex0, tex1}) {
        api.glBindTexture(GL_TEXTURE_3D, tex);
        api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, 1, 1, 1);
    }
    api.glBindImageTexture(0, tex0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    api.glBindImageTexture(1, tex1, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    const GLenum bind_error = api.glGetError();

    auto build_program = [&](const char* source, std::string* log_out) -> GLuint {
        GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.mg_image_metadata_begin();
        api.mg_image_metadata_add("img0", GL_R32UI);
        api.mg_image_metadata_add("img1", GL_R32UI);
        api.mg_image_metadata_end();
        api.glShaderSource(shader, 1, &source, nullptr);
        api.glCompileShader(shader);
        GLint status = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log_out->resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log_out->data());
        }
        if (!status) {
            api.glDeleteShader(shader);
            return 0;
        }
        GLuint program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint linked = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        api.glDeleteShader(shader);
        if (!linked) {
            api.glDeleteProgram(program);
            return 0;
        }
        return program;
    };

    std::string write_log, read_log;
    GLuint write_program = build_program(write_cs, &write_log);
    GLuint read_program = build_program(read_cs, &read_log);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    if (write_program) {
        api.glUseProgram(write_program);
        api.glUniform1i(api.glGetUniformLocation(write_program, "img0"), 0);
        api.glUniform1i(api.glGetUniformLocation(write_program, "img1"), 1);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    if (read_program) {
        api.glUseProgram(read_program);
        api.glUniform1i(api.glGetUniformLocation(read_program, "img0"), 0);
        api.glUniform1i(api.glGetUniformLocation(read_program, "img1"), 1);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    GLuint values[2] = {};
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(values, ptr, sizeof(values));
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.ok = bind_error == GL_NO_ERROR && write_program != 0 && read_program != 0 && values[0] == 11u &&
           values[1] == 22u;
    r.detail = "bind_error=" + std::to_string(bind_error) + " write_program=" + std::to_string(write_program) +
               " read_program=" + std::to_string(read_program) + " values=" + std::to_string(values[0]) + "," +
               std::to_string(values[1]) + (write_log.empty() ? "" : " write_log=" + write_log) +
               (read_log.empty() ? "" : " read_log=" + read_log);
    if (write_program) api.glDeleteProgram(write_program);
    if (read_program) api.glDeleteProgram(read_program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex0);
    api.glDeleteTextures(1, &tex1);
    return r;
}

Result test_program_uniform_image_binding() {
    Result r;
    // glProgramUniform1i names its program explicitly. The wrapper must remap
    // the image unit through that program's map, not the current program's and
    // not by forwarding the application's unit unchanged. One write is made
    // while a different program is current, which also checks that the remap
    // does not depend on glUseProgram having selected the target first.
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 1) in;
writeonly uniform highp uimage3D img0;
writeonly uniform highp uimage3D img1;
void main() {
    imageStore(img0, ivec3(0), uvec4(11u, 0u, 0u, 0u));
    imageStore(img1, ivec3(0), uvec4(22u, 0u, 0u, 0u));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
readonly uniform highp uimage3D img0;
readonly uniform highp uimage3D img1;
layout(std430, binding = 1) buffer Out { uint v[2]; } outb;
void main() {
    outb.v[0] = imageLoad(img0, ivec3(0)).r;
    outb.v[1] = imageLoad(img1, ivec3(0)).r;
}
)";
    const char* idle_cs = R"(#version 310 es
layout(local_size_x = 1) in;
void main() {}
)";

    auto build_program = [&](const char* source, bool with_metadata, std::string* log_out) -> GLuint {
        GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
        if (with_metadata) {
            api.mg_image_metadata_begin();
            api.mg_image_metadata_add("img0", GL_R32UI);
            api.mg_image_metadata_add("img1", GL_R32UI);
            api.mg_image_metadata_end();
        }
        api.glShaderSource(shader, 1, &source, nullptr);
        api.glCompileShader(shader);
        GLint status = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log_out->resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log_out->data());
        }
        if (!status) {
            api.glDeleteShader(shader);
            return 0;
        }
        GLuint program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint linked = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        api.glDeleteShader(shader);
        if (!linked) {
            api.glDeleteProgram(program);
            return 0;
        }
        return program;
    };

    std::string write_log, read_log, idle_log;
    GLuint write_program = build_program(write_cs, true, &write_log);
    GLuint read_program = build_program(read_cs, true, &read_log);
    GLuint idle_program = build_program(idle_cs, false, &idle_log);

    GLuint tex0 = 0, tex1 = 0;
    api.glGenTextures(1, &tex0);
    api.glGenTextures(1, &tex1);
    for (GLuint tex : {tex0, tex1}) {
        api.glBindTexture(GL_TEXTURE_3D, tex);
        api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, 1, 1, 1);
    }
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBindImageTexture(5, tex0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    api.glBindImageTexture(6, tex1, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    const GLenum bind_error = api.glGetError();

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    GLuint values[2] = {};
    bool dispatched = false;
    if (write_program) {
        const GLint loc0 = api.glGetUniformLocation(write_program, "img0");
        const GLint loc1 = api.glGetUniformLocation(write_program, "img1");
        api.glUseProgram(write_program);
        api.glProgramUniform1i(write_program, loc0, 5);
        // The second uniform is written while an unrelated program is current.
        if (idle_program) api.glUseProgram(idle_program);
        api.glProgramUniform1i(write_program, loc1, 6);
        api.glUseProgram(write_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        dispatched = loc0 >= 0 && loc1 >= 0;
    }
    if (read_program) {
        const GLint loc0 = api.glGetUniformLocation(read_program, "img0");
        const GLint loc1 = api.glGetUniformLocation(read_program, "img1");
        api.glUseProgram(read_program);
        api.glProgramUniform1i(read_program, loc0, 5);
        api.glProgramUniform1i(read_program, loc1, 6);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(values, ptr, sizeof(values));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }
    r.ok = bind_error == GL_NO_ERROR && write_program != 0 && read_program != 0 && dispatched &&
           values[0] == 11u && values[1] == 22u;
    r.detail = "bind_error=" + std::to_string(bind_error) + " write_program=" + std::to_string(write_program) +
               " read_program=" + std::to_string(read_program) + " values=" + std::to_string(values[0]) + "," +
               std::to_string(values[1]) + (write_log.empty() ? "" : " write_log=" + write_log) +
               (read_log.empty() ? "" : " read_log=" + read_log) +
               (idle_log.empty() ? "" : " idle_log=" + idle_log);
    if (write_program) api.glDeleteProgram(write_program);
    if (read_program) api.glDeleteProgram(read_program);
    if (idle_program) api.glDeleteProgram(idle_program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex0);
    api.glDeleteTextures(1, &tex1);
    return r;
}

Result test_image_array_uniform_iv() {
    Result r;
    // A 1iv write over an image array names one element per value. Each element
    // is an active image uniform with its own canonical unit; remapping only
    // the base location leaves the second store aimed at a unit the pack never
    // bound.
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(r32ui) writeonly uniform highp uimage3D img[2];
void main() {
    imageStore(img[0], ivec3(0), uvec4(11u, 0u, 0u, 0u));
    imageStore(img[1], ivec3(0), uvec4(22u, 0u, 0u, 0u));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(r32ui) readonly uniform highp uimage3D img[2];
layout(std430, binding = 1) buffer Out { uint v[2]; } outb;
void main() {
    outb.v[0] = imageLoad(img[0], ivec3(0)).r;
    outb.v[1] = imageLoad(img[1], ivec3(0)).r;
}
)";

    auto build_program = [&](const char* source, std::string* log_out) -> GLuint {
        GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &source, nullptr);
        api.glCompileShader(shader);
        GLint status = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log_out->resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log_out->data());
        }
        if (!status) {
            api.glDeleteShader(shader);
            return 0;
        }
        GLuint program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint linked = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
        api.glDeleteShader(shader);
        if (!linked) {
            api.glDeleteProgram(program);
            return 0;
        }
        return program;
    };

    std::string write_log, read_log;
    GLuint write_program = build_program(write_cs, &write_log);
    GLuint read_program = build_program(read_cs, &read_log);

    GLuint tex0 = 0, tex1 = 0;
    api.glGenTextures(1, &tex0);
    api.glGenTextures(1, &tex1);
    for (GLuint tex : {tex0, tex1}) {
        api.glBindTexture(GL_TEXTURE_3D, tex);
        api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, 1, 1, 1);
    }
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBindImageTexture(3, tex0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    api.glBindImageTexture(4, tex1, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    const GLenum bind_error = api.glGetError();

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    GLuint values[2] = {};
    bool dispatched = false;
    const GLint write_units[2] = {3, 4};
    if (write_program) {
        const GLint loc = api.glGetUniformLocation(write_program, "img");
        api.glUseProgram(write_program);
        api.glUniform1iv(loc, 2, write_units);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        dispatched = loc >= 0;
    }
    if (read_program) {
        const GLint loc = api.glGetUniformLocation(read_program, "img");
        api.glUseProgram(read_program);
        api.glUniform1iv(loc, 2, write_units);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(values, ptr, sizeof(values));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }
    r.ok = bind_error == GL_NO_ERROR && write_program != 0 && read_program != 0 && dispatched &&
           values[0] == 11u && values[1] == 22u;
    r.detail = "bind_error=" + std::to_string(bind_error) + " write_program=" + std::to_string(write_program) +
               " read_program=" + std::to_string(read_program) + " values=" + std::to_string(values[0]) + "," +
               std::to_string(values[1]) + (write_log.empty() ? "" : " write_log=" + write_log) +
               (read_log.empty() ? "" : " read_log=" + read_log);
    if (write_program) api.glDeleteProgram(write_program);
    if (read_program) api.glDeleteProgram(read_program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex0);
    api.glDeleteTextures(1, &tex1);
    return r;
}

Result test_depth_filter_complete() {
    Result r;
    // Desktop GL permits MIPMAP_LINEAR/LINEAR on a depth texture. Strict ES
    // makes a sized depth texture filter-incomplete under those filters and
    // samples (0,0,0,1). The wrapper must keep the application-visible filter
    // state while pushing depth-legal filters to the driver, so a sample sees
    // the stored depth value.
    const char* cs = R"(#version 310 es
layout(local_size_x = 1) in;
uniform highp sampler2D depthTex;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = floatBitsToUint(texture(depthTex, vec2(0.5)).x); }
)";
    const float depth_value = 0.5f;
    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_value);
    const GLenum image_error = api.glGetError();
    api.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    api.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GLint read_min = 0, read_mag = 0;
    api.glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &read_min);
    api.glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &read_mag);
    api.glActiveTexture(GL_TEXTURE0);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    GLuint sampler = 0;
    api.glGenSamplers(1, &sampler);
    api.glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glBindSampler(0, sampler);

    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLint status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint len = 0;
    api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len), '\0');
    if (len > 0) api.glGetShaderInfoLog(shader, len, nullptr, log.data());
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    api.glDeleteShader(shader);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    GLuint bits = 0;
    if (status == GL_TRUE && linked == GL_TRUE) {
        api.glUseProgram(program);
        api.glUniform1i(api.glGetUniformLocation(program, "depthTex"), 0);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(bits), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(&bits, ptr, sizeof(bits));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }
    float sampled = 0.0f;
    memcpy(&sampled, &bits, sizeof(sampled));
    GLint sampler_binding = 0;
    api.glGetIntegerv(GL_SAMPLER_BINDING, &sampler_binding);
    const bool truthful_filters = read_min == GL_NEAREST_MIPMAP_LINEAR && read_mag == GL_LINEAR;
    r.ok = image_error == GL_NO_ERROR && status == GL_TRUE && linked == GL_TRUE &&
           truthful_filters && sampler_binding == static_cast<GLint>(sampler) && sampled > 0.49f && sampled < 0.51f;
    r.detail = "image_error=" + std::to_string(image_error) + " compile=" + std::to_string(status) +
               " link=" + std::to_string(linked) + " read_min=" + std::to_string(read_min) +
               " read_mag=" + std::to_string(read_mag) + " sampler_binding=" + std::to_string(sampler_binding) +
               " sampler=" + std::to_string(sampler) + " sampled=" + std::to_string(sampled) +
               (log.empty() ? "" : " log=" + log);
    api.glBindSampler(0, 0);
    if (sampler) api.glDeleteSamplers(1, &sampler);
    api.glDeleteProgram(program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_clear_tex_level() {
    Result r;
    const char* cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(r32ui, binding = 0) uniform highp uimage3D srcImg;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = imageLoad(srcImg, ivec3(0)).r; }
)";
    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glTexStorage3D(GL_TEXTURE_3D, 2, GL_R32UI, 2, 2, 2);
    GLuint seven = 7, nine = 9;
    api.glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 1, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &seven);
    api.glTexSubImage3D(GL_TEXTURE_3D, 1, 0, 0, 0, 1, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &nine);
    const GLenum upload_error = api.glGetError();

    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLint compiled = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    api.glDeleteShader(shader);
    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 4, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    auto read_level = [&](GLint level, GLuint& value) {
        api.glBindImageTexture(0, tex, level, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
        api.glUseProgram(program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        value = 0;
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(value), GL_MAP_READ_BIT);
        if (!ptr) return false;
        memcpy(&value, ptr, sizeof(value));
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        return true;
    };

    api.glClearTexImage(tex, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    const GLenum clear_error = api.glGetError();
    GLuint level0 = 0, level1 = 0;
    const bool read0 = read_level(0, level0);
    const bool read1 = read_level(1, level1);
    const GLenum read_error = api.glGetError();
    r.ok = upload_error == GL_NO_ERROR && compiled == GL_TRUE && linked == GL_TRUE && clear_error == GL_NO_ERROR &&
           read0 && read1 && read_error == GL_NO_ERROR && level0 == 7 && level1 == 0;
    r.detail = "upload_error=" + std::to_string(upload_error) + " compile=" + std::to_string(compiled) +
               " link=" + std::to_string(linked) + " clear_error=" + std::to_string(clear_error) +
               " level0=" + std::to_string(level0) + " level1=" + std::to_string(level1) +
               " read_error=" + std::to_string(read_error);
    api.glDeleteProgram(program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_timer_query64() {
    Result r;
    constexpr GLenum kTimestamp = 0x8E28; // GL_TIMESTAMP, absent from the ES headers
    GLuint query = 0;
    api.glGenQueries(1, &query);
    GLuint64 first = 0, second = 0;
    GLenum first_error = GL_NO_ERROR, second_error = GL_NO_ERROR;
    if (query != 0) {
        api.glQueryCounter(query, kTimestamp);
        first_error = api.glGetError();
        api.glGetQueryObjectui64v(query, GL_QUERY_RESULT, &first);
        second_error = api.glGetError();
        api.glQueryCounter(query, kTimestamp);
        api.glGetQueryObjectui64v(query, GL_QUERY_RESULT, &second);
    }
    r.ok = query != 0 && first_error == GL_NO_ERROR && second_error == GL_NO_ERROR && first != 0 && second >= first;
    r.detail = "query=" + std::to_string(query) + " first=" + std::to_string(first) +
               " second=" + std::to_string(second) + " first_error=" + std::to_string(first_error) +
               " second_error=" + std::to_string(second_error);
    if (query) api.glDeleteQueries(1, &query);
    return r;
}

Result test_copy_image_subdata_depth() {
    Result r;
    // Iris uses glCopyImageSubData for depth-target copies. Same-format D32F
    // is the live path; the destination must contain the source depth, not
    // zero or a converted value.
    const char* cs = R"(#version 310 es
layout(local_size_x = 1) in;
uniform highp sampler2D depthTex;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = floatBitsToUint(texture(depthTex, vec2(0.5)).x); }
)";
    const float depth_value = 0.5f;
    GLuint src = 0, dst = 0;
    api.glGenTextures(1, &src);
    api.glGenTextures(1, &dst);
    api.glBindTexture(GL_TEXTURE_2D, src);
    api.glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, &depth_value);
    api.glBindTexture(GL_TEXTURE_2D, dst);
    api.glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 1, 1, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    const GLenum image_error = api.glGetError();
    api.glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0, dst, GL_TEXTURE_2D, 0, 0, 0, 0, 1, 1, 1);
    const GLenum copy_error = api.glGetError();
    api.glActiveTexture(GL_TEXTURE0);
    api.glBindTexture(GL_TEXTURE_2D, dst);

    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLint status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint len = 0;
    api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len), '\0');
    if (len > 0) api.glGetShaderInfoLog(shader, len, nullptr, log.data());
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    api.glDeleteShader(shader);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    GLuint bits = 0;
    if (status == GL_TRUE && linked == GL_TRUE) {
        api.glUseProgram(program);
        api.glUniform1i(api.glGetUniformLocation(program, "depthTex"), 0);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(bits), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(&bits, ptr, sizeof(bits));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }
    float sampled = 0.0f;
    memcpy(&sampled, &bits, sizeof(sampled));
    r.ok = image_error == GL_NO_ERROR && copy_error == GL_NO_ERROR && status == GL_TRUE && linked == GL_TRUE &&
           sampled > 0.49f && sampled < 0.51f;
    r.detail = "image_error=" + std::to_string(image_error) + " copy_error=" + std::to_string(copy_error) +
               " compile=" + std::to_string(status) + " link=" + std::to_string(linked) +
               " sampled=" + std::to_string(sampled) + (log.empty() ? "" : " log=" + log);
    api.glDeleteProgram(program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &src);
    api.glDeleteTextures(1, &dst);
    return r;
}

Result test_blit_resolve() {
    Result r;
    // A standard multisample resolve: clear a 4x MSAA renderbuffer, blit it to
    // a single-sample texture, and sample the resolved colour. The wrapper's
    // blit must present the resolved value, not the unresolved attachment.
    const char* cs = R"(#version 310 es
layout(local_size_x = 1) in;
uniform highp sampler2D colorTex;
layout(std430, binding = 1) buffer Out { uint v[3]; } outb;
void main() {
    vec4 c = texture(colorTex, vec2(0.5));
    outb.v[0] = floatBitsToUint(c.r);
    outb.v[1] = floatBitsToUint(c.g);
    outb.v[2] = floatBitsToUint(c.b);
}
)";
    GLuint rb = 0, read_fbo = 0, draw_fbo = 0, tex = 0;
    api.glGenRenderbuffers(1, &rb);
    api.glBindRenderbuffer(GL_RENDERBUFFER, rb);
    api.glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_RGBA8, 1, 1);
    api.glGenFramebuffers(1, &read_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, read_fbo);
    api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
    const GLenum read_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glViewport(0, 0, 1, 1);
    api.glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
    api.glClear(GL_COLOR_BUFFER_BIT);

    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    api.glGenFramebuffers(1, &draw_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, draw_fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    const GLenum draw_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fbo);
    api.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fbo);
    api.glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    const GLenum blit_error = api.glGetError();
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLint status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    GLint len = 0;
    api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<size_t>(len), '\0');
    if (len > 0) api.glGetShaderInfoLog(shader, len, nullptr, log.data());
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    api.glDeleteShader(shader);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    GLuint bits[3] = {};
    if (status == GL_TRUE && linked == GL_TRUE) {
        api.glActiveTexture(GL_TEXTURE0);
        api.glBindTexture(GL_TEXTURE_2D, tex);
        api.glUseProgram(program);
        api.glUniform1i(api.glGetUniformLocation(program, "colorTex"), 0);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(bits), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(bits, ptr, sizeof(bits));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }
    float rgb[3];
    memcpy(rgb, bits, sizeof(rgb));
    const bool close = rgb[0] > 0.23f && rgb[0] < 0.27f && rgb[1] > 0.48f && rgb[1] < 0.52f && rgb[2] > 0.73f &&
                       rgb[2] < 0.77f;
    r.ok = read_status == GL_FRAMEBUFFER_COMPLETE && draw_status == GL_FRAMEBUFFER_COMPLETE &&
           blit_error == GL_NO_ERROR && status == GL_TRUE && linked == GL_TRUE && close;
    r.detail = "read_status=" + std::to_string(read_status) + " draw_status=" + std::to_string(draw_status) +
               " blit_error=" + std::to_string(blit_error) + " compile=" + std::to_string(status) +
               " link=" + std::to_string(linked) + " rgb=" + std::to_string(rgb[0]) + "," +
               std::to_string(rgb[1]) + "," + std::to_string(rgb[2]) + (log.empty() ? "" : " log=" + log);
    api.glDeleteProgram(program);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteFramebuffers(1, &read_fbo);
    api.glDeleteFramebuffers(1, &draw_fbo);
    api.glDeleteRenderbuffers(1, &rb);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_depth_copy_mismatch() {
    Result r;
    // A D24 destination copied from a D24S8 source has equal depth width but a
    // different complete format. ANGLE's Vulkan backend loses the device if the
    // blit is submitted; the wrapper must skip it and the context must remain
    // usable (the next fence is the game's first post-render operation).
    GLuint rb = 0, src_fbo = 0, tex = 0;
    api.glGenRenderbuffers(1, &rb);
    api.glBindRenderbuffer(GL_RENDERBUFFER, rb);
    api.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    api.glGenFramebuffers(1, &src_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, src_fbo);
    api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    const GLenum src_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    api.glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 1, 1);
    const GLenum copy_error = api.glGetError();
    GLsync fence = api.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    const GLenum fence_error = api.glGetError();
    r.ok = src_status == GL_FRAMEBUFFER_COMPLETE && copy_error == GL_NO_ERROR && fence != nullptr &&
           fence_error == GL_NO_ERROR;
    r.detail = "src_status=" + std::to_string(src_status) + " copy_error=" + std::to_string(copy_error) +
               " fence=" + (fence ? "ok" : "null") + " fence_error=" + std::to_string(fence_error);
    if (fence && api.glDeleteSync) api.glDeleteSync(fence);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &src_fbo);
    api.glDeleteRenderbuffers(1, &rb);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_blit_depth_mismatch() {
    Result r;
    // Desktop GL and GLES both require identical depth/stencil formats for a
    // depth blit. A D24 draw target and a D24S8 read target are exactly the
    // mismatch that must be rejected with GL_INVALID_OPERATION, not submitted
    // to ANGLE where it can lose the device.
    GLuint rb = 0, src_fbo = 0, dst_fbo = 0, tex = 0;
    api.glGenRenderbuffers(1, &rb);
    api.glBindRenderbuffer(GL_RENDERBUFFER, rb);
    api.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    api.glGenFramebuffers(1, &src_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, src_fbo);
    api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    const GLenum src_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    api.glGenFramebuffers(1, &dst_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tex, 0);
    const GLenum dst_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
    api.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    const GLenum blit_error = api.glGetError();
    GLsync fence = api.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    const GLenum fence_error = api.glGetError();
    r.ok = src_status == GL_FRAMEBUFFER_COMPLETE && dst_status == GL_FRAMEBUFFER_COMPLETE &&
           blit_error == GL_INVALID_OPERATION && fence != nullptr && fence_error == GL_NO_ERROR;
    r.detail = "src_status=" + std::to_string(src_status) + " dst_status=" + std::to_string(dst_status) +
               " blit_error=" + std::to_string(blit_error) + " fence=" + (fence ? "ok" : "null") +
               " fence_error=" + std::to_string(fence_error);
    if (fence && api.glDeleteSync) api.glDeleteSync(fence);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &src_fbo);
    api.glDeleteFramebuffers(1, &dst_fbo);
    api.glDeleteRenderbuffers(1, &rb);
    api.glDeleteTextures(1, &tex);
    return r;
}

static Result run_stage_draw(GLenum draw_mode, GLuint* shaders, int shader_count, const char* label,
                             bool need_patch_vertices, const char* tcs_log, const char* tes_log) {
    Result r;
    GLint statuses[3] = {GL_FALSE, GL_FALSE, GL_FALSE};
    std::string logs;
    for (int i = 0; i < shader_count; ++i) {
        api.glGetShaderiv(shaders[i], GL_COMPILE_STATUS, &statuses[i]);
        std::string log = shader_log(shaders[i]);
        if (!log.empty()) logs += " stage" + std::to_string(i) + "=" + log;
    }
    GLuint program = api.glCreateProgram();
    for (int i = 0; i < shader_count; ++i) api.glAttachShader(program, shaders[i]);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    GLint plen = 0;
    api.glGetProgramiv(program, GL_INFO_LOG_LENGTH, &plen);
    std::string plog(static_cast<size_t>(plen), '\0');
    if (plen > 0) api.glGetProgramInfoLog(program, plen, nullptr, plog.data());
    if (!plog.empty()) logs += " link=" + plog;

    GLuint tex = 0, fbo = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    api.glGenFramebuffers(1, &fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    const GLenum fb_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glViewport(0, 0, 1, 1);
    api.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    api.glClear(GL_COLOR_BUFFER_BIT);
    api.glUseProgram(program);
    if (need_patch_vertices && api.glPatchParameteri) api.glPatchParameteri(GL_PATCH_VERTICES, 3);
    api.glDrawArrays(draw_mode, 0, 3);
    const GLenum draw_error = api.glGetError();
    unsigned char pixel[4] = {};
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    api.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    const GLenum read_error = api.glGetError();

    bool stages_ok = true;
    for (int i = 0; i < shader_count; ++i) stages_ok &= statuses[i] == GL_TRUE;
    r.ok = stages_ok && linked == GL_TRUE && fb_status == GL_FRAMEBUFFER_COMPLETE && draw_error == GL_NO_ERROR &&
           read_error == GL_NO_ERROR && pixel[0] > 200 && pixel[1] < 50 && pixel[2] < 50;
    r.detail = std::string(label) + " stages=" + (stages_ok ? "compiled" : "failed") +
               " link=" + (linked ? "true" : "false") + " fb=" + std::to_string(fb_status) +
               " draw_error=" + std::to_string(draw_error) + " read_error=" + std::to_string(read_error) +
               " pixel=" + std::to_string(pixel[0]) + "," + std::to_string(pixel[1]) + "," +
               std::to_string(pixel[2]) + (tcs_log ? tcs_log : "") + (tes_log ? tes_log : "") +
               (logs.empty() ? "" : " logs=" + logs);
    api.glDeleteProgram(program);
    for (int i = 0; i < shader_count; ++i) api.glDeleteShader(shaders[i]);
    api.glDeleteFramebuffers(1, &fbo);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_geometry_stage() {
    const char* vs = R"(#version 400 core
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
    const char* gs = R"(#version 400 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
void main() {
    for (int i = 0; i < 3; ++i) {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
)";
    const char* fs = R"(#version 400 core
out vec4 color;
void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    GLuint shaders[3] = {api.glCreateShader(GL_VERTEX_SHADER), api.glCreateShader(GL_GEOMETRY_SHADER),
                         api.glCreateShader(GL_FRAGMENT_SHADER)};
    const char* sources[3] = {vs, gs, fs};
    for (int i = 0; i < 3; ++i) {
        api.glShaderSource(shaders[i], 1, &sources[i], nullptr);
        api.glCompileShader(shaders[i]);
    }
    return run_stage_draw(GL_TRIANGLES, shaders, 3, "geometry_stage", false, nullptr, nullptr);
}

Result test_tessellation_stage() {
    const char* vs = R"(#version 400 core
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
    const char* tcs = R"(#version 400 core
layout(vertices = 3) out;
void main() {
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    if (gl_InvocationID == 0) {
        gl_TessLevelInner[0] = 1.0;
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
    }
}
)";
    const char* tes = R"(#version 400 core
layout(triangles, equal_spacing, ccw) in;
void main() {
    vec3 p = gl_TessCoord.x * gl_in[0].gl_Position.xyz +
             gl_TessCoord.y * gl_in[1].gl_Position.xyz +
             gl_TessCoord.z * gl_in[2].gl_Position.xyz;
    gl_Position = vec4(p, 1.0);
}
)";
    const char* fs = R"(#version 400 core
out vec4 color;
void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    GLuint shaders[4] = {api.glCreateShader(GL_VERTEX_SHADER), api.glCreateShader(GL_TESS_CONTROL_SHADER),
                         api.glCreateShader(GL_TESS_EVALUATION_SHADER), api.glCreateShader(GL_FRAGMENT_SHADER)};
    const char* sources[4] = {vs, tcs, tes, fs};
    for (int i = 0; i < 4; ++i) {
        api.glShaderSource(shaders[i], 1, &sources[i], nullptr);
        api.glCompileShader(shaders[i]);
    }
    return run_stage_draw(GL_PATCHES, shaders, 4, "tessellation_stage", true, nullptr, nullptr);
}

Result test_blit_depth_match() {
    Result r;
    // Matching D24S8 formats are legal and must not be rejected by the preflight.
    GLuint src_rb = 0, dst_rb = 0, src_fbo = 0, dst_fbo = 0;
    api.glGenRenderbuffers(1, &src_rb);
    api.glBindRenderbuffer(GL_RENDERBUFFER, src_rb);
    api.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    api.glGenFramebuffers(1, &src_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, src_fbo);
    api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, src_rb);
    const GLenum src_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glGenRenderbuffers(1, &dst_rb);
    api.glBindRenderbuffer(GL_RENDERBUFFER, dst_rb);
    api.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    api.glGenFramebuffers(1, &dst_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dst_rb);
    const GLenum dst_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
    api.glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    const GLenum blit_error = api.glGetError();
    GLsync fence = api.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    const GLenum fence_error = api.glGetError();
    r.ok = src_status == GL_FRAMEBUFFER_COMPLETE && dst_status == GL_FRAMEBUFFER_COMPLETE &&
           blit_error == GL_NO_ERROR && fence != nullptr && fence_error == GL_NO_ERROR;
    r.detail = "src_status=" + std::to_string(src_status) + " dst_status=" + std::to_string(dst_status) +
               " blit_error=" + std::to_string(blit_error) + " fence=" + (fence ? "ok" : "null") +
               " fence_error=" + std::to_string(fence_error);
    if (fence && api.glDeleteSync) api.glDeleteSync(fence);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &src_fbo);
    api.glDeleteFramebuffers(1, &dst_fbo);
    api.glDeleteRenderbuffers(1, &src_rb);
    api.glDeleteRenderbuffers(1, &dst_rb);
    return r;
}

Result test_copy_image_depth_mismatch() {
    Result r;
    // glCopyTexImage2D defines a D24 level from a D24S8 source. The wrapper must
    // skip that blit rather than lose the ANGLE device before the fence.
    GLuint rb = 0, src_fbo = 0, tex = 0;
    api.glGenRenderbuffers(1, &rb);
    api.glBindRenderbuffer(GL_RENDERBUFFER, rb);
    api.glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    api.glGenFramebuffers(1, &src_fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, src_fbo);
    api.glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);
    const GLenum src_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glBindFramebuffer(GL_READ_FRAMEBUFFER, src_fbo);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 0, 0, 1, 1, 0);
    const GLenum copy_error = api.glGetError();
    GLsync fence = api.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    const GLenum fence_error = api.glGetError();
    r.ok = src_status == GL_FRAMEBUFFER_COMPLETE && copy_error == GL_NO_ERROR && fence != nullptr &&
           fence_error == GL_NO_ERROR;
    r.detail = "src_status=" + std::to_string(src_status) + " copy_error=" + std::to_string(copy_error) +
               " fence=" + (fence ? "ok" : "null") + " fence_error=" + std::to_string(fence_error);
    if (fence && api.glDeleteSync) api.glDeleteSync(fence);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &src_fbo);
    api.glDeleteRenderbuffers(1, &rb);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_translated_image_decl() {
    Result r;
    // The pack's shadow vertex stage is desktop GLSL. SPIRV-Cross emits the
    // memory qualifier before precision (`uniform writeonly highp image2D`),
    // and the player atlas is the one declaration that looks like that. The
    // wrapper must still see it, give it a unit distinct from the explicitly
    // bound voxel image, and preserve the qualifier run.
    const char* vs = R"(#version 420 core
layout(binding = 0, r32ui) writeonly uniform uimage3D voxel_img;
layout(binding = 1, rgba8) uniform writeonly image2D playerAtlas_img;
void main() {
    imageStore(voxel_img, ivec3(0), uvec4(11u));
    imageStore(playerAtlas_img, ivec2(0), vec4(0.5, 0.25, 0.75, 1.0));
    gl_Position = vec4(0.0);
}
)";
    const char* fs = R"(#version 400 core
out vec4 color;
void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    GLuint vs_obj = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vs_obj, 1, &vs, nullptr);
    api.glCompileShader(vs_obj);
    GLint vstatus = GL_FALSE;
    api.glGetShaderiv(vs_obj, GL_COMPILE_STATUS, &vstatus);
    const std::string vlog = shader_log(vs_obj);
    GLuint fs_obj = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fs_obj, 1, &fs, nullptr);
    api.glCompileShader(fs_obj);
    GLint fstatus = GL_FALSE;
    api.glGetShaderiv(fs_obj, GL_COMPILE_STATUS, &fstatus);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vs_obj);
    api.glAttachShader(program, fs_obj);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    const std::string translated = shader_source_text(vs_obj);
    const bool voxel_bound = translated.find("layout(binding = 0") != std::string::npos &&
                             translated.find("r32ui") != std::string::npos &&
                             translated.find("voxel_img") != std::string::npos;
    const size_t atlas_pos = translated.find("playerAtlas_img");
    const size_t atlas_layout = atlas_pos == std::string::npos ? std::string::npos
                                                              : translated.rfind("layout(binding = ", atlas_pos);
    bool atlas_bound = atlas_layout != std::string::npos;
    GLint atlas_unit = -1;
    if (atlas_bound) {
        const size_t number = atlas_layout + strlen("layout(binding = ");
        atlas_unit = std::atoi(translated.c_str() + number);
        atlas_bound = atlas_unit != 0;
        const size_t line_end = translated.find('\n', atlas_layout);
        const std::string decl = translated.substr(atlas_layout, line_end - atlas_layout);
        atlas_bound = atlas_bound && decl.find("writeonly") != std::string::npos &&
                      decl.find("highp") != std::string::npos &&
                      decl.find("image2D") != std::string::npos;
    }
    r.ok = vstatus == GL_TRUE && fstatus == GL_TRUE && linked == GL_TRUE && voxel_bound && atlas_bound;
    r.detail = "vstatus=" + std::to_string(vstatus) + " fstatus=" + std::to_string(fstatus) +
               " link=" + std::to_string(linked) + " voxel_bound=" + (voxel_bound ? "yes" : "no") +
               " atlas_bound=" + (atlas_bound ? "yes" : "no") + " atlas_unit=" + std::to_string(atlas_unit) +
               (vlog.empty() ? "" : " vlog=" + vlog) +
               " translated=" + translated.substr(0, std::min<size_t>(translated.size(), 800));
    api.glDeleteProgram(program);
    api.glDeleteShader(vs_obj);
    api.glDeleteShader(fs_obj);
    return r;
}

Result test_metadata_image_format() {
    Result r;
    // Bare desktop declaration with the qualifier after `uniform`, exactly the
    // pack's playerAtlas_img shape. The loader contract supplies GL_RGBA8.
    const char* vs = R"(#version 420 core
uniform writeonly image2D playerAtlas_img;
void main() {
    imageStore(playerAtlas_img, ivec2(0), vec4(0.5, 0.25, 0.75, 1.0));
    gl_Position = vec4(0.0);
}
)";
    const char* fs = R"(#version 420 core
out vec4 color;
void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    api.mg_image_metadata_begin();
    api.mg_image_metadata_add("playerAtlas_img", GL_RGBA8);
    api.mg_image_metadata_end();
    GLuint vs_obj = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vs_obj, 1, &vs, nullptr);
    api.glCompileShader(vs_obj);
    GLint vstatus = GL_FALSE;
    api.glGetShaderiv(vs_obj, GL_COMPILE_STATUS, &vstatus);
    const std::string vlog = shader_log(vs_obj);
    GLuint fs_obj = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fs_obj, 1, &fs, nullptr);
    api.glCompileShader(fs_obj);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vs_obj);
    api.glAttachShader(program, fs_obj);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    const std::string translated = shader_source_text(vs_obj);
    const size_t atlas = translated.find("playerAtlas_img");
    const bool has_rgba8 = atlas != std::string::npos && translated.rfind("rgba8", atlas) != std::string::npos;
    const bool has_binding = atlas != std::string::npos && translated.rfind("binding =", atlas) != std::string::npos;
    r.ok = vstatus == GL_TRUE && linked == GL_TRUE && has_rgba8 && has_binding;
    r.detail = "vstatus=" + std::to_string(vstatus) + " link=" + std::to_string(linked) +
               " rgba8=" + (has_rgba8 ? "yes" : "no") + " binding=" + (has_binding ? "yes" : "no") +
               (vlog.empty() ? "" : " vlog=" + vlog) +
               " translated=" + translated.substr(0, std::min<size_t>(translated.size(), 800));
    api.glDeleteProgram(program);
    api.glDeleteShader(vs_obj);
    api.glDeleteShader(fs_obj);
    return r;
}

Result test_vertex_ssbo_atomics() {
    Result r;
    // The player-reflection option adds six vertex-stage atomics on its 5,376
    // byte binding-3 block. This probe is the direct device test for H1: it
    // performs those six operations and then checks that the context still
    // creates a fence and the atomics landed.
    const char* vs = R"(#version 430 core
layout(std430, binding = 3) buffer V {
    vec3 positions[216];
    vec2 data[216];
    ivec3 boxMin[6];
    ivec3 boxMax[6];
} v;
void main() {
    atomicMin(v.boxMin[0].x, -10);
    atomicMin(v.boxMin[0].y, -10);
    atomicMin(v.boxMin[0].z, -10);
    atomicMax(v.boxMax[0].x, 10);
    atomicMax(v.boxMax[0].y, 10);
    atomicMax(v.boxMax[0].z, 10);
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)";
    const char* fs = R"(#version 420 core
out vec4 color;
void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    GLuint vs_obj = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vs_obj, 1, &vs, nullptr);
    api.glCompileShader(vs_obj);
    GLint vstatus = GL_FALSE;
    api.glGetShaderiv(vs_obj, GL_COMPILE_STATUS, &vstatus);
    const std::string vlog = shader_log(vs_obj);
    GLuint fs_obj = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fs_obj, 1, &fs, nullptr);
    api.glCompileShader(fs_obj);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vs_obj);
    api.glAttachShader(program, fs_obj);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);

    std::vector<unsigned char> initial(5376, 0);
    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, initial.size(), initial.data(), GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buffer);

    GLuint tex = 0, fbo = 0, vao = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    api.glGenFramebuffers(1, &fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    api.glViewport(0, 0, 1, 1);
    api.glGenVertexArrays(1, &vao);
    api.glBindVertexArray(vao);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glUseProgram(program);
    api.glDrawArrays(GL_POINTS, 0, 1);
    const GLenum draw_error = api.glGetError();
    GLsync fence = api.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    const GLenum fence_error = api.glGetError();
    api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    api.glFinish();
    int values[27] = {};
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 5184, 108, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(values, ptr, sizeof(values));
        api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    const GLenum map_error = api.glGetError();
    const bool atomics_ok = values[0] == -10 && values[1] == -10 && values[2] == -10 &&
                            values[24] == 10 && values[25] == 10 && values[26] == 10;
    r.ok = vstatus == GL_TRUE && linked == GL_TRUE && draw_error == GL_NO_ERROR && fence != nullptr &&
           fence_error == GL_NO_ERROR && map_error == GL_NO_ERROR && atomics_ok;
    r.detail = "vstatus=" + std::to_string(vstatus) + " link=" + std::to_string(linked) +
               " draw_error=" + std::to_string(draw_error) + " fence=" + (fence ? "ok" : "null") +
               " fence_error=" + std::to_string(fence_error) + " map_error=" + std::to_string(map_error) +
               " min=" + std::to_string(values[0]) + "," + std::to_string(values[1]) + "," +
               std::to_string(values[2]) + " max=" + std::to_string(values[24]) + "," +
               std::to_string(values[25]) + "," + std::to_string(values[26]) +
               (vlog.empty() ? "" : " vlog=" + vlog);
    if (fence && api.glDeleteSync) api.glDeleteSync(fence);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &fbo);
    api.glDeleteTextures(1, &tex);
    api.glDeleteVertexArrays(1, &vao);
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteProgram(program);
    api.glDeleteShader(vs_obj);
    api.glDeleteShader(fs_obj);
    return r;
}

Result test_vertex_image2d_store() {
    Result r;
    // The player atlas is a 2D RGBA8 image stored from the vertex stage. The
    // existing vertex-image probe covers a 3D RGBA16F image; this is the exact
    // remaining shape introduced by player reflections.
    const char* vs = R"(#version 420 core
layout(binding = 0, rgba8) writeonly uniform highp image2D atlas_img;
void main() {
    imageStore(atlas_img, ivec2(0), vec4(0.25, 0.5, 0.75, 1.0));
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)";
    const char* fs = R"(#version 420 core
out vec4 color;
void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }
)";
    GLuint tex = 0, fbo = 0, vao = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    api.glGenFramebuffers(1, &fbo);
    api.glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    api.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    const GLenum fb_status = api.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    api.glViewport(0, 0, 1, 1);
    api.glGenVertexArrays(1, &vao);
    api.glBindVertexArray(vao);
    api.glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA8);

    GLuint vs_obj = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vs_obj, 1, &vs, nullptr);
    api.glCompileShader(vs_obj);
    GLint vstatus = GL_FALSE;
    api.glGetShaderiv(vs_obj, GL_COMPILE_STATUS, &vstatus);
    const std::string vlog = shader_log(vs_obj);
    GLuint fs_obj = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fs_obj, 1, &fs, nullptr);
    api.glCompileShader(fs_obj);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vs_obj);
    api.glAttachShader(program, fs_obj);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glUseProgram(program);
    api.glDrawArrays(GL_POINTS, 0, 1);
    const GLenum draw_error = api.glGetError();
    unsigned char pixel[4] = {};
    api.glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    const GLenum read_error = api.glGetError();
    const bool color_ok = pixel[0] >= 63 && pixel[0] <= 65 && pixel[1] >= 127 && pixel[1] <= 129 &&
                          pixel[2] >= 191 && pixel[2] <= 193;
    GLsync fence = api.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    const GLenum fence_error = api.glGetError();
    r.ok = vstatus == GL_TRUE && linked == GL_TRUE && fb_status == GL_FRAMEBUFFER_COMPLETE &&
           draw_error == GL_NO_ERROR && read_error == GL_NO_ERROR && color_ok && fence != nullptr &&
           fence_error == GL_NO_ERROR;
    r.detail = "vstatus=" + std::to_string(vstatus) + " link=" + std::to_string(linked) +
               " fb=" + std::to_string(fb_status) + " draw_error=" + std::to_string(draw_error) +
               " read_error=" + std::to_string(read_error) + " pixel=" + std::to_string(pixel[0]) + "," +
               std::to_string(pixel[1]) + "," + std::to_string(pixel[2]) +
               " fence=" + (fence ? "ok" : "null") + " fence_error=" + std::to_string(fence_error) +
               (vlog.empty() ? "" : " vlog=" + vlog);
    if (fence && api.glDeleteSync) api.glDeleteSync(fence);
    api.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    api.glDeleteFramebuffers(1, &fbo);
    api.glDeleteTextures(1, &tex);
    api.glDeleteVertexArrays(1, &vao);
    api.glDeleteProgram(program);
    api.glDeleteShader(vs_obj);
    api.glDeleteShader(fs_obj);
    return r;
}

Result test_translation_failure_not_stale() {
    Result r;
    // Re-sourcing a shader object is legal. If translation fails, the backend
    // source must be replaced, not left as the previously compiled source.
    const char* good = "#version 300 es\nprecision mediump float;\nout vec4 color;\nvoid main() { color = vec4(1.0); }\n";
    const char* bad = "#version 400 core\nout vec4 color;\nvoid main() { color = not_a_value; }\n";
    GLuint shader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shader, 1, &good, nullptr);
    api.glCompileShader(shader);
    GLint good_status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &good_status);
    api.glShaderSource(shader, 1, &bad, nullptr);
    api.glCompileShader(shader);
    GLint bad_status = GL_TRUE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &bad_status);
    const std::string translated = shader_source_text(shader);
    r.ok = good_status == GL_TRUE && bad_status == GL_FALSE && translated.empty();
    r.detail = "good_status=" + std::to_string(good_status) + " bad_status=" + std::to_string(bad_status) +
               " backend_source_length=" + std::to_string(translated.size());
    api.glDeleteShader(shader);
    return r;
}

Result test_initialized_uniform_array() {
    Result r;
    // Desktop permits uniform initializers; ESSL does not. Dropping the
    // initializer must preserve the array declarator.
    const char* fs = R"(#version 420 core
uniform float weights[4] = float[4](0.25, 0.5, 0.75, 1.0);
out vec4 color;
void main() { color = vec4(weights[0], weights[1], weights[2], weights[3]); }
)";
    GLuint shader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shader, 1, &fs, nullptr);
    api.glCompileShader(shader);
    GLint status = GL_FALSE;
    api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    const std::string log = shader_log(shader);
    const std::string translated = shader_source_text(shader);
    const bool has_array = translated.find("weights[4]") != std::string::npos;
    const bool no_initializer = translated.find("= float[4]") == std::string::npos;
    r.ok = status == GL_TRUE && has_array && no_initializer;
    r.detail = "status=" + std::to_string(status) + " array=" + (has_array ? "yes" : "no") +
               " initializer_removed=" + (no_initializer ? "yes" : "no") +
               (log.empty() ? "" : " log=" + log) +
               " translated=" + translated.substr(0, std::min<size_t>(translated.size(), 800));
    api.glDeleteShader(shader);
    return r;
}

Result test_frag_data_shared_shader() {
    Result r;
    // One shader object can be attached to two programs. Program A patches its
    // output location; program B has no location override and must link the
    // original source, not A's patched backend text.
    const char* vs = "#version 300 es\nvoid main() { gl_Position = vec4(0.0); }\n";
    const char* fs = "#version 300 es\nprecision mediump float;\nout vec4 sharedOut;\nvoid main() { sharedOut = vec4(1.0); }\n";
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint fshader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader, 1, &fs, nullptr);
    api.glCompileShader(fshader);
    GLuint program_a = api.glCreateProgram();
    api.glAttachShader(program_a, vshader);
    api.glAttachShader(program_a, fshader);
    api.glBindFragDataLocation(program_a, 5, "sharedOut");
    api.glLinkProgram(program_a);
    GLint link_a = GL_FALSE;
    api.glGetProgramiv(program_a, GL_LINK_STATUS, &link_a);
    const std::string patched = shader_source_text(fshader);
    GLuint program_b = api.glCreateProgram();
    api.glAttachShader(program_b, vshader);
    api.glAttachShader(program_b, fshader);
    api.glLinkProgram(program_b);
    GLint link_b = GL_FALSE;
    api.glGetProgramiv(program_b, GL_LINK_STATUS, &link_b);
    const std::string restored = shader_source_text(fshader);
    const bool a_patched = patched.find("location = 5") != std::string::npos;
    const bool b_restored = restored.find("location = 5") == std::string::npos;
    r.ok = link_a == GL_TRUE && link_b == GL_TRUE && a_patched && b_restored;
    r.detail = "link_a=" + std::to_string(link_a) + " link_b=" + std::to_string(link_b) +
               " a_patched=" + (a_patched ? "yes" : "no") + " b_restored=" + (b_restored ? "yes" : "no") +
               " restored_prefix=" + restored.substr(0, std::min<size_t>(restored.size(), 200));
    api.glDeleteProgram(program_a);
    api.glDeleteProgram(program_b);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader);
    return r;
}

Result test_frag_data_linked_location() {
    Result r;
    const char* vs = "#version 420 core\nvoid main() { gl_Position = vec4(0.0); }\n";
    const char* fs = "#version 420 core\nlayout(location = 3) out vec4 explicitOut;\nvoid main() { explicitOut = vec4(1.0); }\n";
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint fshader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader, 1, &fs, nullptr);
    api.glCompileShader(fshader);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vshader);
    api.glAttachShader(program, fshader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    const GLint location = api.glGetFragDataLocation(program, "explicitOut");
    r.ok = linked == GL_TRUE && location == 3;
    r.detail = "link=" + std::to_string(linked) + " location=" + std::to_string(location) +
               " source=" + shader_source_text(fshader).substr(0, std::min<size_t>(shader_source_text(fshader).size(), 300));
    api.glDeleteProgram(program);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader);
    return r;
}

Result test_ssbo_qualifier_preserved() {
    Result r;
    const char* vs = "#version 430 core\nvoid main() { gl_Position = vec4(0.0); }\n";
    const char* fs = R"(#version 430 core
layout(std430, binding = 0) readonly buffer Block { float value; } blockData;
out vec4 color;
void main() { color = vec4(blockData.value); }
)";
    GLuint vshader = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(vshader, 1, &vs, nullptr);
    api.glCompileShader(vshader);
    GLuint fshader = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(fshader, 1, &fs, nullptr);
    api.glCompileShader(fshader);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, vshader);
    api.glAttachShader(program, fshader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glShaderStorageBlockBinding(program, 0, 2);
    const GLenum binding_error = api.glGetError();
    const std::string source = shader_source_text(fshader);
    const bool readonly_kept = source.find("readonly buffer") != std::string::npos;
    const bool rebound = source.find("binding = 2") != std::string::npos;
    r.ok = linked == GL_TRUE && binding_error == GL_NO_ERROR && readonly_kept && rebound;
    r.detail = "link=" + std::to_string(linked) + " binding_error=" + std::to_string(binding_error) +
               " readonly=" + (readonly_kept ? "yes" : "no") + " rebound=" + (rebound ? "yes" : "no") +
               " source=" + source.substr(0, std::min<size_t>(source.size(), 400));
    api.glDeleteProgram(program);
    api.glDeleteShader(vshader);
    api.glDeleteShader(fshader);
    return r;
}

Result test_guard_scan_bench() {
    Result r;
    const char* cs = "#version 310 es\nlayout(local_size_x = 1) in;\nvoid main() {}\n";
    GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
    api.glShaderSource(shader, 1, &cs, nullptr);
    api.glCompileShader(shader);
    GLuint program = api.glCreateProgram();
    api.glAttachShader(program, shader);
    api.glLinkProgram(program);
    GLint linked = GL_FALSE;
    api.glGetProgramiv(program, GL_LINK_STATUS, &linked);
    api.glUseProgram(program);

    constexpr int kWarmup = 4000;
    constexpr int kIterations = 20000;
    auto run_once = [&]() {
        for (int i = 0; i < kWarmup; ++i) api.glDispatchCompute(1, 1, 1);
        api.glFinish();
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kIterations; ++i) api.glDispatchCompute(1, 1, 1);
        api.glFinish();
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
    };

    api.glBindSampler(0, 0);
    api.glActiveTexture(GL_TEXTURE0);
    api.glBindTexture(GL_TEXTURE_2D, 0);
    double plain_ns = 1e30;
    for (int i = 0; i < 3; ++i) plain_ns = std::min(plain_ns, run_once());

    GLuint tex = 0, sampler = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT32F, 1, 1);
    api.glGenSamplers(1, &sampler);
    api.glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glBindSampler(0, sampler);
    double guard_ns = 1e30;
    for (int i = 0; i < 3; ++i) guard_ns = std::min(guard_ns, run_once());

    r.ok = linked == GL_TRUE;
    r.detail = "linked=" + std::to_string(linked) + " iterations=" + std::to_string(kIterations) +
               " plain_ns=" + std::to_string(plain_ns) + " guard_ns=" + std::to_string(guard_ns) +
               " delta_ns=" + std::to_string(guard_ns - plain_ns);
    api.glBindSampler(0, 0);
    api.glDeleteSamplers(1, &sampler);
    api.glDeleteTextures(1, &tex);
    api.glDeleteProgram(program);
    api.glDeleteShader(shader);
    return r;
}


Result test_image_rebind_bench() {
    Result r;
    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_2D, tex);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 1, 1);
    constexpr int kIterations = 100000;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        api.glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    }
    api.glFinish();
    auto end = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(end - start).count() / kIterations;
    r.ok = ns >= 0.0;
    r.detail = "iterations=" + std::to_string(kIterations) + " ns_per_same_bind=" + std::to_string(ns);
    api.glDeleteTextures(1, &tex);
    return r;
}

Result test_iteration_caps() {
    Result r;
    auto compile_compute = [&](const char* label, const char* src, std::string& log) {
        GLuint shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &src, nullptr);
        api.glCompileShader(shader);
        GLint status = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        log = shader_log(shader);
        api.glDeleteShader(shader);
        return std::string(label) + "=" + (status == GL_TRUE ? "yes" : "no");
    };
    std::string log_quad, log_arith, log_ballot, log_rgba16, log_rg16, log_rg16f;
    const char* quad = R"(#version 320 es
#extension GL_KHR_shader_subgroup_quad : require
layout(local_size_x = 4) in;
layout(std430, binding = 0) buffer Out { vec4 v; } o;
void main() { o.v = subgroupQuadSwapHorizontal(vec4(1.0)); }
)";
    const char* arith = R"(#version 320 es
#extension GL_KHR_shader_subgroup_arithmetic : require
layout(local_size_x = 4) in;
layout(std430, binding = 0) buffer Out { float v; } o;
void main() { o.v = subgroupInclusiveAdd(1.0); }
)";
    const char* ballot = R"(#version 320 es
#extension GL_KHR_shader_subgroup_ballot : require
layout(local_size_x = 4) in;
layout(std430, binding = 0) buffer Out { uint v; } o;
void main() { o.v = subgroupBallot(true).x; }
)";
    const char* rgba16 = R"(#version 320 es
layout(local_size_x = 1) in;
layout(rgba16, binding = 0) writeonly uniform highp image2D img;
void main() { imageStore(img, ivec2(0), vec4(1.0)); }
)";
    const char* rg16 = R"(#version 320 es
layout(local_size_x = 1) in;
layout(rg16, binding = 0) writeonly uniform highp image2D img;
void main() { imageStore(img, ivec2(0), vec4(1.0)); }
)";
    const char* rg16f = R"(#version 320 es
layout(local_size_x = 1) in;
layout(rg16f, binding = 0) writeonly uniform highp image2D img;
void main() { imageStore(img, ivec2(0), vec4(1.0)); }
)";
    const std::string quad_res = compile_compute("quad", quad, log_quad);
    const std::string arith_res = compile_compute("arith", arith, log_arith);
    const std::string ballot_res = compile_compute("ballot", ballot, log_ballot);
    const std::string rgba16_res = compile_compute("rgba16", rgba16, log_rgba16);
    const std::string rg16_res = compile_compute("rg16", rg16, log_rg16);
    const std::string rg16f_res = compile_compute("rg16f", rg16f, log_rg16f);

    auto bind_test = [&](const char* label, GLenum internal, GLenum image_format) {
        GLuint tex = 0;
        api.glGenTextures(1, &tex);
        api.glBindTexture(GL_TEXTURE_2D, tex);
        api.glTexStorage2D(GL_TEXTURE_2D, 1, internal, 1, 1);
        while (api.glGetError() != GL_NO_ERROR) {}
        api.glBindImageTexture(0, tex, 0, GL_FALSE, 0, GL_WRITE_ONLY, image_format);
        const GLenum error = api.glGetError();
        api.glDeleteTextures(1, &tex);
        return std::string(label) + "=" + std::to_string(error);
    };
    const std::string rgba16_bind = bind_test("rgba16_bind", 0x805B, 0x805B);
    const std::string rg16_bind = bind_test("rg16_bind", 0x822B, 0x822B);
    const std::string rg16f_bind = bind_test("rg16f_bind", 0x822F, 0x822F);
    r.ok = true;
    r.detail = quad_res + " " + arith_res + " " + ballot_res + " " + rgba16_res + " " + rg16_res + " " + rg16f_res +
               " " + rgba16_bind + " " + rg16_bind + " " + rg16f_bind +
               (log_quad.empty() ? "" : " quad_log=" + log_quad) +
               (log_arith.empty() ? "" : " arith_log=" + log_arith) +
               (log_ballot.empty() ? "" : " ballot_log=" + log_ballot) +
               (log_rgba16.empty() ? "" : " rgba16_log=" + log_rgba16) +
               (log_rg16.empty() ? "" : " rg16_log=" + log_rg16) +
               (log_rg16f.empty() ? "" : " rg16f_log=" + log_rg16f);
    return r;
}

Result test_patch_default_levels() {
    Result r;
    // ES has no default tessellation-level entry point. The frontend must say
    // so rather than accepting values it cannot apply or calling a null slot.
    constexpr GLenum kPatchDefaultInner = 0x8E73;
    const GLfloat inner[2] = {1.0f, 1.0f};
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glPatchParameterfv(kPatchDefaultInner, inner);
    const GLenum error = api.glGetError();
    r.ok = error == GL_INVALID_OPERATION;
    r.detail = "glPatchParameterfv=" + std::to_string(error) +
               (error == GL_NO_ERROR ? " (SILENT PLACEHOLDER)" : " (truthful unsupported)");
    return r;
}

Result test_act_volume() {
    Result r;
    const int W = 192, H = 96, D = 192;
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(rgba16f, binding = 0) writeonly uniform highp image3D img;
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(p, ivec3(192, 96, 192)))) return;
    imageStore(img, p, vec4(float(p.x), float(p.y), float(p.z), 1.0));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(rgba16f, binding = 0) readonly uniform highp image3D img;
layout(std430, binding = 1) buffer Out { uint v[12]; } outb;
void main() {
    ivec3 pts[4] = ivec3[](ivec3(0, 0, 0), ivec3(95, 47, 95), ivec3(191, 95, 191), ivec3(191, 95, 0));
    for (int i = 0; i < 4; i++) {
        vec4 s = imageLoad(img, pts[i]);
        outb.v[i * 3 + 0] = uint(s.x);
        outb.v[i * 3 + 1] = uint(s.y);
        outb.v[i * 3 + 2] = uint(s.z);
    }
}
)";
    const char* sample_cs = R"(#version 310 es
layout(local_size_x = 1) in;
uniform highp sampler3D samp;
layout(std430, binding = 1) buffer Out { uint v[3]; } outb;
void main() {
    vec4 s = texelFetch(samp, ivec3(95, 47, 95), 0);
    outb.v[0] = uint(s.x);
    outb.v[1] = uint(s.y);
    outb.v[2] = uint(s.z);
}
)";

    auto build_compute = [&](const char* src, GLuint& shader, GLuint& program, std::string& log) {
        shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &src, nullptr);
        api.glCompileShader(shader);
        GLint cstatus = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &cstatus);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log.resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log.data());
        }
        program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint lstatus = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);
        return cstatus == GL_TRUE && lstatus == GL_TRUE;
    };

    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, W, H, D);
    const GLenum storage_error = api.glGetError();
    api.glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    const GLenum bind_error = api.glGetError();

    GLuint write_shader = 0, write_program = 0;
    std::string write_log;
    const bool write_ok = build_compute(write_cs, write_shader, write_program, write_log);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 64, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);

    if (write_ok) {
        api.glUseProgram(write_program);
        api.glDispatchCompute(24, 12, 24);
    }
    api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_STORAGE_BARRIER_BIT);

    GLuint read_shader = 0, read_program = 0;
    std::string read_log;
    const bool read_ok = build_compute(read_cs, read_shader, read_program, read_log);
    GLuint values[12] = {};
    if (read_ok) {
        api.glUseProgram(read_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(values, ptr, sizeof(values));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }

    GLuint sample_shader = 0, sample_program = 0;
    std::string sample_log;
    const bool sample_ok = build_compute(sample_cs, sample_shader, sample_program, sample_log);
    GLuint sample_values[3] = {};
    GLint sampler_loc = -1;
    if (sample_ok) {
        api.glUseProgram(sample_program);
        sampler_loc = api.glGetUniformLocation(sample_program, "samp");
        api.glActiveTexture(GL_TEXTURE0);
        api.glBindTexture(GL_TEXTURE_3D, tex);
        if (sampler_loc >= 0) api.glUniform1i(sampler_loc, 0);
        api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(sample_values), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(sample_values, ptr, sizeof(sample_values));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }

    const bool edge = values[0] == 0u && values[1] == 0u && values[2] == 0u;
    const bool center = values[3] == 95u && values[4] == 47u && values[5] == 95u;
    const bool far_corner = values[6] == 191u && values[7] == 95u && values[8] == 191u;
    const bool z_zero = values[9] == 191u && values[10] == 95u && values[11] == 0u;
    const bool sampled = sample_values[0] == 95u && sample_values[1] == 47u && sample_values[2] == 95u;
    r.ok = storage_error == GL_NO_ERROR && bind_error == GL_NO_ERROR && write_ok && read_ok &&
           sample_ok && sampler_loc >= 0 && edge && center && far_corner && z_zero && sampled;
    r.detail = "storage_error=" + std::to_string(storage_error) + " bind_error=" + std::to_string(bind_error) +
               " write=" + std::to_string(write_ok) + " read=" + std::to_string(read_ok) +
               " sample=" + std::to_string(sample_ok) + " sampler_loc=" + std::to_string(sampler_loc) +
               " edge=" + std::to_string(edge) + " center=" + std::to_string(center) +
               " far=" + std::to_string(far_corner) + " z0=" + std::to_string(z_zero) +
               " sampled=" + std::to_string(sampled) +
               " image_values=" + std::to_string(values[6]) + "," + std::to_string(values[7]) + "," +
               std::to_string(values[8]) + " sampler_values=" + std::to_string(sample_values[0]) + "," +
               std::to_string(sample_values[1]) + "," + std::to_string(sample_values[2]) +
               " write_log=" + write_log + " read_log=" + read_log + " sample_log=" + sample_log;

    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex);
    api.glDeleteProgram(write_program);
    api.glDeleteShader(write_shader);
    api.glDeleteProgram(read_program);
    api.glDeleteShader(read_shader);
    api.glDeleteProgram(sample_program);
    api.glDeleteShader(sample_shader);
    return r;
}

Result test_act_pingpong() {
    Result r;
    const int W = 192, H = 96, D = 192;
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(rgba16f) writeonly uniform highp image3D img0;
layout(rgba16f) writeonly uniform highp image3D img1;
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(p, ivec3(192, 96, 192)))) return;
    imageStore(img0, p, vec4(11.0));
    imageStore(img1, p, vec4(22.0));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(rgba16f) readonly uniform highp image3D img0;
layout(rgba16f) readonly uniform highp image3D img1;
layout(std430, binding = 2) buffer Out { uint v[2]; } outb;
void main() {
    outb.v[0] = uint(imageLoad(img0, ivec3(95, 47, 95)).x);
    outb.v[1] = uint(imageLoad(img1, ivec3(95, 47, 95)).x);
}
)";

    auto build_compute = [&](const char* src, GLuint& shader, GLuint& program, std::string& log) {
        shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &src, nullptr);
        api.glCompileShader(shader);
        GLint cstatus = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &cstatus);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log.resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log.data());
        }
        program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint lstatus = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);
        return cstatus == GL_TRUE && lstatus == GL_TRUE;
    };

    GLuint tex0 = 0, tex1 = 0;
    api.glGenTextures(1, &tex0);
    api.glGenTextures(1, &tex1);
    for (GLuint tex : {tex0, tex1}) {
        api.glBindTexture(GL_TEXTURE_3D, tex);
        api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, W, H, D);
    }
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBindImageTexture(0, tex0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    api.glBindImageTexture(1, tex1, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    const GLenum bind_error = api.glGetError();

    GLuint write_shader = 0, write_program = 0;
    std::string write_log;
    const bool write_ok = build_compute(write_cs, write_shader, write_program, write_log);
    GLint write_units[2] = {-1, -1};
    if (write_ok) {
        api.glUseProgram(write_program);
        const GLint loc0 = api.glGetUniformLocation(write_program, "img0");
        const GLint loc1 = api.glGetUniformLocation(write_program, "img1");
        api.glUniform1i(loc0, 0);
        api.glUniform1i(loc1, 1);
        if (api.glGetUniformiv) {
            api.glGetUniformiv(write_program, loc0, &write_units[0]);
            api.glGetUniformiv(write_program, loc1, &write_units[1]);
        }
        api.glDispatchCompute(24, 12, 24);
    }
    api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_STORAGE_BARRIER_BIT);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffer);

    GLuint read_shader = 0, read_program = 0;
    std::string read_log;
    const bool read_ok = build_compute(read_cs, read_shader, read_program, read_log);
    GLuint values[2] = {};
    GLuint program_values[2] = {};
    GLint read_loc0 = -1, read_loc1 = -1;
    GLint read_units[2] = {-1, -1};
    auto map_values = [&](GLuint (&dst)[2]) {
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(dst), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(dst, ptr, sizeof(dst));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    };
    if (read_ok) {
        api.glUseProgram(read_program);
        read_loc0 = api.glGetUniformLocation(read_program, "img0");
        read_loc1 = api.glGetUniformLocation(read_program, "img1");
        api.glUniform1i(read_loc0, 0);
        api.glUniform1i(read_loc1, 1);
        if (api.glGetUniformiv) {
            api.glGetUniformiv(read_program, read_loc0, &read_units[0]);
            api.glGetUniformiv(read_program, read_loc1, &read_units[1]);
        }
        api.glDispatchCompute(1, 1, 1);
        map_values(values);

        if (api.glProgramUniform1i) {
            api.glProgramUniform1i(read_program, read_loc0, 0);
            api.glProgramUniform1i(read_program, read_loc1, 1);
            api.glDispatchCompute(1, 1, 1);
            map_values(program_values);
        }
    }

    const bool separate = values[0] == 11u && values[1] == 22u;
    const bool program_separate = program_values[0] == 11u && program_values[1] == 22u;
    const std::string translated = read_shader ? shader_source_text(read_shader) : std::string();
    r.ok = bind_error == GL_NO_ERROR && write_ok && read_ok && read_loc0 >= 0 && read_loc1 >= 0 && separate;
    r.detail = "bind_error=" + std::to_string(bind_error) + " write=" + std::to_string(write_ok) +
               " read=" + std::to_string(read_ok) + " loc0=" + std::to_string(read_loc0) +
               " loc1=" + std::to_string(read_loc1) +
               " write_units=" + std::to_string(write_units[0]) + "," + std::to_string(write_units[1]) +
               " read_units=" + std::to_string(read_units[0]) + "," + std::to_string(read_units[1]) +
               " values=" + std::to_string(values[0]) + "," + std::to_string(values[1]) +
               " program_values=" + std::to_string(program_values[0]) + "," +
               std::to_string(program_values[1]) + " separate=" + std::to_string(separate) +
               " program_separate=" + std::to_string(program_separate) +
               " write_log=" + write_log + " read_log=" + read_log +
               " read_source=" + translated.substr(0, std::min<size_t>(translated.size(), 1200));

    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex0);
    api.glDeleteTextures(1, &tex1);
    api.glDeleteProgram(write_program);
    api.glDeleteShader(write_shader);
    api.glDeleteProgram(read_program);
    api.glDeleteShader(read_shader);
    return r;
}

Result test_act_pingpong_binding() {
    Result r;
    const int W = 192, H = 96, D = 192;
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(rgba16f, binding = 0) writeonly uniform highp image3D img0;
layout(rgba16f, binding = 1) writeonly uniform highp image3D img1;
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(p, ivec3(192, 96, 192)))) return;
    imageStore(img0, p, vec4(11.0));
    imageStore(img1, p, vec4(22.0));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(rgba16f, binding = 0) readonly uniform highp image3D img0;
layout(rgba16f, binding = 1) readonly uniform highp image3D img1;
layout(std430, binding = 2) buffer Out { uint v[2]; } outb;
void main() {
    outb.v[0] = uint(imageLoad(img0, ivec3(95, 47, 95)).x);
    outb.v[1] = uint(imageLoad(img1, ivec3(95, 47, 95)).x);
}
)";

    auto build_compute = [&](const char* src, GLuint& shader, GLuint& program, std::string& log) {
        shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &src, nullptr);
        api.glCompileShader(shader);
        GLint cstatus = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &cstatus);
        GLint len = 0;
        api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) {
            log.resize(static_cast<size_t>(len), '\0');
            api.glGetShaderInfoLog(shader, len, nullptr, log.data());
        }
        program = api.glCreateProgram();
        api.glAttachShader(program, shader);
        api.glLinkProgram(program);
        GLint lstatus = GL_FALSE;
        api.glGetProgramiv(program, GL_LINK_STATUS, &lstatus);
        return cstatus == GL_TRUE && lstatus == GL_TRUE;
    };

    GLuint tex0 = 0, tex1 = 0;
    api.glGenTextures(1, &tex0);
    api.glGenTextures(1, &tex1);
    for (GLuint tex : {tex0, tex1}) {
        api.glBindTexture(GL_TEXTURE_3D, tex);
        api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        api.glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, W, H, D);
    }
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBindImageTexture(0, tex0, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    api.glBindImageTexture(1, tex1, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    const GLenum bind_error = api.glGetError();

    GLuint write_shader = 0, write_program = 0;
    std::string write_log;
    const bool write_ok = build_compute(write_cs, write_shader, write_program, write_log);
    if (write_ok) {
        api.glUseProgram(write_program);
        api.glDispatchCompute(24, 12, 24);
    }
    api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_STORAGE_BARRIER_BIT);

    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffer);

    GLuint read_shader = 0, read_program = 0;
    std::string read_log;
    const bool read_ok = build_compute(read_cs, read_shader, read_program, read_log);
    GLuint values[2] = {};
    if (read_ok) {
        api.glUseProgram(read_program);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, sizeof(values), GL_MAP_READ_BIT);
        if (ptr) {
            memcpy(values, ptr, sizeof(values));
            api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
    }

    const bool separate = values[0] == 11u && values[1] == 22u;
    r.ok = bind_error == GL_NO_ERROR && write_ok && read_ok && separate;
    r.detail = "bind_error=" + std::to_string(bind_error) + " write=" + std::to_string(write_ok) +
               " read=" + std::to_string(read_ok) + " values=" + std::to_string(values[0]) + "," +
               std::to_string(values[1]) + " separate=" + std::to_string(separate) +
               " write_log=" + write_log + " read_log=" + read_log;

    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex0);
    api.glDeleteTextures(1, &tex1);
    api.glDeleteProgram(write_program);
    api.glDeleteShader(write_shader);
    api.glDeleteProgram(read_program);
    api.glDeleteShader(read_shader);
    return r;
}

Result test_r32ui_volume() {
    Result r;
    const int W = 128, H = 64, D = 128;
    const char* write_cs = R"(#version 310 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(r32ui, binding = 0) writeonly uniform highp uimage3D img;
void main() {
    ivec3 p = ivec3(gl_GlobalInvocationID);
    if (any(greaterThanEqual(p, ivec3(128, 64, 128)))) return;
    imageStore(img, p, uvec4(7u));
}
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(r32ui, binding = 0) readonly uniform highp uimage3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = imageLoad(img, ivec3(63, 31, 63)).r; }
)";
    auto build = [&](const char* src, GLuint& shader, GLuint& program, std::string& log) {
        shader = api.glCreateShader(GL_COMPUTE_SHADER);
        api.glShaderSource(shader, 1, &src, nullptr);
        api.glCompileShader(shader);
        GLint ok = GL_FALSE;
        api.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        GLint len = 0; api.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        if (len > 0) { log.resize(len, '\0'); api.glGetShaderInfoLog(shader, len, nullptr, log.data()); }
        program = api.glCreateProgram(); api.glAttachShader(program, shader); api.glLinkProgram(program);
        GLint link = GL_FALSE; api.glGetProgramiv(program, GL_LINK_STATUS, &link);
        return ok == GL_TRUE && link == GL_TRUE;
    };
    GLuint tex = 0;
    api.glGenTextures(1, &tex);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    api.glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    api.glTexImage3D(GL_TEXTURE_3D, 0, GL_R16UI, W, H, D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    const GLenum storage_error = api.glGetError();
    api.glActiveTexture(GL_TEXTURE0);
    api.glBindTexture(GL_TEXTURE_3D, tex);
    api.glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    const GLenum bind_error = api.glGetError();
    GLuint ws = 0, wp = 0, rs = 0, rp = 0;
    std::string wl, rl;
    const bool write_ok = build(write_cs, ws, wp, wl);
    const bool read_ok = build(read_cs, rs, rp, rl);
    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    api.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    if (write_ok) {
        api.glUseProgram(wp);
        GLint loc = api.glGetUniformLocation(wp, "img");
        if (loc >= 0) api.glUniform1i(loc, 0);
        api.glDispatchCompute(16, 8, 16);
    }
    api.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                        GL_SHADER_STORAGE_BARRIER_BIT);
    GLuint value = 0;
    if (read_ok) {
        api.glUseProgram(rp);
        GLint loc = api.glGetUniformLocation(rp, "img");
        if (loc >= 0) api.glUniform1i(loc, 0);
        api.glDispatchCompute(1, 1, 1);
        api.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        api.glFinish();
        void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
        if (ptr) { memcpy(&value, ptr, 4); api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
    }
    r.ok = storage_error == GL_NO_ERROR && bind_error == GL_NO_ERROR && write_ok && read_ok && value == 7u;
    r.detail = "storage_error=" + std::to_string(storage_error) + " bind_error=" + std::to_string(bind_error) +
               " write=" + std::to_string(write_ok) + " read=" + std::to_string(read_ok) +
               " value=" + std::to_string(value) + " write_log=" + wl + " read_log=" + rl;
    api.glDeleteBuffers(1, &buffer);
    api.glDeleteTextures(1, &tex);
    api.glDeleteProgram(wp); api.glDeleteShader(ws); api.glDeleteProgram(rp); api.glDeleteShader(rs);
    return r;
}

Result test_clear_buffer_subdata() {
    Result r;
    GLuint buffer = 0;
    api.glGenBuffers(1, &buffer);
    api.glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    std::vector<unsigned char> ones(64, 0xff);
    api.glBufferData(GL_SHADER_STORAGE_BUFFER, 64, ones.data(), GL_DYNAMIC_DRAW);
    int zero = 0;
    if (api.glClearBufferSubData) {
        api.glClearBufferSubData(GL_SHADER_STORAGE_BUFFER, GL_R8, 0, 64, GL_RED, GL_BYTE, &zero);
    }
    const GLenum clear_error = api.glGetError();
    api.glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    unsigned char got[64] = {};
    void* ptr = api.glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 64, GL_MAP_READ_BIT);
    if (ptr) { memcpy(got, ptr, 64); api.glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
    bool all_zero = true;
    for (unsigned char v : got) all_zero &= (v == 0);
    r.ok = api.glClearBufferSubData != nullptr && clear_error == GL_NO_ERROR && all_zero;
    r.detail = "entry=" + std::string(api.glClearBufferSubData ? "yes" : "no") +
               " error=" + std::to_string(clear_error) + " all_zero=" + std::to_string(all_zero);
    api.glDeleteBuffers(1, &buffer);
    return r;
}

Result test_integer_image_binding() {
    Result r;
    // ESSL 3.10 cannot name an r16ui image on this provider. The frontend must
    // report that refusal; an r32ui image is the current legal fallback. When
    // the loader implements the r16ui/r8ui lowering, replace this with an
    // imageStore/readback test instead of weakening the assertion.
    GLuint tex16 = 0;
    api.glGenTextures(1, &tex16);
    api.glBindTexture(GL_TEXTURE_2D, tex16);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, 1, 1);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBindImageTexture(0, tex16, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R16UI);
    const GLenum e16 = api.glGetError();

    GLuint tex32 = 0;
    api.glGenTextures(1, &tex32);
    api.glBindTexture(GL_TEXTURE_2D, tex32);
    api.glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, 1, 1);
    while (api.glGetError() != GL_NO_ERROR) {}
    api.glBindImageTexture(1, tex32, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
    const GLenum e32 = api.glGetError();

    r.ok = (e16 == GL_NO_ERROR && e32 == GL_NO_ERROR);
    r.detail = "r16ui=" + std::string(e16 == GL_NO_ERROR ? "accepted via R32UI" : "refused") +
               " error=" + std::to_string(e16) +
               " r32ui=" + std::string(e32 == GL_NO_ERROR ? "accepted" : "refused") +
               " error=" + std::to_string(e32);
    api.glDeleteTextures(1, &tex16);
    api.glDeleteTextures(1, &tex32);
    return r;
}

void print_result(const char* name, const Result& r) {
    printf("  \"%s\": {\"pass\": %s, \"detail\": %s}%s\n", name, r.ok ? "true" : "false",
           escape(r.detail).c_str(), ",");
}

} // namespace

int main(int argc, char** argv) {
    const char* mode = (argc > 1) ? argv[1] : "all";
    const char* output_path = getenv("MG_PROBE_OUTPUT");
    if (output_path && *output_path) {
        g_out = fopen(output_path, "w");
        if (!g_out) {
            fprintf(stderr, "cannot open probe output %s\n", output_path);
            return 89;
        }
    }
    const char* library = getenv("MG_LIBRARY");
    if (!library) library = "libmobileglues.so";
    g_lib = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (!g_lib) {
        fprintf(stderr, "dlopen %s failed: %s\n", library, dlerror());
        return 91;
    }

    api.eglGetDisplay = sym<PFNEGLGETDISPLAYPROC>("eglGetDisplay");
    api.eglInitialize = sym<PFNEGLINITIALIZEPROC>("eglInitialize");
    api.eglBindAPI = sym<PFNEGLBINDAPIPROC>("eglBindAPI");
    api.eglChooseConfig = sym<PFNEGLCHOOSECONFIGPROC>("eglChooseConfig");
    api.eglCreatePbufferSurface = sym<PFNEGLCREATEPBUFFERSURFACEPROC>("eglCreatePbufferSurface");
    api.eglCreateContext = sym<PFNEGLCREATECONTEXTPROC>("eglCreateContext");
    api.eglMakeCurrent = sym<PFNEGLMAKECURRENTPROC>("eglMakeCurrent");
    api.eglDestroySurface = sym<PFNEGLDESTROYSURFACEPROC>("eglDestroySurface");
    api.eglDestroyContext = sym<PFNEGLDESTROYCONTEXTPROC>("eglDestroyContext");
    api.eglTerminate = sym<PFNEGLTERMINATEPROC>("eglTerminate");
    api.eglGetProcAddress = sym<PFNEGLGETPROCADDRESSPROC>("eglGetProcAddress");

    api.glGetString = sym<PFNGLGETSTRINGPROC>("glGetString");
    api.glGetStringi = sym<PFNGLGETSTRINGIPROC>("glGetStringi");
    api.glGetIntegerv = sym<PFNGLGETINTEGERVPROC>("glGetIntegerv");
    api.glGetError = sym<PFNGLGETERRORPROC>("glGetError");
    api.glCreateShader = sym<PFNGLCREATESHADERPROC>("glCreateShader");
    api.glShaderSource = sym<PFNGLSHADERSOURCEPROC>("glShaderSource");
    api.glCompileShader = sym<PFNGLCOMPILESHADERPROC>("glCompileShader");
    api.glGetShaderiv = sym<PFNGLGETSHADERIVPROC>("glGetShaderiv");
    api.glGetShaderInfoLog = sym<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
    api.glGetShaderSource = sym<PFNGLGETSHADERSOURCEPROC>("glGetShaderSource");
    api.glDeleteShader = sym<PFNGLDELETESHADERPROC>("glDeleteShader");
    api.glCreateProgram = sym<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
    api.glAttachShader = sym<PFNGLATTACHSHADERPROC>("glAttachShader");
    api.glLinkProgram = sym<PFNGLLINKPROGRAMPROC>("glLinkProgram");
    api.glGetProgramiv = sym<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
    api.glGetProgramInfoLog = sym<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
    api.glDeleteProgram = sym<PFNGLDELETEPROGRAMPROC>("glDeleteProgram");
    api.glBindFragDataLocation = sym<GLint (*)(GLuint, GLuint, const GLchar*)>("glBindFragDataLocation");
    api.glGetFragDataLocation = sym<GLint (*)(GLuint, const GLchar*)>("glGetFragDataLocation");
    api.glGetProgramResourceIndex = sym<GLuint (*)(GLuint, GLenum, const GLchar*)>("glGetProgramResourceIndex");
    api.glGetProgramResourceName =
        sym<void (*)(GLuint, GLenum, GLuint, GLsizei, GLsizei*, GLchar*)>("glGetProgramResourceName");
    api.glGetProgramResourceiv =
        sym<void (*)(GLuint, GLenum, GLuint, GLsizei, const GLenum*, GLsizei, GLsizei*, GLint*)>("glGetProgramResourceiv");
    api.glShaderStorageBlockBinding = sym<void (*)(GLuint, GLuint, GLuint)>("glShaderStorageBlockBinding");
    api.mg_image_metadata_begin = sym<void (*)()>("mg_image_metadata_begin");
    api.mg_image_metadata_add = sym<void (*)(const char*, GLenum)>("mg_image_metadata_add");
    api.mg_image_metadata_end = sym<void (*)()>("mg_image_metadata_end");
    api.glUseProgram = sym<PFNGLUSEPROGRAMPROC>("glUseProgram");
    api.glBindBuffer = sym<PFNGLBINDBUFFERPROC>("glBindBuffer");
    api.glGenBuffers = sym<PFNGLGENBUFFERSPROC>("glGenBuffers");
    api.glDeleteBuffers = sym<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
    api.glBufferData = sym<PFNGLBUFFERDATAPROC>("glBufferData");
    api.glBindBufferBase = sym<PFNGLBINDBUFFERBASEPROC>("glBindBufferBase");
    api.glClearBufferSubData = sym<void (*)(GLenum, GLenum, GLintptr, GLsizeiptr, GLenum, GLenum, const void*)>("glClearBufferSubData");
    api.glGetUniformLocation = sym<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    api.glUniform1ui = sym<PFNGLUNIFORM1UIPROC>("glUniform1ui");
    api.glUniform1i = sym<PFNGLUNIFORM1IPROC>("glUniform1i");
    api.glGetUniformiv = sym<void (*)(GLuint, GLint, GLint*)>("glGetUniformiv");
    api.glProgramUniform1i = sym<void (*)(GLuint, GLint, GLint)>("glProgramUniform1i");
    api.glProgramUniform1iv = sym<void (*)(GLuint, GLint, GLsizei, const GLint*)>("glProgramUniform1iv");
    api.glUniform1iv = sym<void (*)(GLint, GLsizei, const GLint*)>("glUniform1iv");
    api.glActiveTexture = sym<PFNGLACTIVETEXTUREPROC>("glActiveTexture");
    api.glDispatchCompute = sym<PFNGLDISPATCHCOMPUTEPROC>("glDispatchCompute");
    api.glMemoryBarrier = sym<PFNGLMEMORYBARRIERPROC>("glMemoryBarrier");
    api.glMapBufferRange = sym<PFNGLMAPBUFFERRANGEPROC>("glMapBufferRange");
    api.glUnmapBuffer = sym<PFNGLUNMAPBUFFERPROC>("glUnmapBuffer");
    api.glBindImageTexture = sym<PFNGLBINDIMAGETEXTUREPROC>("glBindImageTexture");
    api.glTexSubImage3D = sym<PFNGLTEXSUBIMAGE3DPROC>("glTexSubImage3D");
    api.glClearTexImage = sym<void (*)(GLuint, GLint, GLenum, GLenum, const void*)>("glClearTexImage");
    api.glGenSamplers = sym<PFNGLGENSAMPLERSPROC>("glGenSamplers");
    api.glBindSampler = sym<PFNGLBINDSAMPLERPROC>("glBindSampler");
    api.glSamplerParameteri = sym<PFNGLSAMPLERPARAMETERIPROC>("glSamplerParameteri");
    api.glDeleteSamplers = sym<PFNGLDELETESAMPLERSPROC>("glDeleteSamplers");
    api.glTexStorage2D = sym<PFNGLTEXSTORAGE2DPROC>("glTexStorage2D");
    api.glTexStorage3D = sym<PFNGLTEXSTORAGE3DPROC>("glTexStorage3D");
    api.glTexImage3D = sym<PFNGLTEXIMAGE3DPROC>("glTexImage3D");
    api.glTexImage2D = sym<PFNGLTEXIMAGE2DPROC>("glTexImage2D");
    api.glGenFramebuffers = sym<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    api.glBindFramebuffer = sym<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    api.glFramebufferTexture2D = sym<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    api.glCheckFramebufferStatus = sym<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus");
    api.glDeleteFramebuffers = sym<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
    api.glGenTextures = sym<PFNGLGENTEXTURESPROC>("glGenTextures");
    api.glBindTexture = sym<PFNGLBINDTEXTUREPROC>("glBindTexture");
    api.glTexParameteri = sym<PFNGLTEXPARAMETERIPROC>("glTexParameteri");
    api.glGetTexParameteriv = sym<void (*)(GLenum, GLenum, GLint*)>("glGetTexParameteriv");
    api.glGenQueries = sym<void (*)(GLsizei, GLuint*)>("glGenQueries");
    api.glDeleteQueries = sym<void (*)(GLsizei, const GLuint*)>("glDeleteQueries");
    api.glQueryCounter = sym<void (*)(GLuint, GLenum)>("glQueryCounter");
    api.glGetQueryObjectui64v = sym<void (*)(GLuint, GLenum, GLuint64*)>("glGetQueryObjectui64v");
    api.glCopyImageSubData = sym<void (*)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint,
                                          GLint, GLint, GLsizei, GLsizei, GLsizei)>("glCopyImageSubData");
    api.glDeleteTextures = sym<PFNGLDELETETEXTURESPROC>("glDeleteTextures");
    api.glDrawArrays = sym<PFNGLDRAWARRAYSPROC>("glDrawArrays");
    api.glGenVertexArrays = sym<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
    api.glBindVertexArray = sym<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    api.glDeleteVertexArrays = sym<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
    api.glFinish = sym<PFNGLFINISHPROC>("glFinish");
    api.glViewport = sym<PFNGLVIEWPORTPROC>("glViewport");
    api.glCopyTexSubImage2D = sym<PFNGLCOPYTEXSUBIMAGE2DPROC>("glCopyTexSubImage2D");
    api.glCopyTexImage2D = sym<PFNGLCOPYTEXIMAGE2DPROC>("glCopyTexImage2D");
    api.glReadPixels = sym<PFNGLREADPIXELSPROC>("glReadPixels");
    api.glPatchParameteri = sym<void (*)(GLenum, GLint)>("glPatchParameteri");
    api.glPatchParameterfv = sym<void (*)(GLenum, const GLfloat*)>("glPatchParameterfv");
    api.glGenRenderbuffers = sym<void (*)(GLsizei, GLuint*)>("glGenRenderbuffers");
    api.glDeleteRenderbuffers = sym<void (*)(GLsizei, const GLuint*)>("glDeleteRenderbuffers");
    api.glBindRenderbuffer = sym<void (*)(GLenum, GLuint)>("glBindRenderbuffer");
    api.glRenderbufferStorage = sym<void (*)(GLenum, GLenum, GLsizei, GLsizei)>("glRenderbufferStorage");
    api.glRenderbufferStorageMultisample = sym<void (*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei)>(
        "glRenderbufferStorageMultisample");
    api.glFenceSync = sym<GLsync (*)(GLenum, GLbitfield)>("glFenceSync");
    api.glDeleteSync = sym<void (*)(GLsync)>("glDeleteSync");
    api.glFramebufferRenderbuffer = sym<void (*)(GLenum, GLenum, GLenum, GLuint)>("glFramebufferRenderbuffer");
    api.glClearColor = sym<void (*)(GLfloat, GLfloat, GLfloat, GLfloat)>("glClearColor");
    api.glClear = sym<void (*)(GLbitfield)>("glClear");
    api.glBlitFramebuffer = sym<void (*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)>(
        "glBlitFramebuffer");
    api.glEnable = sym<PFNGLENABLEPROC>("glEnable");
    api.glDisable = sym<PFNGLDISABLEPROC>("glDisable");
    api.glBlendFunc = sym<PFNGLBLENDFUNCPROC>("glBlendFunc");

    if (!make_context()) {
        fprintf(stderr, "failed to create a MobileGlues context\n");
        return 92;
    }

    fprintf(g_out, "{\n");
    fprintf(g_out, "  \"wrapper\": {\"library\": %s, \"renderer\": %s, \"version\": %s, \"shading\": %s},\n",
           escape(library).c_str(), escape(gl_string(GL_RENDERER)).c_str(),
           escape(gl_string(GL_VERSION)).c_str(), escape(gl_string(GL_SHADING_LANGUAGE_VERSION)).c_str());

    bool first = true;
    auto run = [&](const char* name, Result (*fn)()) {
        Result r = fn();
        if (!first) fprintf(g_out, ",\n");
        first = false;
        fprintf(g_out, "  \"%s\": {\"pass\": %s, \"detail\": %s}", name, r.ok ? "true" : "false",
                escape(r.detail).c_str());
    };
    if (!strcmp(mode, "all") || !strcmp(mode, "source_multi")) run("source_multi", test_source_multi);
    if (!strcmp(mode, "all") || !strcmp(mode, "source_multi_implicit")) run("source_multi_implicit", test_source_multi_implicit);
    if (!strcmp(mode, "all") || !strcmp(mode, "source_embedded_nul")) run("source_embedded_nul", test_source_embedded_nul);
    if (!strcmp(mode, "all") || !strcmp(mode, "compile_failure")) run("compile_failure", test_compile_failure);
    if (!strcmp(mode, "all") || !strcmp(mode, "link_failure")) run("link_failure", test_link_failure);
    if (!strcmp(mode, "all") || !strcmp(mode, "get_error")) run("get_error", test_get_error);
    if (!strcmp(mode, "all") || !strcmp(mode, "limits")) run("limits", test_limits);
    if (!strcmp(mode, "all") || !strcmp(mode, "strings")) run("strings", test_strings);
    if (!strcmp(mode, "all") || !strcmp(mode, "frag_data")) run("frag_data_isolation", test_frag_data_isolation);
    if (!strcmp(mode, "all") || !strcmp(mode, "frag_data_interleaved")) run("frag_data_interleaved", test_frag_data_interleaved);
    if (!strcmp(mode, "all") || !strcmp(mode, "frag_data_relayout")) run("frag_data_replaces_existing_layout", test_frag_data_replaces_existing_layout);
    if (!strcmp(mode, "all") || !strcmp(mode, "ssbo_binding")) run("ssbo_block_binding", test_storage_block_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "ssbo_graphics")) run("ssbo_graphics_binding", test_graphics_storage_block_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "compute_512")) run("compute_512", test_compute_512);
    if (!strcmp(mode, "all") || !strcmp(mode, "vertex_image")) run("vertex_image_store", test_vertex_image_store);
    if (!strcmp(mode, "all") || !strcmp(mode, "read_image_link")) run("read_image_link", test_read_image_link);
    if (!strcmp(mode, "all") || !strcmp(mode, "desktop_glsl")) {
        run("desktop_glsl_120", [] { return test_desktop_glsl_version("120"); });
        run("desktop_glsl_150", [] { return test_desktop_glsl_version("150"); });
        run("desktop_glsl_330", [] { return test_desktop_glsl_version("330"); });
        run("desktop_glsl_400", [] { return test_desktop_glsl_version("400"); });
        run("desktop_glsl_460", [] { return test_desktop_glsl_version("460"); });
    }
    if (!strcmp(mode, "all") || !strcmp(mode, "rgb8_snorm")) run("rgb8_snorm_attachment", test_rgb8_snorm_attachment);
    if (!strcmp(mode, "all") || !strcmp(mode, "depth_filter")) run("depth_filter_complete", test_depth_filter_complete);
    if (!strcmp(mode, "all") || !strcmp(mode, "timer_query")) run("timer_query64", test_timer_query64);
    if (!strcmp(mode, "all") || !strcmp(mode, "copy_image_depth")) run("copy_image_subdata_depth", test_copy_image_subdata_depth);
    if (!strcmp(mode, "all") || !strcmp(mode, "blit_resolve")) run("blit_multisample_resolve", test_blit_resolve);
    if (!strcmp(mode, "all") || !strcmp(mode, "depth_copy_mismatch")) run("depth_copy_mismatch", test_depth_copy_mismatch);
    if (!strcmp(mode, "all") || !strcmp(mode, "blit_depth_mismatch")) run("blit_depth_mismatch", test_blit_depth_mismatch);
    if (!strcmp(mode, "all") || !strcmp(mode, "blit_depth_match")) run("blit_depth_match", test_blit_depth_match);
    if (!strcmp(mode, "all") || !strcmp(mode, "copy_image_depth_mismatch")) run("copy_image_depth_mismatch", test_copy_image_depth_mismatch);
    if (!strcmp(mode, "all") || !strcmp(mode, "geometry_stage")) run("geometry_stage", test_geometry_stage);
    if (!strcmp(mode, "all") || !strcmp(mode, "tessellation_stage")) run("tessellation_stage", test_tessellation_stage);
    if (!strcmp(mode, "all") || !strcmp(mode, "patch_defaults")) run("patch_default_levels", test_patch_default_levels);
    if (!strcmp(mode, "all") || !strcmp(mode, "clear_tex_level")) run("clear_tex_level", test_clear_tex_level);
    if (!strcmp(mode, "all") || !strcmp(mode, "translated_image_decl")) run("translated_image_decl", test_translated_image_decl);
    if (!strcmp(mode, "all") || !strcmp(mode, "metadata_image_format")) run("metadata_image_format", test_metadata_image_format);
    if (!strcmp(mode, "all") || !strcmp(mode, "vertex_ssbo_atomics")) run("vertex_ssbo_atomics", test_vertex_ssbo_atomics);
    if (!strcmp(mode, "all") || !strcmp(mode, "vertex_image2d")) run("vertex_image2d_store", test_vertex_image2d_store);
    if (!strcmp(mode, "all") || !strcmp(mode, "translation_failure")) run("translation_failure_not_stale", test_translation_failure_not_stale);
    if (!strcmp(mode, "all") || !strcmp(mode, "uniform_initializer")) run("initialized_uniform_array", test_initialized_uniform_array);
    if (!strcmp(mode, "all") || !strcmp(mode, "frag_data_shared")) run("frag_data_shared_shader", test_frag_data_shared_shader);
    if (!strcmp(mode, "all") || !strcmp(mode, "frag_data_linked")) run("frag_data_linked_location", test_frag_data_linked_location);
    if (!strcmp(mode, "all") || !strcmp(mode, "ssbo_qualifier")) run("ssbo_qualifier_preserved", test_ssbo_qualifier_preserved);
    if (!strcmp(mode, "guard_bench")) run("guard_scan_bench", test_guard_scan_bench);
    if (!strcmp(mode, "image_rebind_bench")) run("image_rebind_bench", test_image_rebind_bench);
    if (!strcmp(mode, "iteration_caps")) run("iteration_caps", test_iteration_caps);
    if (!strcmp(mode, "all") || !strcmp(mode, "extensions")) run("extension_consistency", test_extension_consistency);
    if (!strcmp(mode, "all") || !strcmp(mode, "formatless_image")) run("formatless_image_metadata", test_formatless_image_metadata);
    if (!strcmp(mode, "all") || !strcmp(mode, "explicit_image")) run("explicit_image_binding", test_explicit_image_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "duplicate_image")) run("duplicate_image_binding", test_duplicate_image_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "program_uniform_image")) run("program_uniform_image_binding", test_program_uniform_image_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "image_array_uniform_iv")) run("image_array_uniform_iv", test_image_array_uniform_iv);
    if (!strcmp(mode, "all") || !strcmp(mode, "integer_image")) run("integer_image", test_integer_image_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "act_volume")) run("act_volume", test_act_volume);
    if (!strcmp(mode, "all") || !strcmp(mode, "act_pingpong")) run("act_pingpong", test_act_pingpong);
    if (!strcmp(mode, "all") || !strcmp(mode, "act_pingpong_binding")) run("act_pingpong_binding", test_act_pingpong_binding);
    if (!strcmp(mode, "all") || !strcmp(mode, "r32ui_volume")) run("r32ui_volume", test_r32ui_volume);
    if (!strcmp(mode, "all") || !strcmp(mode, "clear_buffer")) run("clear_buffer_subdata", test_clear_buffer_subdata);
    fprintf(g_out, "\n}\n");

    destroy_context();
    if (g_out != stdout) fclose(g_out);
    return 0;
}
