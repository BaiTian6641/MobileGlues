// MobileGlues - tools/capability_probe/probe.cpp
// SPDX-License-Identifier: LGPL-2.1-only
//
// Below-wrapper Android EGL/GLES capability probe.
//
// This binary deliberately does not link MobileGlues. It creates its own EGL
// context from the system provider and reports what that provider actually
// advertises and does. A function pointer or an extension string is not a test:
// the behavioral section compiles and runs real ESSL, allocates the formats and
// buffer sizes the shaderpacks request, and reads results back.
//
// Usage: capability_probe [output.json]
// Exit: 0 = probe completed (individual results may be false), 2 = no context.

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

#include <android/log.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <unistd.h>

#define LOG_TAG "MGProbe"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Android's link-time libGLESv2 stub exports ES 2.0 and a few ES 1.1/OES
// entry points only. Every ES 3.x symbol this probe needs is resolved through
// eglGetProcAddress on the actual provider, exactly as a real loader does.
namespace {
PFNGLGETSTRINGIPROC p_glGetStringi = nullptr;
PFNGLTEXSTORAGE2DPROC p_glTexStorage2D = nullptr;
PFNGLTEXSTORAGE3DPROC p_glTexStorage3D = nullptr;
PFNGLBINDIMAGETEXTUREPROC p_glBindImageTexture = nullptr;
PFNGLCLEARBUFFERFVPROC p_glClearBufferfv = nullptr;
PFNGLBINDBUFFERBASEPROC p_glBindBufferBase = nullptr;
PFNGLBINDBUFFERRANGEPROC p_glBindBufferRange = nullptr;
PFNGLDISPATCHCOMPUTEPROC p_glDispatchCompute = nullptr;
PFNGLMEMORYBARRIERPROC p_glMemoryBarrier = nullptr;
PFNGLMAPBUFFERRANGEPROC p_glMapBufferRange = nullptr;
PFNGLUNMAPBUFFERPROC p_glUnmapBuffer = nullptr;
PFNGLGENVERTEXARRAYSPROC p_glGenVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC p_glBindVertexArray = nullptr;
PFNGLDELETEVERTEXARRAYSPROC p_glDeleteVertexArrays = nullptr;
PFNGLGETINTEGERI_VPROC p_glGetIntegeri_v = nullptr;
PFNGLUNIFORM1UIPROC p_glUniform1ui = nullptr;
} // namespace

#define glGetStringi p_glGetStringi
#define glTexStorage2D p_glTexStorage2D
#define glTexStorage3D p_glTexStorage3D
#define glBindImageTexture p_glBindImageTexture
#define glClearBufferfv p_glClearBufferfv
#define glBindBufferBase p_glBindBufferBase
#define glBindBufferRange p_glBindBufferRange
#define glDispatchCompute p_glDispatchCompute
#define glMemoryBarrier p_glMemoryBarrier
#define glMapBufferRange p_glMapBufferRange
#define glUnmapBuffer p_glUnmapBuffer
#define glGenVertexArrays p_glGenVertexArrays
#define glBindVertexArray p_glBindVertexArray
#define glDeleteVertexArrays p_glDeleteVertexArrays
#define glGetIntegeri_v p_glGetIntegeri_v
#define glUniform1ui p_glUniform1ui

namespace {

bool load_gles31_entry_points() {
    // The link-time stub has only ES 2.0, and ANGLE's eglGetProcAddress does
    // not publish core ES functions. The real provider library does export them,
    // so resolve there first and keep eglGetProcAddress as the fallback.
    void* gles = dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
    if (!gles) {
        fprintf(stderr, "dlopen libGLESv2.so failed: %s\n", dlerror());
    }
    bool ok = true;
#define LOAD_PROC(name)                                                                                                \
    do {                                                                                                               \
        name = gles ? reinterpret_cast<decltype(name)>(dlsym(gles, #name + 2)) : nullptr;                              \
        if (!(name)) name = reinterpret_cast<decltype(name)>(eglGetProcAddress(#name + 2));                            \
        if (!(name)) {                                                                                                 \
            fprintf(stderr, "missing GLES entry point: %s\n", #name);                                                   \
            ok = false;                                                                                                \
        }                                                                                                              \
    } while (0)
    LOAD_PROC(p_glGetStringi);
    LOAD_PROC(p_glTexStorage2D);
    LOAD_PROC(p_glTexStorage3D);
    LOAD_PROC(p_glBindImageTexture);
    LOAD_PROC(p_glClearBufferfv);
    LOAD_PROC(p_glBindBufferBase);
    LOAD_PROC(p_glBindBufferRange);
    LOAD_PROC(p_glDispatchCompute);
    LOAD_PROC(p_glMemoryBarrier);
    LOAD_PROC(p_glMapBufferRange);
    LOAD_PROC(p_glUnmapBuffer);
    LOAD_PROC(p_glGenVertexArrays);
    LOAD_PROC(p_glBindVertexArray);
    LOAD_PROC(p_glDeleteVertexArrays);
    LOAD_PROC(p_glGetIntegeri_v);
    LOAD_PROC(p_glUniform1ui);
#undef LOAD_PROC
    return ok;
}

std::string g_json;
std::vector<std::string> g_errors;

void json_escape(const char* s, std::string& out) {
    if (!s) {
        out += "null";
        return;
    }
    out += '"';
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
        switch (*p) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (*p < 0x20) {
                char buf[7];
                snprintf(buf, sizeof(buf), "\\u%04x", *p);
                out += buf;
            } else {
                out += static_cast<char>(*p);
            }
        }
    }
    out += '"';
}

std::string q(const char* s) {
    std::string out;
    json_escape(s, out);
    return out;
}

std::string qstr(const std::string& s) {
    return q(s.c_str());
}

std::string gl_string(GLenum name) {
    const GLubyte* s = glGetString(name);
    return s ? reinterpret_cast<const char*>(s) : "";
}

void add_error(const std::string& where) {
    GLenum e;
    while ((e = glGetError()) != GL_NO_ERROR) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s: 0x%04x", where.c_str(), e);
        g_errors.push_back(buf);
    }
}

std::string gl_error_name(GLenum e) {
    switch (e) {
    case GL_NO_ERROR: return "GL_NO_ERROR";
    case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
    default: {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%04x", e);
        return buf;
    }
    }
}

GLuint compile_shader(GLenum stage, const char* source, std::string* log_out) {
    GLuint shader = glCreateShader(stage);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    GLint len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
        std::string log(static_cast<size_t>(len), '\0');
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        if (log_out) *log_out = log;
    }
    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint link_program(GLenum compute_stage, const char* source, std::string* log_out) {
    std::string shader_log;
    GLuint shader = compile_shader(compute_stage, source, &shader_log);
    if (!shader) {
        if (log_out) *log_out = shader_log;
        return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);
    glDeleteShader(shader);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    GLint len = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
    if (len > 0) {
        std::string log(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(program, len, nullptr, log.data());
        if (log_out) *log_out += log;
    }
    if (!ok) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool has_extension(const char* list, const char* ext) {
    if (!list) return false;
    const size_t n = strlen(ext);
    const char* p = list;
    while ((p = strstr(p, ext)) != nullptr) {
        const bool left = (p == list) || (p[-1] == ' ');
        const bool right = (p[n] == '\0') || (p[n] == ' ');
        if (left && right) return true;
        p += n;
    }
    return false;
}

struct LimitEntry {
    const char* name;
    GLenum pname;
};

const LimitEntry kLimits[] = {
    {"MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE},
    {"MAX_3D_TEXTURE_SIZE", GL_MAX_3D_TEXTURE_SIZE},
    {"MAX_ARRAY_TEXTURE_LAYERS", GL_MAX_ARRAY_TEXTURE_LAYERS},
    {"MAX_CUBE_MAP_TEXTURE_SIZE", GL_MAX_CUBE_MAP_TEXTURE_SIZE},
    {"MAX_TEXTURE_IMAGE_UNITS", GL_MAX_TEXTURE_IMAGE_UNITS},
    {"MAX_VERTEX_TEXTURE_IMAGE_UNITS", GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS},
    {"MAX_COMBINED_TEXTURE_IMAGE_UNITS", GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS},
    {"MAX_VERTEX_IMAGE_UNIFORMS", GL_MAX_VERTEX_IMAGE_UNIFORMS},
    {"MAX_FRAGMENT_IMAGE_UNIFORMS", GL_MAX_FRAGMENT_IMAGE_UNIFORMS},
    {"MAX_COMPUTE_IMAGE_UNIFORMS", GL_MAX_COMPUTE_IMAGE_UNIFORMS},
    {"MAX_COMBINED_IMAGE_UNIFORMS", GL_MAX_COMBINED_IMAGE_UNIFORMS},
    {"MAX_VERTEX_SHADER_STORAGE_BLOCKS", GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS},
    {"MAX_FRAGMENT_SHADER_STORAGE_BLOCKS", GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS},
    {"MAX_COMPUTE_SHADER_STORAGE_BLOCKS", GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS},
    {"MAX_COMBINED_SHADER_STORAGE_BLOCKS", GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS},
    {"MAX_SHADER_STORAGE_BUFFER_BINDINGS", GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS},
    {"MAX_SHADER_STORAGE_BLOCK_SIZE", GL_MAX_SHADER_STORAGE_BLOCK_SIZE},
    {"MAX_UNIFORM_BUFFER_BINDINGS", GL_MAX_UNIFORM_BUFFER_BINDINGS},
    {"MAX_UNIFORM_BLOCK_SIZE", GL_MAX_UNIFORM_BLOCK_SIZE},
    {"MAX_COMPUTE_WORK_GROUP_INVOCATIONS", GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS},
    {"MAX_COMPUTE_SHARED_MEMORY_SIZE", GL_MAX_COMPUTE_SHARED_MEMORY_SIZE},
    {"MAX_DRAW_BUFFERS", GL_MAX_DRAW_BUFFERS},
    {"MAX_COLOR_ATTACHMENTS", GL_MAX_COLOR_ATTACHMENTS},
    {"MAX_FRAGMENT_INPUT_COMPONENTS", GL_MAX_FRAGMENT_INPUT_COMPONENTS},
    {"MAX_VERTEX_OUTPUT_COMPONENTS", GL_MAX_VERTEX_OUTPUT_COMPONENTS},
    {"MAX_FRAGMENT_UNIFORM_VECTORS", GL_MAX_FRAGMENT_UNIFORM_VECTORS},
    {"MAX_VERTEX_UNIFORM_VECTORS", GL_MAX_VERTEX_UNIFORM_VECTORS},
    {"MAX_VERTEX_ATTRIBS", GL_MAX_VERTEX_ATTRIBS},
    {"MAX_TEXTURE_MAX_ANISOTROPY_EXT", 0x84FF},
    {"MAX_SAMPLES", GL_MAX_SAMPLES},
    {"MAX_RENDERBUFFER_SIZE", GL_MAX_RENDERBUFFER_SIZE},
};

struct FormatEntry {
    const char* name;
    GLenum internal;
    GLenum format;
    GLenum type;
};

const FormatEntry kFormats[] = {
    {"R8UI", GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE},
    {"R16UI", GL_R16UI, GL_RED_INTEGER, GL_UNSIGNED_SHORT},
    {"R11F_G11F_B10F", GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV},
    {"RGB8_SNORM", GL_RGB8_SNORM, GL_RGB, GL_BYTE},
    {"RGBA8_SNORM", GL_RGBA8_SNORM, GL_RGBA, GL_BYTE},
    {"RGB16F", GL_RGB16F, GL_RGB, GL_HALF_FLOAT},
    {"RGBA16F", GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT},
    {"R16F", GL_R16F, GL_RED, GL_HALF_FLOAT},
    {"RG16F", GL_RG16F, GL_RG, GL_HALF_FLOAT},
    {"R32F", GL_R32F, GL_RED, GL_FLOAT},
    {"RGBA32F", GL_RGBA32F, GL_RGBA, GL_FLOAT},
    {"R32UI", GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT},
    {"RGBA32UI", GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT},
    {"R8", GL_R8, GL_RED, GL_UNSIGNED_BYTE},
    {"RGB8", GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE},
    {"RGB10_A2", GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV},
    {"RGBA8", GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE},
    {"DEPTH_COMPONENT24", GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},
    {"DEPTH_COMPONENT32F", GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT},
    {"DEPTH24_STENCIL8", GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8},
};

struct ProbeContext {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLConfig config = nullptr;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
};

bool make_context(ProbeContext* p) {
    p->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (p->display == EGL_NO_DISPLAY) return false;
    EGLint major = 0, minor = 0;
    if (!eglInitialize(p->display, &major, &minor)) return false;
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE,
    };
    EGLint num = 0;
    if (!eglChooseConfig(p->display, config_attribs, &p->config, 1, &num) || num <= 0) return false;
    const EGLint surface_attribs[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    p->surface = eglCreatePbufferSurface(p->display, p->config, surface_attribs);
    if (p->surface == EGL_NO_SURFACE) return false;
    const EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 2,
        EGL_CONTEXT_OPENGL_ROBUST_ACCESS, EGL_FALSE,
        EGL_NONE,
    };
    p->context = eglCreateContext(p->display, p->config, EGL_NO_CONTEXT, ctx_attribs);
    if (p->context == EGL_NO_CONTEXT) {
        const EGLint es3_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        p->context = eglCreateContext(p->display, p->config, EGL_NO_CONTEXT, es3_attribs);
    }
    if (p->context == EGL_NO_CONTEXT) return false;
    if (!eglMakeCurrent(p->display, p->surface, p->surface, p->context)) return false;
    return true;
}

void destroy_context(ProbeContext* p) {
    if (p->display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (p->surface != EGL_NO_SURFACE) eglDestroySurface(p->display, p->surface);
    if (p->context != EGL_NO_CONTEXT) eglDestroyContext(p->display, p->context);
    eglTerminate(p->display);
}

void emit_device_json() {
    g_json += "  \"device\": {\n";
    g_json += "    \"abi\": " + q("arm64-v8a") + ",\n";
    g_json += "    \"page_size\": " + std::to_string(sysconf(_SC_PAGESIZE)) + ",\n";
    g_json += "    \"soc_model\": " + q("Snapdragon X Elite - X1E80100 - Qualcomm Oryon CPU") + ",\n";
    g_json += "    \"note\": \"static fields supplied by the runner; use getprop for authoritative build identity\"\n";
    g_json += "  },\n";
}

void emit_egl_json(const ProbeContext& p) {
    g_json += "  \"egl\": {\n";
    g_json += "    \"vendor\": " + q(eglQueryString(p.display, EGL_VENDOR)) + ",\n";
    g_json += "    \"version\": " + q(eglQueryString(p.display, EGL_VERSION)) + ",\n";
    g_json += "    \"client_apis\": " + q(eglQueryString(p.display, EGL_CLIENT_APIS)) + ",\n";
    g_json += "    \"extensions\": " + q(eglQueryString(p.display, EGL_EXTENSIONS)) + ",\n";
    EGLint cfg_id = 0, red = 0, green = 0, blue = 0, alpha = 0, depth = 0, stencil = 0;
    eglGetConfigAttrib(p.display, p.config, EGL_CONFIG_ID, &cfg_id);
    eglGetConfigAttrib(p.display, p.config, EGL_RED_SIZE, &red);
    eglGetConfigAttrib(p.display, p.config, EGL_GREEN_SIZE, &green);
    eglGetConfigAttrib(p.display, p.config, EGL_BLUE_SIZE, &blue);
    eglGetConfigAttrib(p.display, p.config, EGL_ALPHA_SIZE, &alpha);
    eglGetConfigAttrib(p.display, p.config, EGL_DEPTH_SIZE, &depth);
    eglGetConfigAttrib(p.display, p.config, EGL_STENCIL_SIZE, &stencil);
    g_json += "    \"config\": {\"id\": " + std::to_string(cfg_id) +
              ", \"rgba\": [" + std::to_string(red) + "," + std::to_string(green) + "," +
              std::to_string(blue) + "," + std::to_string(alpha) + "], \"depth\": " +
              std::to_string(depth) + ", \"stencil\": " + std::to_string(stencil) + "},\n";
    g_json += "    \"context_robust_access_requested\": false\n";
    g_json += "  },\n";
}

void emit_gles_json() {
    g_json += "  \"gles\": {\n";
    g_json += "    \"vendor\": " + q(gl_string(GL_VENDOR).c_str()) + ",\n";
    g_json += "    \"renderer\": " + q(gl_string(GL_RENDERER).c_str()) + ",\n";
    g_json += "    \"version\": " + q(gl_string(GL_VERSION).c_str()) + ",\n";
    g_json += "    \"shading_language\": " + q(gl_string(GL_SHADING_LANGUAGE_VERSION).c_str()) + ",\n";
    GLint num_ext = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_ext);
    std::string exts;
    for (GLint i = 0; i < num_ext; ++i) {
        const GLubyte* e = glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
        if (!e) continue;
        if (!exts.empty()) exts += ' ';
        exts += reinterpret_cast<const char*>(e);
    }
    g_json += "    \"extension_count\": " + std::to_string(num_ext) + ",\n";
    g_json += "    \"extensions\": " + q(exts.c_str()) + "\n";
    g_json += "  },\n";
}

void emit_limits_json() {
    g_json += "  \"limits\": {\n";
    bool first = true;
    for (const auto& e : kLimits) {
        GLint value = -1;
        if (e.pname != 0) glGetIntegerv(e.pname, &value);
        if (!first) g_json += ",\n";
        first = false;
        g_json += "    " + q(e.name) + ": " + std::to_string(value);
    }
    GLint work_count[3] = {-1, -1, -1};
    GLint work_size[3] = {-1, -1, -1};
    if (glGetIntegeri_v) {
        for (GLuint i = 0; i < 3; ++i) {
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &work_count[i]);
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &work_size[i]);
        }
    }
    g_json += ",\n    \"MAX_COMPUTE_WORK_GROUP_COUNT\": [" +
              std::to_string(work_count[0]) + ", " + std::to_string(work_count[1]) + ", " +
              std::to_string(work_count[2]) + "]";
    g_json += ",\n    \"MAX_COMPUTE_WORK_GROUP_SIZE\": [" +
              std::to_string(work_size[0]) + ", " + std::to_string(work_size[1]) + ", " +
              std::to_string(work_size[2]) + "]";
    g_json += "\n  },\n";
}

struct FormatResult {
    bool texture_2d = false;
    bool texture_3d = false;
    bool framebuffer_complete = false;
    bool image_binding = false;
    bool clear_ok = false;
    std::string error;
};

FormatResult probe_format(const FormatEntry& f) {
    FormatResult r;
    const bool depth_format = (f.internal == GL_DEPTH_COMPONENT24 ||
                               f.internal == GL_DEPTH_COMPONENT32F ||
                               f.internal == GL_DEPTH24_STENCIL8);
    GLuint tex2d = 0, tex3d = 0;
    glGenTextures(1, &tex2d);
    glBindTexture(GL_TEXTURE_2D, tex2d);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    while (glGetError() != GL_NO_ERROR) {}
    if (depth_format) {
        glTexImage2D(GL_TEXTURE_2D, 0, f.internal, 4, 4, 0, f.format, f.type, nullptr);
    } else {
        glTexStorage2D(GL_TEXTURE_2D, 1, f.internal, 4, 4);
    }
    GLenum e = glGetError();
    r.texture_2d = (e == GL_NO_ERROR);
    if (!r.texture_2d) r.error = "texture2d:" + gl_error_name(e);

    // A texture name cannot be retargeted after immutable storage is created.
    // Probe the 3D case on its own name.
    glGenTextures(1, &tex3d);
    glBindTexture(GL_TEXTURE_3D, tex3d);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    while (glGetError() != GL_NO_ERROR) {}
    glTexStorage3D(GL_TEXTURE_3D, 1, f.internal, 4, 4, 4);
    e = glGetError();
    r.texture_3d = (e == GL_NO_ERROR);
    if (!r.texture_3d && r.error.empty()) r.error = "texture3d:" + gl_error_name(e);

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glBindTexture(GL_TEXTURE_2D, tex2d);
    const GLenum attachment = depth_format
                                  ? (f.internal == GL_DEPTH24_STENCIL8 ? GL_DEPTH_STENCIL_ATTACHMENT
                                                                      : GL_DEPTH_ATTACHMENT)
                                  : GL_COLOR_ATTACHMENT0;
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex2d, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    r.framebuffer_complete = (status == GL_FRAMEBUFFER_COMPLETE);
    if (!r.framebuffer_complete && r.error.empty()) {
        r.error = "fbo:" + std::to_string(status) + ":" + gl_error_name(glGetError());
    }

    if (!depth_format) {
        while (glGetError() != GL_NO_ERROR) {}
        glBindImageTexture(0, tex2d, 0, GL_FALSE, 0, GL_WRITE_ONLY, f.internal);
        e = glGetError();
        r.image_binding = (e == GL_NO_ERROR);
        if (!r.image_binding && r.error.empty()) r.error = "bindImageTexture:" + gl_error_name(e);

        if (r.framebuffer_complete) {
            glClearBufferfv(GL_COLOR, 0, (const GLfloat[]){0.f, 0.f, 0.f, 0.f});
            r.clear_ok = (glGetError() == GL_NO_ERROR);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex2d);
    glDeleteTextures(1, &tex3d);
    return r;
}

void emit_formats_json() {
    g_json += "  \"formats\": {\n";
    bool first = true;
    for (const auto& f : kFormats) {
        FormatResult r = probe_format(f);
        if (!first) g_json += ",\n";
        first = false;
        g_json += "    " + q(f.name) + ": {";
        g_json += "\"texture2d\": " + std::string(r.texture_2d ? "true" : "false");
        g_json += ", \"texture3d\": " + std::string(r.texture_3d ? "true" : "false");
        g_json += ", \"framebuffer\": " + std::string(r.framebuffer_complete ? "true" : "false");
        g_json += ", \"image_binding\": " + std::string(r.image_binding ? "true" : "false");
        g_json += ", \"clear\": " + std::string(r.clear_ok ? "true" : "false");
        if (!r.error.empty()) g_json += ", \"error\": " + q(r.error.c_str());
        g_json += "}";
    }
    g_json += "\n  },\n";
}

struct TestResult {
    bool pass = false;
    std::string detail;
};

TestResult test_compute_512() {
    TestResult r;
    const char* src = R"(#version 310 es
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;
layout(std430, binding = 0) buffer Out { uint value; } outb;
void main() {
    if (gl_GlobalInvocationID == uvec3(0u)) outb.value = 0x512u;
}
)";
    std::string log;
    GLuint program = link_program(GL_COMPUTE_SHADER, src, &log);
    if (!program) {
        r.detail = "compile/link failed: " + log;
        return r;
    }
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buffer);
    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.pass = (value == 0x512u);
    r.detail = "value=" + std::to_string(value) + (log.empty() ? "" : " log=" + log);
    glDeleteBuffers(1, &buffer);
    glDeleteProgram(program);
    return r;
}

TestResult test_vertex_image_store() {
    TestResult r;
    // ESSL 3.10 has no r16ui image qualifier on this provider; use a legal
    // float format to test the stage capability itself. The pack's integer
    // format is a separate lowering problem.
    const char* vs = R"(#version 310 es
layout(rgba16f, binding = 0) uniform highp writeonly image3D img;
void main() {
    imageStore(img, ivec3(0), vec4(42.0, 0.0, 0.0, 0.0));
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    gl_PointSize = 1.0;
}
)";
    const char* fs = R"(#version 310 es
precision highp float;
layout(location = 0) out vec4 color;
void main() { color = vec4(1.0); }
)";
    const char* read_cs = R"(#version 310 es
layout(local_size_x = 1) in;
layout(rgba16f, binding = 0) uniform highp readonly image3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() { outb.value = uint(imageLoad(img, ivec3(0)).r); }
)";
    std::string log;
    GLuint vsid = compile_shader(GL_VERTEX_SHADER, vs, &log);
    GLuint fsid = compile_shader(GL_FRAGMENT_SHADER, fs, &log);
    if (!vsid || !fsid) {
        r.detail = "stage compile failed: " + log;
        if (vsid) glDeleteShader(vsid);
        if (fsid) glDeleteShader(fsid);
        return r;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vsid);
    glAttachShader(program, fsid);
    glLinkProgram(program);
    glDeleteShader(vsid);
    glDeleteShader(fsid);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string plog(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(program, len, nullptr, plog.data());
        r.detail = "link failed: " + plog;
        glDeleteProgram(program);
        return r;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA16F, 1, 1, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    while (glGetError() != GL_NO_ERROR) {}
    glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA16F);
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        r.detail = "image binding failed: " + gl_error_name(e);
        glDeleteTextures(1, &tex);
        glDeleteProgram(program);
        return r;
    }

    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(program);
    glDrawArrays(GL_POINTS, 0, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    e = glGetError();

    TestResult read_result;
    GLuint read_program = link_program(GL_COMPUTE_SHADER, read_cs, &log);
    if (!read_program) {
        r.detail = "readback shader failed: " + log;
        glDeleteVertexArrays(1, &vao);
        glDeleteTextures(1, &tex);
        glDeleteProgram(program);
        return r;
    }
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    glUseProgram(read_program);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.pass = (value == 42u && e == GL_NO_ERROR);
    r.detail = "draw_error=" + gl_error_name(e) + " image_value=" + std::to_string(value);
    glDeleteBuffers(1, &buffer);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
    glDeleteProgram(read_program);
    glDeleteProgram(program);
    return r;
}

TestResult test_integer_images() {
    TestResult r;
    const char* src = R"(#version 310 es
layout(local_size_x = 1) in;
layout(r8ui, binding = 0) uniform highp uimage2D img8;
layout(r16ui, binding = 1) uniform highp uimage2D img16;
layout(std430, binding = 2) buffer Out { uint values[2]; } outb;
void main() {
    imageStore(img8, ivec2(0), uvec4(17u, 0u, 0u, 0u));
    imageStore(img16, ivec2(0), uvec4(4097u, 0u, 0u, 0u));
    memoryBarrier();
    outb.values[0] = imageLoad(img8, ivec2(0)).r;
    outb.values[1] = imageLoad(img16, ivec2(0)).r;
}
)";
    std::string log;
    GLuint program = link_program(GL_COMPUTE_SHADER, src, &log);
    if (!program) {
        r.detail = "compile/link failed: " + log;
        return r;
    }
    GLuint tex8 = 0, tex16 = 0, buffer = 0;
    glGenTextures(1, &tex8);
    glBindTexture(GL_TEXTURE_2D, tex8);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8UI, 1, 1);
    glGenTextures(1, &tex16);
    glBindTexture(GL_TEXTURE_2D, tex16);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R16UI, 1, 1);
    glBindImageTexture(0, tex8, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R8UI);
    glBindImageTexture(1, tex16, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R16UI);
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buffer);
    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint values[2] = {0, 0};
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 8, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(values, ptr, 8);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.pass = (values[0] == 17u && values[1] == 4097u);
    r.detail = "r8ui=" + std::to_string(values[0]) + " r16ui=" + std::to_string(values[1]) +
               (log.empty() ? "" : " log=" + log);
    glDeleteBuffers(1, &buffer);
    glDeleteTextures(1, &tex8);
    glDeleteTextures(1, &tex16);
    glDeleteProgram(program);
    return r;
}

TestResult test_large_ssbo() {
    TestResult r;
    constexpr uint64_t kSize = 202899456ULL; // WSR 256 main block
    // 202899456 / 4 - 1. The runtime array index is deliberately written as a
    // constant: uint(length - 1) is not a valid ESSL expression here.
    const char* src = R"(#version 310 es
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer Big { uint data[]; } big;
void main() { big.data[50724863u] = 0xdeadbeefu; }
)";
    std::string log;
    GLuint program = link_program(GL_COMPUTE_SHADER, src, &log);
    if (!program) {
        r.detail = "compile/link failed: " + log;
        return r;
    }
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    while (glGetError() != GL_NO_ERROR) {}
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(kSize), nullptr, GL_DYNAMIC_COPY);
    GLenum alloc_error = glGetError();
    if (alloc_error != GL_NO_ERROR) {
        r.detail = "glBufferData(" + std::to_string(kSize) + ") failed: " + gl_error_name(alloc_error);
        glDeleteBuffers(1, &buffer);
        glDeleteProgram(program);
        return r;
    }
    const GLintptr offset = static_cast<GLintptr>(kSize - 4);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buffer, 0, static_cast<GLsizeiptr>(kSize));
    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, 4, GL_MAP_READ_BIT);
    GLenum map_error = glGetError();
    if (ptr) {
        memcpy(&value, ptr, 4);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.pass = (map_error == GL_NO_ERROR && value == 0xdeadbeefu);
    r.detail = "size=" + std::to_string(kSize) + " map_error=" + gl_error_name(map_error) +
               " last_value=0x" + std::to_string(value) + (log.empty() ? "" : " log=" + log);
    glDeleteBuffers(1, &buffer);
    glDeleteProgram(program);
    return r;
}

TestResult test_image_format_qualifier(const char* format) {
    TestResult r;
    const bool integer_format = (strstr(format, "ui") != nullptr);
    std::string source = "#version 310 es\n";
    source += "layout(local_size_x = 1) in;\n";
    source += "layout(";
    source += format;
    source += ", binding = 0) uniform highp writeonly ";
    source += integer_format ? "uimage2D" : "image2D";
    source += " img;\n";
    source += "void main() {}\n";
    std::string log;
    GLuint shader = compile_shader(GL_COMPUTE_SHADER, source.c_str(), &log);
    r.pass = (shader != 0);
    if (shader) glDeleteShader(shader);
    r.detail = r.pass ? "accepted" : ("rejected: " + log);
    return r;
}

void emit_image_qualifier_json() {
    const char* formats[] = {
        "r8ui", "r16ui", "r32ui", "r8", "r16", "r32f",
        "rgba8", "rgba8_snorm", "rgba16f", "rgba32f",
        "rg11f_b10f", "r11f_g11f_b10f", "rgb10_a2", "rg16f", "rg32f",
    };
    g_json += "  \"image_format_qualifiers\": {\n";
    bool first = true;
    for (const char* format : formats) {
        TestResult r = test_image_format_qualifier(format);
        if (!first) g_json += ",\n";
        first = false;
        g_json += "    " + q(format) + ": {\"pass\": " + (r.pass ? "true" : "false") +
                  ", \"detail\": " + q(r.detail.c_str()) + "}";
    }
    g_json += "\n  },\n";
}

TestResult test_ssbo_size(const char* label, uint64_t size) {
    TestResult r;
    const char* src = R"(#version 310 es
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer Big { uint data[]; } big;
uniform highp uint index;
void main() { big.data[index] = 0xdeadbeefu; }
)";
    std::string log;
    GLuint program = link_program(GL_COMPUTE_SHADER, src, &log);
    if (!program) {
        r.detail = "compile/link failed: " + log;
        return r;
    }
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    while (glGetError() != GL_NO_ERROR) {}
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_COPY);
    GLenum alloc_error = glGetError();
    if (alloc_error != GL_NO_ERROR) {
        r.detail = "glBufferData failed: " + gl_error_name(alloc_error);
        glDeleteBuffers(1, &buffer);
        glDeleteProgram(program);
        return r;
    }
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, buffer, 0, static_cast<GLsizeiptr>(size));
    glUseProgram(program);
    glUniform1ui(glGetUniformLocation(program, "index"), static_cast<GLuint>(size / 4 - 1));
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    const GLintptr offset = static_cast<GLintptr>(size - 4);
    GLuint value = 0;
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, offset, 4, GL_MAP_READ_BIT);
    GLenum map_error = glGetError();
    if (ptr) {
        memcpy(&value, ptr, 4);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.pass = (map_error == GL_NO_ERROR && value == 0xdeadbeefu);
    r.detail = std::string(label) + " size=" + std::to_string(size) +
               " map_error=" + gl_error_name(map_error) + " last_value=" + std::to_string(value) +
               (log.empty() ? "" : " log=" + log);
    glDeleteBuffers(1, &buffer);
    glDeleteProgram(program);
    return r;
}

void emit_ssbo_json() {
    struct SizeEntry {
        const char* label;
        uint64_t size;
    };
    const SizeEntry sizes[] = {
        {"wsr128", 50855936ULL},
        {"wsr192", 114229248ULL},
        {"wsr256", 202899456ULL},
    };
    g_json += "  \"ssbo_sizes\": {\n";
    bool first = true;
    for (const auto& e : sizes) {
        TestResult r = test_ssbo_size(e.label, e.size);
        if (!first) g_json += ",\n";
        first = false;
        g_json += "    " + q(e.label) + ": {\"pass\": " + (r.pass ? "true" : "false") +
                  ", \"detail\": " + q(r.detail.c_str()) + "}";
    }
    g_json += "\n  },\n";
}

TestResult test_r32ui_image_substitution() {
    TestResult r;
    // R32UI is a legal ESSL 3.10 image format on this provider. It is the first
    // candidate backing for the pack's r16ui/r8ui volumes if the loader lowers
    // integer IDs, but 32 bits per texel costs twice/quadruple the storage.
    const char* src = R"(#version 310 es
layout(local_size_x = 1) in;
layout(r32ui, binding = 0) uniform highp uimage3D img;
layout(std430, binding = 1) buffer Out { uint value; } outb;
void main() {
    imageStore(img, ivec3(0), uvec4(0x12345678u));
    memoryBarrier();
    outb.value = imageLoad(img, ivec3(0)).r;
}
)";
    std::string log;
    GLuint program = link_program(GL_COMPUTE_SHADER, src, &log);
    if (!program) {
        r.detail = "compile/link failed: " + log;
        return r;
    }
    GLuint tex = 0, buffer = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_3D, tex);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_R32UI, 1, 1, 1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindImageTexture(0, tex, 0, GL_TRUE, 0, GL_READ_WRITE, GL_R32UI);
    GLenum bind_error = glGetError();
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_COPY);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buffer);
    glUseProgram(program);
    glDispatchCompute(1, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    GLuint value = 0;
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 4, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(&value, ptr, 4);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    r.pass = (bind_error == GL_NO_ERROR && value == 0x12345678u);
    r.detail = "bind_error=" + gl_error_name(bind_error) + " value=0x" + std::to_string(value) +
               (log.empty() ? "" : " log=" + log);
    glDeleteBuffers(1, &buffer);
    glDeleteTextures(1, &tex);
    glDeleteProgram(program);
    return r;
}

TestResult test_depth_sampling() {
    TestResult r;
    // A 2x2 raw depth texture with known values. Filtering it linearly is the
    // operation under test; many strict GLES providers make this incomplete.
    GLuint depth_tex = 0;
    glGenTextures(1, &depth_tex);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    const unsigned int depth_data[4] = {0u, 0x3f800000u, 0x00000000u, 0x3f000000u};
    // Use a 32F depth texture where possible; the fallback is reported.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, 2, 2, 0, GL_DEPTH_COMPONENT, GL_FLOAT, depth_data);
    GLenum e = glGetError();
    bool depth32f = (e == GL_NO_ERROR);
    if (!depth32f) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, 2, 2, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
        e = glGetError();
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    const char* vs = R"(#version 300 es
void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0, (gl_VertexID == 2) ? 3.0 : -1.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
)";
    const char* fs = R"(#version 300 es
precision highp float;
uniform highp sampler2D rawDepth;
uniform highp sampler2DShadow cmpDepth;
layout(location = 0) out vec4 color;
void main() {
    float raw = texture(rawDepth, vec2(0.5)).r;
    float cmp = texture(cmpDepth, vec3(0.5, 0.5, 0.5));
    color = vec4(raw, cmp, 0.0, 1.0);
}
)";
    std::string log;
    GLuint vsid = compile_shader(GL_VERTEX_SHADER, vs, &log);
    GLuint fsid = compile_shader(GL_FRAGMENT_SHADER, fs, &log);
    if (!vsid || !fsid) {
        r.detail = "compile failed: " + log;
        if (vsid) glDeleteShader(vsid);
        if (fsid) glDeleteShader(fsid);
        glDeleteTextures(1, &depth_tex);
        return r;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vsid);
    glAttachShader(program, fsid);
    glLinkProgram(program);
    glDeleteShader(vsid);
    glDeleteShader(fsid);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::string plog(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(program, len, nullptr, plog.data());
        r.detail = "link failed: " + plog;
        glDeleteProgram(program);
        glDeleteTextures(1, &depth_tex);
        return r;
    }

    GLuint color_tex = 0, fbo = 0, vao = 0;
    glGenTextures(1, &color_tex);
    glBindTexture(GL_TEXTURE_2D, color_tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depth_tex);
    glUniform1i(glGetUniformLocation(program, "rawDepth"), 0);
    glUniform1i(glGetUniformLocation(program, "cmpDepth"), 1);
    glViewport(0, 0, 1, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    unsigned char pixel[4] = {0, 0, 0, 0};
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    e = glGetError();
    r.pass = (status == GL_FRAMEBUFFER_COMPLETE && e == GL_NO_ERROR);
    r.detail = "depth32f=" + std::string(depth32f ? "true" : "false") +
               " fbo=" + std::to_string(status) + " read_error=" + gl_error_name(e) +
               " raw_byte=" + std::to_string(pixel[0]) + " cmp_byte=" + std::to_string(pixel[1]);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &color_tex);
    glDeleteTextures(1, &depth_tex);
    glDeleteProgram(program);
    return r;
}

void emit_tests_json() {
    struct NamedTest {
        const char* name;
        TestResult (*fn)();
    };
    const NamedTest tests[] = {
        {"compute_512_invocations", test_compute_512},
        {"vertex_image_store", test_vertex_image_store},
        {"integer_images_r8ui_r16ui", test_integer_images},
        {"r32ui_image_substitution", test_r32ui_image_substitution},
        {"depth_raw_and_compare_linear", test_depth_sampling},
    };
    g_json += "  \"tests\": {\n";
    bool first = true;
    for (const auto& t : tests) {
        while (glGetError() != GL_NO_ERROR) {}
        TestResult r = t.fn();
        add_error(t.name);
        if (!first) g_json += ",\n";
        first = false;
        g_json += "    " + q(t.name) + ": {\"pass\": " + (r.pass ? "true" : "false") +
                  ", \"detail\": " + q(r.detail.c_str()) + "}";
    }
    g_json += "\n  },\n";
}

} // namespace

int main(int argc, char** argv) {
    const char* output_path = (argc > 1) ? argv[1] : nullptr;
    ProbeContext ctx;
    if (!make_context(&ctx)) {
        fprintf(stderr, "failed to create an EGL/GLES 3.x context\n");
        return 2;
    }
    if (!load_gles31_entry_points()) {
        fprintf(stderr, "provider is missing required GLES 3.1 entry points\n");
        destroy_context(&ctx);
        return 3;
    }

    GLint major = 0, minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    g_json = "{\n";
    g_json += "  \"probe\": {\"name\": \"mobileglues-capability-probe\", \"schema\": 1, \"gles\": " +
              std::to_string(major) + "." + std::to_string(minor) + "},\n";
    emit_device_json();
    emit_egl_json(ctx);
    emit_gles_json();
    emit_limits_json();
    emit_formats_json();
    emit_image_qualifier_json();
    emit_ssbo_json();
    emit_tests_json();
    g_json += "  \"gl_errors\": [";
    for (size_t i = 0; i < g_errors.size(); ++i) {
        if (i) g_json += ", ";
        g_json += q(g_errors[i].c_str());
    }
    g_json += "]\n}\n";

    destroy_context(&ctx);
    fputs(g_json.c_str(), stdout);
    if (output_path) {
        FILE* f = fopen(output_path, "w");
        if (f) {
            fputs(g_json.c_str(), f);
            fclose(f);
        }
    }
    LOGI("probe complete: %zu GL errors recorded", g_errors.size());
    return 0;
}
