// Exercise the real buffer.cpp forwarding paths. The recording backend checks
// API arguments and rejection propagation; GPU visibility is a separate probe.
#include <array>
#include <cstdio>
#include <vector>
#include "gl/buffer.h"

struct gles_func_t g_gles_func{};
global_settings_t global_settings{};
gl_state_s g_default_gl_state{};
thread_local gl_state_t gl_state = &g_default_gl_state;

namespace {
struct Call {
    GLenum target;
    GLintptr offset;
    GLsizeiptr length;
    GLbitfield flags;
    const void* data;
};
std::vector<Call> stores, maps, flushes;
std::vector<GLuint> bindings;
std::array<unsigned char, 1024> mapped{};
GLuint copy_write_binding = 77;
GLenum driver_error = GL_NO_ERROR;
int failures = 0;

void check(bool condition, const char* name) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
}
void storage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    stores.push_back({target, 0, size, flags, data});
    if ((flags & GL_MAP_PERSISTENT_BIT) && !(flags & (GL_MAP_WRITE_BIT | GL_MAP_READ_BIT)))
        driver_error = GL_INVALID_VALUE;
}
void* map_range(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield flags) {
    maps.push_back({target, offset, length, flags, nullptr});
    if (flags & 0x80000000u) {
        driver_error = GL_INVALID_VALUE;
        return nullptr;
    }
    return mapped.data();
}
void flush(GLenum target, GLintptr offset, GLsizeiptr length) {
    flushes.push_back({target, offset, length, 0, nullptr});
}
void get_integer(GLenum pname, GLint* value) {
    check(pname == GL_COPY_WRITE_BUFFER_BINDING, "borrow queries copy-write binding");
    *value = static_cast<GLint>(copy_write_binding);
}
void bind(GLenum target, GLuint buffer) {
    check(target == GL_COPY_WRITE_BUFFER, "borrow binds copy-write target");
    copy_write_binding = buffer;
    bindings.push_back(buffer);
}
}

int main() {
    GLES.glBufferStorageEXT = storage;
    GLES.glMapBufferRange = map_range;
    GLES.glFlushMappedBufferRange = flush;
    GLES.glGetIntegerv = get_integer;
    GLES.glBindBuffer = bind;

    constexpr GLbitfield persistent = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
    constexpr GLbitfield explicit_map = persistent | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    static_assert(explicit_map == 0x72);
    glBufferStorage(GL_UNIFORM_BUFFER, 1024, nullptr, persistent);
    check(stores.back().flags == persistent, "persistent storage flags are unchanged");
    check(stores.back().target == GL_UNIFORM_BUFFER && stores.back().length == 1024,
          "storage target and size are unchanged");
    check(glMapBufferRange(GL_UNIFORM_BUFFER, 256, 512, explicit_map) == mapped.data(),
          "backend mapping pointer propagates");
    check(maps.back().flags == explicit_map && maps.back().offset == 256 && maps.back().length == 512,
          "Minecraft persistent map 0x72 and range are unchanged");
    glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 12, 36);
    check(flushes.size() == 1 && flushes.back().target == GL_UNIFORM_BUFFER &&
          flushes.back().offset == 12 && flushes.back().length == 36,
          "flush uses caller range relative to mapping, not absolute map offset");

    const auto transient = GL_MAP_WRITE_BIT | GL_MAP_FLUSH_EXPLICIT_BIT;
    glMapBufferRange(GL_ARRAY_BUFFER, 64, 128, transient);
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 4, 8);
    check(maps.back().flags == transient && flushes.size() == 2 && flushes.back().offset == 4,
          "nonpersistent explicit mapping still flushes");

    constexpr auto coherent = persistent | GL_MAP_COHERENT_BIT;
    glBufferStorage(GL_SHADER_STORAGE_BUFFER, 512, mapped.data(), coherent);
    glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 512, coherent);
    check(stores.back().flags == coherent && stores.back().data == mapped.data() &&
          maps.back().flags == coherent, "explicit coherent storage and map remain coherent");

    glBufferStorage(GL_ARRAY_BUFFER, 512, nullptr, GL_DYNAMIC_STORAGE_BIT);
    check(stores.back().flags == GL_DYNAMIC_STORAGE_BIT, "dynamic storage receives no invented mapping rights");
    driver_error = GL_NO_ERROR;
    glBufferStorage(GL_ARRAY_BUFFER, 512, nullptr, GL_MAP_PERSISTENT_BIT);
    check(driver_error == GL_INVALID_VALUE && stores.back().flags == GL_MAP_PERSISTENT_BIT,
          "invalid persistent storage is not repaired into a valid allocation");
    driver_error = GL_NO_ERROR;
    check(glMapBufferRange(GL_ARRAY_BUFFER, 0, 128, GL_MAP_WRITE_BIT | 0x80000000u) == nullptr &&
          maps.back().flags == (GL_MAP_WRITE_BIT | 0x80000000u) && driver_error == GL_INVALID_VALUE,
          "unknown map flags reach the backend and rejection is propagated");

    // GL_PARAMETER_BUFFER is a wrapper-only target. Its real buffer is borrowed
    // through COPY_WRITE; every operation must restore the original binding.
    GLuint parameter = gen_buffer();
    modify_buffer(parameter, 901);
    glBindBuffer(GL_PARAMETER_BUFFER, parameter);
    glBufferStorage(GL_PARAMETER_BUFFER, 512, nullptr, persistent);
    glMapBufferRange(GL_PARAMETER_BUFFER, 32, 128, explicit_map);
    glFlushMappedBufferRange(GL_PARAMETER_BUFFER, 3, 5);
    check(stores.back().target == GL_COPY_WRITE_BUFFER && stores.back().flags == persistent,
          "borrowed storage preserves flags");
    check(maps.back().target == GL_COPY_WRITE_BUFFER && maps.back().offset == 32 &&
          maps.back().length == 128 && maps.back().flags == explicit_map, "borrowed map preserves flags and range");
    check(flushes.size() == 3 && flushes.back().target == GL_COPY_WRITE_BUFFER && flushes.back().offset == 3 &&
          flushes.back().length == 5, "borrowed flush preserves its relative range");
    check(bindings == std::vector<GLuint>({901, 77, 901, 77, 901, 77}) && copy_write_binding == 77,
          "all borrowed operations restore driver binding");
    check(find_bound_buffer_by_target(GL_PARAMETER_BUFFER) == parameter,
          "borrow leaves logical parameter binding intact");
    std::printf("buffer mapping forwarding: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
