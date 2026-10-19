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
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLUNIFORM1UIPROC glUniform1ui;
    PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
    PFNGLMEMORYBARRIERPROC glMemoryBarrier;
    PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
    PFNGLUNMAPBUFFERPROC glUnmapBuffer;
    PFNGLBINDIMAGETEXTUREPROC glBindImageTexture;
    PFNGLTEXSTORAGE2DPROC glTexStorage2D;
    PFNGLTEXSTORAGE3DPROC glTexStorage3D;
    PFNGLTEXIMAGE2DPROC glTexImage2D;
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
    PFNGLGENTEXTURESPROC glGenTextures;
    PFNGLBINDTEXTUREPROC glBindTexture;
    PFNGLTEXPARAMETERIPROC glTexParameteri;
    PFNGLDELETETEXTURESPROC glDeleteTextures;
    PFNGLDRAWARRAYSPROC glDrawArrays;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLFINISHPROC glFinish;
    PFNGLVIEWPORTPROC glViewport;
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
    // The backend limit is in the below-wrapper probe; this harness records what
    // the application is actually told. MG_EXPECT_MAX_TEXTURE_IMAGE_UNITS lets a
    // runner pin the provider's value without hardcoding one device in the test.
    const char* expected_env = getenv("MG_EXPECT_MAX_TEXTURE_IMAGE_UNITS");
    const int expected = expected_env ? atoi(expected_env) : -1;
    r.ok = (expected < 0) ? true : (frontend == expected);
    r.detail = "frontend_max_texture_image_units=" + std::to_string(frontend) +
               " backend_access=" + (backend_access ? "yes" : "no") +
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
    api.glGetUniformLocation = sym<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
    api.glUniform1ui = sym<PFNGLUNIFORM1UIPROC>("glUniform1ui");
    api.glDispatchCompute = sym<PFNGLDISPATCHCOMPUTEPROC>("glDispatchCompute");
    api.glMemoryBarrier = sym<PFNGLMEMORYBARRIERPROC>("glMemoryBarrier");
    api.glMapBufferRange = sym<PFNGLMAPBUFFERRANGEPROC>("glMapBufferRange");
    api.glUnmapBuffer = sym<PFNGLUNMAPBUFFERPROC>("glUnmapBuffer");
    api.glBindImageTexture = sym<PFNGLBINDIMAGETEXTUREPROC>("glBindImageTexture");
    api.glTexStorage2D = sym<PFNGLTEXSTORAGE2DPROC>("glTexStorage2D");
    api.glTexStorage3D = sym<PFNGLTEXSTORAGE3DPROC>("glTexStorage3D");
    api.glTexImage2D = sym<PFNGLTEXIMAGE2DPROC>("glTexImage2D");
    api.glGenFramebuffers = sym<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
    api.glBindFramebuffer = sym<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
    api.glFramebufferTexture2D = sym<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
    api.glCheckFramebufferStatus = sym<PFNGLCHECKFRAMEBUFFERSTATUSPROC>("glCheckFramebufferStatus");
    api.glDeleteFramebuffers = sym<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
    api.glGenTextures = sym<PFNGLGENTEXTURESPROC>("glGenTextures");
    api.glBindTexture = sym<PFNGLBINDTEXTUREPROC>("glBindTexture");
    api.glTexParameteri = sym<PFNGLTEXPARAMETERIPROC>("glTexParameteri");
    api.glDeleteTextures = sym<PFNGLDELETETEXTURESPROC>("glDeleteTextures");
    api.glDrawArrays = sym<PFNGLDRAWARRAYSPROC>("glDrawArrays");
    api.glGenVertexArrays = sym<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
    api.glBindVertexArray = sym<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
    api.glDeleteVertexArrays = sym<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
    api.glFinish = sym<PFNGLFINISHPROC>("glFinish");
    api.glViewport = sym<PFNGLVIEWPORTPROC>("glViewport");
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
    if (!strcmp(mode, "all") || !strcmp(mode, "extensions")) run("extension_consistency", test_extension_consistency);
    if (!strcmp(mode, "all") || !strcmp(mode, "formatless_image")) run("formatless_image_metadata", test_formatless_image_metadata);
    if (!strcmp(mode, "all") || !strcmp(mode, "integer_image")) run("integer_image", test_integer_image_binding);
    fprintf(g_out, "\n}\n");

    destroy_context();
    if (g_out != stdout) fclose(g_out);
    return 0;
}
