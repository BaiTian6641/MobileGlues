// Complete MobileGlues DSO lane for the shared buffer visibility tests.
// All EGL/GL calls resolve from the requested DSO and are checked with dladdr.
// No system EGL/GLES link or symbol-resolution fallback is allowed.
#include "api.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>
#include <cstdlib>
#include <string_view>
#include <MG/extensions.h>
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


namespace {
void* library_handle = nullptr;
void* library_base = nullptr;
struct FullApi {
    decltype(&::eglBindAPI) eglBindAPI = nullptr;
    decltype(&::eglChooseConfig) eglChooseConfig = nullptr;
    decltype(&::eglCreateContext) eglCreateContext = nullptr;
    decltype(&::eglCreatePbufferSurface) eglCreatePbufferSurface = nullptr;
    decltype(&::eglDestroyContext) eglDestroyContext = nullptr;
    decltype(&::eglDestroySurface) eglDestroySurface = nullptr;
    decltype(&::eglGetDisplay) eglGetDisplay = nullptr;
    decltype(&::eglInitialize) eglInitialize = nullptr;
    decltype(&::eglMakeCurrent) eglMakeCurrent = nullptr;
    decltype(&::eglTerminate) eglTerminate = nullptr;
    decltype(&::glAttachShader) glAttachShader = nullptr;
    decltype(&::glBindVertexArray) glBindVertexArray = nullptr;
    decltype(&::glClear) glClear = nullptr;
    decltype(&::glClearColor) glClearColor = nullptr;
    decltype(&::glClientWaitSync) glClientWaitSync = nullptr;
    decltype(&::glCompileShader) glCompileShader = nullptr;
    decltype(&::glCreateProgram) glCreateProgram = nullptr;
    decltype(&::glCreateShader) glCreateShader = nullptr;
    decltype(&::glDeleteProgram) glDeleteProgram = nullptr;
    decltype(&::glDeleteShader) glDeleteShader = nullptr;
    decltype(&::glDeleteSync) glDeleteSync = nullptr;
    decltype(&::glDeleteVertexArrays) glDeleteVertexArrays = nullptr;
    decltype(&::glDisable) glDisable = nullptr;
    decltype(&::glDrawArrays) glDrawArrays = nullptr;
    decltype(&::glEnableVertexAttribArray) glEnableVertexAttribArray = nullptr;
    decltype(&::glFenceSync) glFenceSync = nullptr;
    decltype(&::glGenVertexArrays) glGenVertexArrays = nullptr;
    decltype(&::glGetError) glGetError = nullptr;
    decltype(&::glGetIntegerv) glGetIntegerv = nullptr;
    decltype(&::glGetProgramInfoLog) glGetProgramInfoLog = nullptr;
    decltype(&::glGetProgramiv) glGetProgramiv = nullptr;
    decltype(&::glGetShaderInfoLog) glGetShaderInfoLog = nullptr;
    decltype(&::glGetShaderiv) glGetShaderiv = nullptr;
    decltype(&::glGetString) glGetString = nullptr;
    decltype(&::glGetStringi) glGetStringi = nullptr;
    decltype(&::glGetUniformBlockIndex) glGetUniformBlockIndex = nullptr;
    decltype(&::glLinkProgram) glLinkProgram = nullptr;
    decltype(&::glReadPixels) glReadPixels = nullptr;
    decltype(&::glShaderSource) glShaderSource = nullptr;
    decltype(&::glUniformBlockBinding) glUniformBlockBinding = nullptr;
    decltype(&::glUseProgram) glUseProgram = nullptr;
    decltype(&::glVertexAttribPointer) glVertexAttribPointer = nullptr;
    decltype(&::glViewport) glViewport = nullptr;
} full;
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
GLenum probe_error() { return full.glGetError(); }
GLenum buffer_error() { return full.glGetError(); }
void check_error(const char* operation) {
    const GLenum error = probe_error();
    if (error) throw std::runtime_error(std::string(operation) + " GL error " + std::to_string(error));
}
template<class T> T load(void* library, const char* name) {
    void* symbol = dlsym(library, name);
    require(symbol != nullptr, name);
    Dl_info owner{};
    require(dladdr(symbol, &owner) && owner.dli_fbase == library_base, "symbol belongs to a dependency instead of requested DSO");
    return reinterpret_cast<T>(symbol);
}
void init_api(void* library) {
    full.eglBindAPI = load<decltype(full.eglBindAPI)>(library, "eglBindAPI");
    full.eglChooseConfig = load<decltype(full.eglChooseConfig)>(library, "eglChooseConfig");
    full.eglCreateContext = load<decltype(full.eglCreateContext)>(library, "eglCreateContext");
    full.eglCreatePbufferSurface = load<decltype(full.eglCreatePbufferSurface)>(library, "eglCreatePbufferSurface");
    full.eglDestroyContext = load<decltype(full.eglDestroyContext)>(library, "eglDestroyContext");
    full.eglDestroySurface = load<decltype(full.eglDestroySurface)>(library, "eglDestroySurface");
    full.eglGetDisplay = load<decltype(full.eglGetDisplay)>(library, "eglGetDisplay");
    full.eglInitialize = load<decltype(full.eglInitialize)>(library, "eglInitialize");
    full.eglMakeCurrent = load<decltype(full.eglMakeCurrent)>(library, "eglMakeCurrent");
    full.eglTerminate = load<decltype(full.eglTerminate)>(library, "eglTerminate");
    full.glAttachShader = load<decltype(full.glAttachShader)>(library, "glAttachShader");
    full.glBindVertexArray = load<decltype(full.glBindVertexArray)>(library, "glBindVertexArray");
    full.glClear = load<decltype(full.glClear)>(library, "glClear");
    full.glClearColor = load<decltype(full.glClearColor)>(library, "glClearColor");
    full.glClientWaitSync = load<decltype(full.glClientWaitSync)>(library, "glClientWaitSync");
    full.glCompileShader = load<decltype(full.glCompileShader)>(library, "glCompileShader");
    full.glCreateProgram = load<decltype(full.glCreateProgram)>(library, "glCreateProgram");
    full.glCreateShader = load<decltype(full.glCreateShader)>(library, "glCreateShader");
    full.glDeleteProgram = load<decltype(full.glDeleteProgram)>(library, "glDeleteProgram");
    full.glDeleteShader = load<decltype(full.glDeleteShader)>(library, "glDeleteShader");
    full.glDeleteSync = load<decltype(full.glDeleteSync)>(library, "glDeleteSync");
    full.glDeleteVertexArrays = load<decltype(full.glDeleteVertexArrays)>(library, "glDeleteVertexArrays");
    full.glDisable = load<decltype(full.glDisable)>(library, "glDisable");
    full.glDrawArrays = load<decltype(full.glDrawArrays)>(library, "glDrawArrays");
    full.glEnableVertexAttribArray = load<decltype(full.glEnableVertexAttribArray)>(library, "glEnableVertexAttribArray");
    full.glFenceSync = load<decltype(full.glFenceSync)>(library, "glFenceSync");
    full.glGenVertexArrays = load<decltype(full.glGenVertexArrays)>(library, "glGenVertexArrays");
    full.glGetError = load<decltype(full.glGetError)>(library, "glGetError");
    full.glGetIntegerv = load<decltype(full.glGetIntegerv)>(library, "glGetIntegerv");
    full.glGetProgramInfoLog = load<decltype(full.glGetProgramInfoLog)>(library, "glGetProgramInfoLog");
    full.glGetProgramiv = load<decltype(full.glGetProgramiv)>(library, "glGetProgramiv");
    full.glGetShaderInfoLog = load<decltype(full.glGetShaderInfoLog)>(library, "glGetShaderInfoLog");
    full.glGetShaderiv = load<decltype(full.glGetShaderiv)>(library, "glGetShaderiv");
    full.glGetString = load<decltype(full.glGetString)>(library, "glGetString");
    full.glGetStringi = load<decltype(full.glGetStringi)>(library, "glGetStringi");
    full.glGetUniformBlockIndex = load<decltype(full.glGetUniformBlockIndex)>(library, "glGetUniformBlockIndex");
    full.glLinkProgram = load<decltype(full.glLinkProgram)>(library, "glLinkProgram");
    full.glReadPixels = load<decltype(full.glReadPixels)>(library, "glReadPixels");
    full.glShaderSource = load<decltype(full.glShaderSource)>(library, "glShaderSource");
    full.glUniformBlockBinding = load<decltype(full.glUniformBlockBinding)>(library, "glUniformBlockBinding");
    full.glUseProgram = load<decltype(full.glUseProgram)>(library, "glUseProgram");
    full.glVertexAttribPointer = load<decltype(full.glVertexAttribPointer)>(library, "glVertexAttribPointer");
    full.glViewport = load<decltype(full.glViewport)>(library, "glViewport");
}
struct Context {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    void open() {
        display = full.eglGetDisplay(EGL_DEFAULT_DISPLAY);
        require(display != EGL_NO_DISPLAY && full.eglInitialize(display, nullptr, nullptr), "EGL initialize");
        require(full.eglBindAPI(EGL_OPENGL_ES_API), "EGL bind GLES");
        const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
        EGLConfig config{}; EGLint count = 0;
        require(full.eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "EGL config");
        const EGLint size[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
        surface = full.eglCreatePbufferSurface(display, config, size);
        const EGLint version[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = full.eglCreateContext(display, config, EGL_NO_CONTEXT, version);
        require(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT &&
                full.eglMakeCurrent(display, surface, surface, context), "EGL current context");
    }
    bool close() {
        if (display == EGL_NO_DISPLAY) return true;
        bool ok = full.eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) ok = full.eglDestroyContext(display, context) && ok;
        if (surface != EGL_NO_SURFACE) ok = full.eglDestroySurface(display, surface) && ok;
        ok = full.eglTerminate(display) && ok;
        display = EGL_NO_DISPLAY; context = EGL_NO_CONTEXT; surface = EGL_NO_SURFACE;
        return ok;
    }
    ~Context() { close(); }
};
// Generated from the provider probe so both lanes execute the same test bodies.
#include "full_wrapper_cases.inc"

}

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "usage: %s /absolute/libmobileglues.so /absolute/output.json expected-source-marker\n", argv[0]); return 2; }
    if (argv[1][0] != '/' || argv[2][0] != '/' || !argv[3][0]) {
        std::fprintf(stderr, "Library/output paths must be absolute and expected marker nonempty\n"); return 2;
    }
    Context context;
    std::string vendor, renderer, version, os, backend_vendor, backend_renderer, backend_version, marker;
    int failures = 0;
    std::vector<std::string> results;
    try {
#ifdef __ANDROID__
        char fingerprint[PROP_VALUE_MAX]{};
        __system_property_get("ro.build.fingerprint", fingerprint); os = fingerprint;
#else
        utsname info{}; require(uname(&info) == 0, "host OS identity");
        os = std::string(info.sysname) + " " + info.release + " " + info.machine;
#endif
        const char* config_dir = std::getenv("MG_DIR_PATH");
        constexpr std::string_view temporary_prefix = "/data/local/tmp/";
        const std::string_view config_path = config_dir ? config_dir : "";
        require(config_path.starts_with(temporary_prefix) && config_path.size() > temporary_prefix.size() &&
                config_path.find("..") == std::string_view::npos, "MG_DIR_PATH must select isolated /data/local/tmp directory");
        library_handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!library_handle) throw std::runtime_error(dlerror());
        void* marker_symbol = dlsym(library_handle, "mg_get_build_id");
        Dl_info owner{};
        require(marker_symbol && dladdr(marker_symbol, &owner), "MobileGlues build marker export");
        library_base = owner.dli_fbase;
        init_api(library_handle);
        const char* marker_value = load<const char* (*)()>(library_handle, "mg_get_build_id")();
        require(marker_value != nullptr, "null MobileGlues marker");
        marker = marker_value;
        require(marker == argv[3], "unexpected MobileGlues source marker");
        context.open();
        check_error("context creation");
        auto identity = [](GLenum name) {
            const auto* value = full.glGetString(name);
            require(value != nullptr, "frontend identity unavailable");
            return std::string(reinterpret_cast<const char*>(value));
        };
        vendor = identity(GL_VENDOR);
        renderer = identity(GL_RENDERER);
        version = identity(GL_VERSION);
        auto backend_string = [](GLenum name) {
            const auto* value = full.glGetString(name + GL_BACKEND_GETTER_MG);
            require(value != nullptr, "backend identity unavailable");
            return std::string(reinterpret_cast<const char*>(value));
        };
        backend_vendor = backend_string(GL_VENDOR);
        backend_renderer = backend_string(GL_RENDERER);
        backend_version = backend_string(GL_VERSION);
        const std::string settings = identity(GL_SETTINGS_MG);
        require(settings.find("IgnoreError: None\n") != std::string::npos, "qualification requires error ignoring disabled");
        check_error("identity query");

        bool has_storage = false; GLint extension_count = 0; full.glGetIntegerv(GL_NUM_EXTENSIONS, &extension_count);
        require(extension_count >= 0 && extension_count < 100000, "invalid extension count");
        for (GLint i = 0; i < extension_count; ++i) {
            const auto* extension = full.glGetStringi(GL_EXTENSIONS, i);
            require(extension != nullptr, "null extension string");
            has_storage |= std::strcmp(reinterpret_cast<const char*>(extension), "GL_ARB_buffer_storage") == 0;
        }
        check_error("extension query");
        require(has_storage, "wrapper does not advertise GL_ARB_buffer_storage");
        void* library = library_handle;
        buffers = {load<decltype(buffers.gen)>(library,"glGenBuffers"), load<decltype(buffers.remove)>(library,"glDeleteBuffers"),
                   load<decltype(buffers.bind)>(library,"glBindBuffer"), load<decltype(buffers.bind_range)>(library,"glBindBufferRange"),
                   load<decltype(buffers.storage)>(library,"glBufferStorage"), load<decltype(buffers.map)>(library,"glMapBufferRange"),
                   load<decltype(buffers.unmap)>(library,"glUnmapBuffer"), load<decltype(buffers.flush)>(library,"glFlushMappedBufferRange"),
                   load<decltype(buffers.parameter)>(library,"glGetBufferParameteriv")};
        GLuint render_program = program(), vao = 0; full.glGenVertexArrays(1, &vao);
        check_error("shader/VAO setup");
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
        // Minecraft 26.2's GlBuffer.Direct uses the DSA entry points, not the
        // target-bound ones. Exercise the same 0x72 mapping and verify that an
        // explicit flush makes the written bytes visible to a readback.
        run("dsa_persistent_explicit_0x72", [&]{
            using StorageFn = void (*)(GLuint, GLsizeiptr, const void*, GLbitfield);
            using MapFn = void* (*)(GLuint, GLintptr, GLsizeiptr, GLbitfield);
            using UnmapFn = GLboolean (*)(GLuint);
            using FlushFn = void (*)(GLuint, GLintptr, GLsizeiptr);
            using CopyFn = void (*)(GLuint, GLuint, GLintptr, GLintptr, GLsizeiptr);
            const auto storage = load<StorageFn>(library, "glNamedBufferStorage");
            const auto map = load<MapFn>(library, "glMapNamedBufferRange");
            const auto unmap = load<UnmapFn>(library, "glUnmapNamedBuffer");
            const auto flush = load<FlushFn>(library, "glFlushMappedNamedBufferRange");
            const auto copy = load<CopyFn>(library, "glCopyNamedBufferSubData");
            GLuint source = 0, destination = 0;
            buffers.gen(1, &source); buffers.gen(1, &destination);
            require(source != 0 && destination != 0, "DSA buffer generation failed");
            constexpr GLbitfield persistent = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT_EXT;
            storage(source, 64, nullptr, persistent);
            storage(destination, 64, nullptr, GL_MAP_READ_BIT);
            check_error("DSA storage");
            auto* mapping = static_cast<unsigned char*>(
                    map(source, 0, 64, persistent | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT));
            require(mapping != nullptr, "DSA mapping failed");
            for (int i = 0; i < 64; ++i) mapping[i] = static_cast<unsigned char>(i * 3 + 7);
            flush(source, 0, 64);
            check_error("DSA explicit flush");
            // Minecraft 26.2 keeps its staging buffer mapped while copying from
            // it. Copy before unmap so the test covers that exact sequence.
            copy(source, destination, 0, 0, 64);
            check_error("DSA mapped-source copy");
            const GLsync fence = full.glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            require(fence != nullptr, "DSA fence failed");
            const GLenum wait = full.glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
            full.glDeleteSync(fence);
            require(wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED, "DSA fence did not complete");
            require(unmap(source) == GL_TRUE, "DSA source unmap failed");
            check_error("DSA source unmap");
            auto* readback = static_cast<unsigned char*>(map(destination, 0, 64, GL_MAP_READ_BIT));
            require(readback != nullptr, "DSA destination mapping failed");
            for (int i = 0; i < 64; ++i)
                require(readback[i] == static_cast<unsigned char>(i * 3 + 7), "DSA flushed bytes were not visible after copy");
            require(unmap(destination) == GL_TRUE, "DSA destination unmap failed");
            buffers.remove(1, &source); buffers.remove(1, &destination);
        });
        full.glBindVertexArray(0); full.glDeleteVertexArrays(1, &vao); full.glUseProgram(0); full.glDeleteProgram(render_program);
        check_error("program/VAO cleanup");
        // Keep the library loaded until the EGL context has been destroyed.
    } catch (const std::exception& e) { ++failures; results.push_back("{\"name\":\"setup\",\"pass\":false,\"detail\":" + quote(e.what()) + "}"); }
    if (!context.close()) { ++failures; results.push_back("{\"name\":\"context_cleanup\",\"pass\":false}"); }
    const char* lane = "full MobileGlues DSO (EGL and all GL frontend entry points)";
    FILE* output = std::fopen(argv[2], "w");
    if (!output) { std::perror("open JSON output"); return 3; }
    std::fprintf(output, "{\"lane\":%s,\"os\":%s,\"vendor\":%s,\"renderer\":%s,\"version\":%s,\"backend_vendor\":%s,\"backend_renderer\":%s,\"backend_version\":%s,\"build_marker\":%s,\"backend_identity_available\":%s,\"egl_api_requested\":\"EGL_OPENGL_ES_API\",\"egl_context_client_version_requested\":3,\"shader_dialect\":\"GLSL 330 core\",\"results\":[",
                quote(lane).c_str(), quote(os.c_str()).c_str(), quote(vendor.c_str()).c_str(), quote(renderer.c_str()).c_str(), quote(version.c_str()).c_str(),
                quote(backend_vendor.c_str()).c_str(), quote(backend_renderer.c_str()).c_str(), quote(backend_version.c_str()).c_str(), quote(marker.c_str()).c_str(), backend_version.empty() ? "false" : "true");
    for (size_t i = 0; i < results.size(); ++i) std::fprintf(output, "%s%s", i ? "," : "", results[i].c_str());
    std::fprintf(output, "],\"pass\":%s}\n", failures ? "false" : "true");
    if (std::fclose(output) != 0) return 3;
    return failures ? 1 : 0;
}
