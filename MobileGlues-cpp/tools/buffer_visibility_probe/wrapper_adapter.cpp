// Link the actual buffer and error-query translation units to a real GLES
// provider. Shaders and rendering stay below the wrapper in this host adapter.
#include "gl/buffer.h"
#include "gl/getter.h"
#include "api.h"
#include <cstdarg>
#include <cstdio>

struct gles_func_t g_gles_func{};
struct gles_caps_t g_gles_caps{};
global_settings_t global_settings{};
gl_state_s g_default_gl_state{};
thread_local gl_state_t gl_state = &g_default_gl_state;

using ErrorReader = GLenum (*)();
ErrorReader attach_buffer_wrapper(BufferApi& api, void (*get_integer)(GLenum, GLint*), ErrorReader get_error) {
    GLES.glGenBuffers = api.gen;
    GLES.glDeleteBuffers = api.remove;
    GLES.glBindBuffer = api.bind;
    GLES.glBindBufferRange = api.bind_range;
    GLES.glBufferStorageEXT = api.storage;
    GLES.glMapBufferRange = api.map;
    GLES.glUnmapBuffer = api.unmap;
    GLES.glFlushMappedBufferRange = api.flush;
    GLES.glGetBufferParameteriv = api.parameter;
    GLES.glGetIntegerv = get_integer;
    GLES.glGetError = get_error;
    api = {glGenBuffers, glDeleteBuffers, glBindBuffer, glBindBufferRange, glBufferStorage,
           glMapBufferRange, glUnmapBuffer, glFlushMappedBufferRange, glGetBufferParameteriv};
    return glGetError;
}

// Preserve native diagnostic text in the host process without an Android log
// service or a shared renderer log file. These adapters do not alter GL errors.
int __android_log_print(int, const char*, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int written = std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
    return written;
}
extern "C" void write_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
    std::fputc('\n', stderr);
}
