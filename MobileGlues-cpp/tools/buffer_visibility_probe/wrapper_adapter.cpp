// Link the actual buffer translation unit to a real GLES provider. This adapter
// exercises buffer forwarding only; shaders and rendering stay below the wrapper.
#include "gl/buffer.h"
#include "api.h"

struct gles_func_t g_gles_func{};
struct gles_caps_t g_gles_caps{};
global_settings_t global_settings{};
gl_state_s g_default_gl_state{};
thread_local gl_state_t gl_state = &g_default_gl_state;

void attach_buffer_wrapper(BufferApi& api, void (*get_integer)(GLenum, GLint*)) {
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
    api = {glGenBuffers, glDeleteBuffers, glBindBuffer, glBindBufferRange, glBufferStorage,
           glMapBufferRange, glUnmapBuffer, glFlushMappedBufferRange, glGetBufferParameteriv};
}
