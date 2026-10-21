// Real EGL/GLES buffer visibility checks. The provider build never links
// MobileGlues; the host adapter links the real buffer.cpp and getter.cpp.
#include "api.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>
#include <sys/utsname.h>
#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef MG_BUFFER_WRAPPER
using ErrorReader = GLenum (*)();
ErrorReader attach_buffer_wrapper(BufferApi&, void (*)(GLenum, GLint*), ErrorReader);
ErrorReader wrapper_error = nullptr;
#endif

namespace {
BufferApi buffers{};
std::string quote(const char* text) {
    std::string out = "\"";
    if (text) for (const unsigned char c : std::string(text)) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { char escaped[7]; std::snprintf(escaped, sizeof escaped, "\\u%04x", c); out += escaped; }
        else out += static_cast<char>(c);
    }
    return out + '"';
}
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
GLenum probe_error() {
    const GLenum provider = glGetError();
#ifdef MG_BUFFER_WRAPPER
    // Shader/render calls in this adapter use the raw provider. Inspect that
    // queue first, then the actual frontend queue used by buffer entry points.
    // The frontend getter otherwise classifies unpromoted provider errors as
    // internal diagnostics. Never let that hide a failed probe draw.
    require(wrapper_error != nullptr, "buffer frontend error reader was not attached");
    const GLenum frontend = wrapper_error();
    require(provider == GL_NO_ERROR || frontend == GL_NO_ERROR,
            "simultaneous provider and frontend errors");
    if (frontend != GL_NO_ERROR) return frontend;
#endif
    return provider;
}
GLenum buffer_error() {
#ifdef MG_BUFFER_WRAPPER
    require(wrapper_error != nullptr, "buffer frontend error reader was not attached");
    const GLenum unpromoted = glGetError();
    const GLenum frontend = wrapper_error();
    require(unpromoted == GL_NO_ERROR, "buffer rejection remained in provider queue instead of frontend");
    return frontend;
#else
    return glGetError();
#endif
}
void check_error(const char* operation) {
    const GLenum error = probe_error();
    if (error) throw std::runtime_error(std::string(operation) + " GL error " + std::to_string(error));
}
template<class T> T load(void* library, const char* name) {
    void* symbol = dlsym(library, name);
    if (!symbol) symbol = reinterpret_cast<void*>(eglGetProcAddress(name));
    require(symbol != nullptr, name);
    return reinterpret_cast<T>(symbol);
}
struct Context {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    void open() {
#ifdef __ANDROID__
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#else
        auto platform = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (platform) display = platform(EGL_PLATFORM_SURFACELESS_MESA, nullptr, nullptr);
        if (display == EGL_NO_DISPLAY) display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
        require(display != EGL_NO_DISPLAY && eglInitialize(display, nullptr, nullptr), "EGL initialize");
        require(eglBindAPI(EGL_OPENGL_ES_API), "EGL bind GLES");
        const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
        EGLConfig config{}; EGLint count = 0;
        require(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "EGL config");
        const EGLint size[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
        surface = eglCreatePbufferSurface(display, config, size);
        const EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
        require(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT &&
                eglMakeCurrent(display, surface, surface, context), "EGL current context");
    }
    ~Context() {
        if (display == EGL_NO_DISPLAY) return;
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
        eglTerminate(display);
    }
};
GLuint shader(GLenum stage, const char* source) {
    GLuint object = glCreateShader(stage);
    glShaderSource(object, 1, &source, nullptr);
    glCompileShader(object);
    GLint ok = 0; glGetShaderiv(object, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]{}; glGetShaderInfoLog(object, sizeof log, nullptr, log);
        glDeleteShader(object); throw std::runtime_error(log);
    }
    return object;
}
GLuint program() {
    const char* vertex = "#version 310 es\nlayout(location=0) in vec2 position;\n"
                         "void main(){gl_Position=vec4(position,0.0,1.0);}";
    const char* fragment = "#version 310 es\nprecision highp float;\n"
                           "layout(std140) uniform Globals { vec4 color; };\n"
                           "layout(location=0) out vec4 result; void main(){result=color;}";
    GLuint vs = shader(GL_VERTEX_SHADER, vertex), fs = shader(GL_FRAGMENT_SHADER, fragment);
    GLuint result = glCreateProgram(); glAttachShader(result, vs); glAttachShader(result, fs); glLinkProgram(result);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; glGetProgramiv(result, GL_LINK_STATUS, &ok);
    if (!ok) { char log[4096]{}; glGetProgramInfoLog(result, sizeof log, nullptr, log); glDeleteProgram(result); throw std::runtime_error(log); }
    return result;
}
void synchronize() {
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    require(fence != nullptr, "fence creation");
    GLenum result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 5000000000ULL);
    glDeleteSync(fence);
    require(result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED, "bounded GPU fence wait");
}
struct Storage {
    GLuint id = 0;
    GLenum target;
    GLintptr offset;
    GLsizeiptr size;
    void* pointer = nullptr;
    GLbitfield access;
    bool persistent;
    Storage(GLenum target_in, GLintptr offset_in, GLsizeiptr size_in, GLbitfield flags, GLbitfield access_in)
        : target(target_in), offset(offset_in), size(size_in), access(access_in),
          persistent((access_in & GL_MAP_PERSISTENT_BIT_EXT) != 0) {
        buffers.gen(1, &id);
        try {
            buffers.bind(target, id);
            buffers.storage(target, offset + size, nullptr, flags);
            check_error("storage");
            GLint actual = 0; buffers.parameter(target, GL_BUFFER_STORAGE_FLAGS_EXT, &actual);
            require(static_cast<GLbitfield>(actual) == flags, "storage flags changed");
        } catch (...) {
            buffers.remove(1, &id);
            throw;
        }
    }
    void write(const void* data) {
        buffers.bind(target, id);
        if (!pointer) pointer = buffers.map(target, offset, size, access);
        require(pointer != nullptr, "map returned null");
        GLint actual = 0; buffers.parameter(target, GL_BUFFER_ACCESS_FLAGS, &actual);
        require(static_cast<GLbitfield>(actual) == access, "mapping access flags changed");
        std::memcpy(pointer, data, static_cast<size_t>(size));
        if (access & GL_MAP_FLUSH_EXPLICIT_BIT) buffers.flush(target, 0, size);
        check_error("mapped write/flush");
        if (!persistent) {
            require(buffers.unmap(target), "transient unmap");
            pointer = nullptr;
        }
    }
    ~Storage() {
        if (pointer) { buffers.bind(target, id); buffers.unmap(target); }
        if (id) buffers.remove(1, &id);
    }
};
void visibility(GLbitfield flags, GLbitfield access, GLuint render_program, GLuint vao) {
    GLint alignment = 0; glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    require(alignment > 0, "uniform buffer alignment");
    Storage uniform(GL_UNIFORM_BUFFER, alignment, sizeof(float) * 4, flags, access);
    Storage vertex(GL_ARRAY_BUFFER, 64, sizeof(float) * 12, flags, access);
    glUseProgram(render_program);
    GLuint block = glGetUniformBlockIndex(render_program, "Globals");
    require(block != GL_INVALID_INDEX, "uniform block exists");
    glUniformBlockBinding(render_program, block, 2);
    buffers.bind_range(GL_UNIFORM_BUFFER, 2, uniform.id, uniform.offset, uniform.size);
    glBindVertexArray(vao);
    buffers.bind(GL_ARRAY_BUFFER, vertex.id);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<const void*>(vertex.offset));
    glEnableVertexAttribArray(0);
    glDisable(GL_DEPTH_TEST); glDisable(GL_SCISSOR_TEST); glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glViewport(0, 0, 64, 64);
    for (int pass = 0; pass < 2; ++pass) {
        const float left = pass == 0 ? -1.0f : 0.0f, right = left + 1.0f;
        const std::array<float, 12> positions{left,-1,right,-1,left,1,left,1,right,-1,right,1};
        const std::array<float, 4> color{pass == 0 ? 1.0f : 0.0f, pass == 1 ? 1.0f : 0.0f, 0, 1};
        uniform.write(color.data()); vertex.write(positions.data());
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        synchronize();
        std::array<unsigned char, 4> lit{}, dark{};
        glReadPixels(pass == 0 ? 16 : 48, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, lit.data());
        glReadPixels(pass == 0 ? 48 : 16, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, dark.data());
        check_error("draw/readback");
        require(lit[pass] == 255 && lit[1 - pass] == 0 && lit[2] == 0 && lit[3] == 255,
                "mapped UBO color or vertex placement was not visible");
        require(dark[0] == 0 && dark[1] == 0 && dark[2] == 0 && dark[3] == 255,
                "vertex update left stale geometry");
    }
    buffers.bind_range(GL_UNIFORM_BUFFER, 2, 0, 0, 0);
}
void invalid_flags() {
    GLuint id = 0; buffers.gen(1, &id); buffers.bind(GL_ARRAY_BUFFER, id);
    buffers.storage(GL_ARRAY_BUFFER, 64, nullptr, GL_MAP_PERSISTENT_BIT_EXT);
    GLenum error = buffer_error(); buffers.remove(1, &id);
    require(error == GL_INVALID_VALUE, "invalid storage flags were not rejected");
    buffers.gen(1, &id); buffers.bind(GL_ARRAY_BUFFER, id);
    buffers.storage(GL_ARRAY_BUFFER, 64, nullptr, GL_MAP_WRITE_BIT);
    check_error("invalid-map setup");
    void* mapping = buffers.map(GL_ARRAY_BUFFER, 0, 64, GL_MAP_WRITE_BIT | 0x80000000u);
    error = buffer_error();
    if (mapping) buffers.unmap(GL_ARRAY_BUFFER);
    buffers.remove(1, &id);
    require(mapping == nullptr && error == GL_INVALID_VALUE, "unknown mapping flags were not rejected");
}
void invalid_flush_range() {
    Storage storage(GL_ARRAY_BUFFER, 16, 32, GL_MAP_WRITE_BIT,
                    GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT);
    storage.pointer = buffers.map(storage.target, storage.offset, storage.size, storage.access);
    require(storage.pointer != nullptr, "flush-range setup mapping failed");
    std::memset(storage.pointer, 0, static_cast<size_t>(storage.size));
    check_error("flush-range setup");
    // Flush offsets are relative to this mapping, not the buffer's offset16.
    buffers.flush(storage.target, -1, 1);
    require(buffer_error() == GL_INVALID_VALUE, "negative flush offset was not rejected");
    buffers.flush(storage.target, storage.size - 1, 2);
    require(buffer_error() == GL_INVALID_VALUE, "flush past mapped range was not rejected");
    buffers.flush(storage.target, 0, storage.size);
    check_error("legal flush after invalid ranges");
    const GLboolean unmapped = buffers.unmap(storage.target);
    storage.pointer = nullptr;
    require(unmapped == GL_TRUE, "flush-range unmap failed");
    check_error("flush-range unmap");
}

}

int main() {
    Context context;
    std::string vendor, renderer, version, os;
    int failures = 0;
    std::vector<std::string> results;
    try {
        context.open();
        vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
#ifdef __ANDROID__
        char fingerprint[PROP_VALUE_MAX]{};
        __system_property_get("ro.build.fingerprint", fingerprint); os = fingerprint;
#else
        utsname info{}; require(uname(&info) == 0, "host OS identity");
        os = std::string(info.sysname) + " " + info.release + " " + info.machine;
#endif
        bool has_storage = false; GLint extension_count = 0; glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
        for (GLint i = 0; i < extension_count; ++i)
            has_storage |= std::strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i)), "GL_EXT_buffer_storage") == 0;
        require(has_storage, "provider does not advertise GL_EXT_buffer_storage");
#ifdef __ANDROID__
        void* library = dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
#else
        void* library = dlopen("libGLESv2.so.2", RTLD_NOW | RTLD_LOCAL);
#endif
        require(library != nullptr, "provider GLES library");
        buffers = {load<decltype(buffers.gen)>(library,"glGenBuffers"), load<decltype(buffers.remove)>(library,"glDeleteBuffers"),
                   load<decltype(buffers.bind)>(library,"glBindBuffer"), load<decltype(buffers.bind_range)>(library,"glBindBufferRange"),
                   load<decltype(buffers.storage)>(library,"glBufferStorageEXT"), load<decltype(buffers.map)>(library,"glMapBufferRange"),
                   load<decltype(buffers.unmap)>(library,"glUnmapBuffer"), load<decltype(buffers.flush)>(library,"glFlushMappedBufferRange"),
                   load<decltype(buffers.parameter)>(library,"glGetBufferParameteriv")};
#ifdef MG_BUFFER_WRAPPER
        wrapper_error = attach_buffer_wrapper(buffers, glGetIntegerv, glGetError);
#endif
        GLuint render_program = program(), vao = 0; glGenVertexArrays(1, &vao);
        auto run = [&](const char* name, auto test) {
            try { check_error("case entry"); test(); check_error("case cleanup"); results.push_back("{\"name\":" + quote(name) + ",\"pass\":true}"); }
            catch (const std::exception& e) { ++failures; results.push_back("{\"name\":" + quote(name) + ",\"pass\":false,\"detail\":" + quote(e.what()) + "}"); }
        };
        constexpr GLbitfield persistent = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT;
        run("persistent_explicit_0x72", [&]{visibility(persistent, persistent | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT, render_program, vao);});
        run("nonpersistent_explicit", [&]{visibility(GL_MAP_WRITE_BIT, GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT, render_program, vao);});
        run("persistent_coherent", [&]{visibility(persistent | GL_MAP_COHERENT_BIT_EXT, persistent | GL_MAP_COHERENT_BIT_EXT, render_program, vao);});
        run("invalid_flags", invalid_flags);
        run("invalid_flush_range", invalid_flush_range);
        glBindVertexArray(0); glDeleteVertexArrays(1, &vao); glUseProgram(0); glDeleteProgram(render_program);
        // Keep the library loaded until the EGL context has been destroyed.
    } catch (const std::exception& e) { ++failures; results.push_back("{\"name\":\"setup\",\"pass\":false,\"detail\":" + quote(e.what()) + "}"); }
#ifdef MG_BUFFER_WRAPPER
    const char* lane = "host real buffer.cpp with provider GLES rendering";
#else
    const char* lane = "below-wrapper provider GLES";
#endif
    std::printf("{\"lane\":%s,\"os\":%s,\"vendor\":%s,\"renderer\":%s,\"version\":%s,\"results\":[",
                quote(lane).c_str(), quote(os.c_str()).c_str(), quote(vendor.c_str()).c_str(), quote(renderer.c_str()).c_str(), quote(version.c_str()).c_str());
    for (size_t i = 0; i < results.size(); ++i) std::printf("%s%s", i ? "," : "", results[i].c_str());
    std::printf("],\"pass\":%s}\n", failures ? "false" : "true");
    return failures ? 1 : 0;
}
