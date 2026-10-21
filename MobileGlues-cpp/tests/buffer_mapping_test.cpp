// Exercise real buffer.cpp and getter.cpp against a recording backend. These
// tests cover arguments and application errors; GPU visibility needs its probe.
#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>
#include "gl/buffer.h"
#include "gl/getter.h"

// Internal tracked state from the real buffer translation unit.
void set_bound_buffer_by_target(GLenum target, GLuint buffer);
size_t get_buffer_data_size(GLuint buffer);

struct gles_func_t g_gles_func{};
global_settings_t global_settings{};
gl_state_s g_default_gl_state{};
thread_local gl_state_t gl_state = &g_default_gl_state;
int __android_log_print(int, const char*, const char*, ...) { return 0; }
extern "C" void write_log(const char*, ...) {}

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
std::vector<std::string> events;
std::deque<GLenum> driver_errors;
std::array<unsigned char, 1024> mapped{};
GLuint copy_write_binding = 77;
GLenum allocation_error = GL_NO_ERROR;
GLenum query_error = GL_NO_ERROR;
GLenum bind_error = GL_NO_ERROR;
GLenum restore_error = GL_NO_ERROR;
GLenum repeating_error = GL_NO_ERROR;
int failures = 0;
int checks = 0;

void check(bool condition, const char* name) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); ++failures; }
}
void queue_error(GLenum error) {
    if (error != GL_NO_ERROR) driver_errors.push_back(error);
}
GLenum get_error() {
    events.emplace_back("error");
    if (driver_errors.empty()) return repeating_error;
    const GLenum error = driver_errors.front();
    driver_errors.pop_front();
    return error;
}
void check_error(GLenum expected, const char* name) {
    check(glGetError() == expected, name);
    check(glGetError() == GL_NO_ERROR && driver_errors.empty(), "error is consumed once, backend queue drained");
}
void storage(GLenum target, GLsizeiptr size, const void* data, GLbitfield flags) {
    events.emplace_back("storage");
    stores.push_back({target, 0, size, flags, data});
    if (allocation_error != GL_NO_ERROR) {
        queue_error(allocation_error);
    } else if (size <= 0 || (flags & 0x80000000u) ||
               ((flags & GL_MAP_PERSISTENT_BIT) && !(flags & (GL_MAP_WRITE_BIT | GL_MAP_READ_BIT))) ||
               ((flags & GL_MAP_COHERENT_BIT) && !(flags & GL_MAP_PERSISTENT_BIT))) {
        queue_error(GL_INVALID_VALUE);
    }
}
void* map_range(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield flags) {
    events.emplace_back("map");
    maps.push_back({target, offset, length, flags, nullptr});
    if ((flags & 0x80000000u) || offset < 0 || length < 0 || length > 1024 || offset > 1024 - length) {
        queue_error(GL_INVALID_VALUE);
        return nullptr;
    }
    if (length == 0) {
        queue_error(GL_INVALID_OPERATION);
        return nullptr;
    }
    return mapped.data();
}
void flush(GLenum target, GLintptr offset, GLsizeiptr length) {
    events.emplace_back("flush");
    flushes.push_back({target, offset, length, 0, nullptr});
    if (offset < 0 || length < 0 || length > 512 || offset > 512 - length) queue_error(GL_INVALID_VALUE);
}
void get_integer(GLenum pname, GLint* value) {
    events.emplace_back("query");
    check(pname == GL_COPY_WRITE_BUFFER_BINDING, "borrow queries copy-write binding");
    if (query_error != GL_NO_ERROR) {
        queue_error(query_error);
        return;
    }
    *value = static_cast<GLint>(copy_write_binding);
}
void bind(GLenum target, GLuint buffer) {
    events.push_back("bind:" + std::to_string(buffer));
    check(target == GL_COPY_WRITE_BUFFER, "borrow binds copy-write target");
    bindings.push_back(buffer);
    const GLenum error = buffer == 77 ? restore_error : bind_error;
    if (error != GL_NO_ERROR) {
        queue_error(error);
        return;
    }
    copy_write_binding = buffer;
}
void operation(unsigned which, GLenum target, bool valid) {
    if (which == 0) glBufferStorage(target, valid ? 64 : -7, mapped.data(), GL_MAP_WRITE_BIT);
    if (which == 1) glMapBufferRange(target, valid ? 8 : -7, 16, GL_MAP_WRITE_BIT);
    if (which == 2) glFlushMappedBufferRange(target, valid ? 3 : -7, 4);
}
std::size_t call_count(unsigned which) {
    return which == 0 ? stores.size() : which == 1 ? maps.size() : flushes.size();
}
}

int main() {
    GLES.glBufferStorageEXT = storage;
    GLES.glMapBufferRange = map_range;
    GLES.glFlushMappedBufferRange = flush;
    GLES.glGetIntegerv = get_integer;
    GLES.glBindBuffer = bind;
    GLES.glGetError = get_error;

    const GLuint buffer = gen_buffer();
    modify_buffer(buffer, 801);
    set_bound_buffer_by_target(GL_UNIFORM_BUFFER, buffer);
    set_bound_buffer_by_target(GL_ARRAY_BUFFER, buffer);
    set_bound_buffer_by_target(GL_SHADER_STORAGE_BUFFER, buffer);
    constexpr GLbitfield persistent = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
    constexpr GLbitfield explicit_map = persistent | GL_MAP_FLUSH_EXPLICIT_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    static_assert(explicit_map == 0x72);
    glBufferStorage(GL_UNIFORM_BUFFER, 1024, nullptr, persistent);
    check(stores.back().flags == persistent, "persistent storage flags are unchanged");
    check(stores.back().target == GL_UNIFORM_BUFFER && stores.back().length == 1024 &&
          get_buffer_data_size(buffer) == 1024, "storage target, size and successful metadata are correct");
    check(glMapBufferRange(GL_UNIFORM_BUFFER, 256, 512, explicit_map) == mapped.data(),
          "backend mapping pointer propagates");
    check(maps.back().flags == explicit_map && maps.back().offset == 256 && maps.back().length == 512,
          "Minecraft persistent map 0x72 and range are unchanged");
    glFlushMappedBufferRange(GL_UNIFORM_BUFFER, 12, 36);
    check(flushes.size() == 1 && flushes.back().target == GL_UNIFORM_BUFFER &&
          flushes.back().offset == 12 && flushes.back().length == 36,
          "flush uses caller range relative to mapping, not absolute map offset");
    check_error(GL_NO_ERROR, "valid persistent sequence has no application error");

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
    check_error(GL_NO_ERROR, "valid transient and coherent operations have no application error");

    for (GLbitfield flags : {GLbitfield(GL_MAP_PERSISTENT_BIT), GLbitfield(GL_MAP_COHERENT_BIT), GLbitfield(0x80000000u)}) {
        glBufferStorage(GL_ARRAY_BUFFER, 333, nullptr, flags);
        check(stores.back().flags == flags && get_buffer_data_size(buffer) == 512,
              "invalid storage flags reach backend and failed allocation preserves size");
        check_error(GL_INVALID_VALUE, "storage rejection reaches actual frontend glGetError");
    }
    allocation_error = GL_OUT_OF_MEMORY;
    glBufferStorage(GL_ARRAY_BUFFER, 2048, nullptr, persistent);
    allocation_error = GL_NO_ERROR;
    check(get_buffer_data_size(buffer) == 512, "out-of-memory allocation preserves tracked size");
    check_error(GL_OUT_OF_MEMORY, "allocation error remains its original enum");
    check(glMapBufferRange(GL_ARRAY_BUFFER, 0, 128, GL_MAP_WRITE_BIT | 0x80000000u) == nullptr &&
          maps.back().flags == (GL_MAP_WRITE_BIT | 0x80000000u), "invalid mapping flags and null result propagate");
    check_error(GL_INVALID_VALUE, "map rejection reaches actual frontend glGetError");
    check(glMapBufferRange(GL_ARRAY_BUFFER, 8, 0, GL_MAP_WRITE_BIT) == nullptr,
          "zero-length rejected mapping remains null");
    check_error(GL_INVALID_OPERATION, "map rejection is not rewritten to a generic value error");
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 500, 13);
    check(flushes.back().offset == 500 && flushes.back().length == 13, "invalid flush range is unchanged");
    check_error(GL_INVALID_VALUE, "flush rejection reaches actual frontend glGetError");

    for (unsigned which = 0; which != 3; ++which) {
        driver_errors = {GL_INVALID_ENUM, GL_OUT_OF_MEMORY};
        operation(which, GL_ARRAY_BUFFER, true);
        check_error(GL_NO_ERROR, "stale backend diagnostics are not application errors");
        driver_errors = {GL_INVALID_ENUM, GL_OUT_OF_MEMORY};
        operation(which, GL_ARRAY_BUFFER, false);
        check_error(GL_INVALID_VALUE, "fresh application error wins over drained stale backend errors");
        mg_set_gl_error(GL_INVALID_ENUM);
        operation(which, GL_ARRAY_BUFFER, true);
        check_error(GL_INVALID_ENUM, "successful operation preserves pending frontend error");
        mg_set_gl_error(GL_INVALID_ENUM);
        operation(which, GL_ARRAY_BUFFER, false);
        check_error(GL_INVALID_ENUM, "first frontend error survives later application rejection");
    }

    // GL_PARAMETER_BUFFER exists only in the frontend. Every temporary binding
    // must be restored, including rejected application operations.
    const GLuint parameter = gen_buffer();
    modify_buffer(parameter, 901);
    glBindBuffer(GL_PARAMETER_BUFFER, parameter);
    bindings.clear();
    glBufferStorage(GL_PARAMETER_BUFFER, 512, nullptr, persistent);
    glMapBufferRange(GL_PARAMETER_BUFFER, 32, 128, explicit_map);
    glFlushMappedBufferRange(GL_PARAMETER_BUFFER, 3, 5);
    check(stores.back().target == GL_COPY_WRITE_BUFFER && stores.back().flags == persistent,
          "borrowed storage preserves flags");
    check(maps.back().target == GL_COPY_WRITE_BUFFER && maps.back().offset == 32 &&
          maps.back().length == 128 && maps.back().flags == explicit_map, "borrowed map preserves flags and range");
    check(flushes.back().target == GL_COPY_WRITE_BUFFER && flushes.back().offset == 3 &&
          flushes.back().length == 5, "borrowed flush preserves its relative range");
    check(bindings == std::vector<GLuint>({901, 77, 901, 77, 901, 77}) && copy_write_binding == 77,
          "all borrowed operations restore driver binding");
    check(find_bound_buffer_by_target(GL_PARAMETER_BUFFER) == parameter,
          "borrow leaves logical parameter binding intact");
    check_error(GL_NO_ERROR, "borrowed valid operations report no error");

    for (unsigned which = 0; which != 3; ++which) {
        const auto old_size = get_buffer_data_size(parameter);
        events.clear();
        operation(which, GL_PARAMETER_BUFFER, false);
        const auto app = which == 0 ? "storage" : which == 1 ? "map" : "flush";
        const auto position = std::find(events.begin(), events.end(), app);
        const auto restore = std::find(events.begin(), events.end(), "bind:77");
        check(position != events.end() && restore != events.end() && position < restore &&
              std::count(position + 1, restore, "error") >= 2,
              "application errors are consumed before borrowed binding restore");
        check(copy_write_binding == 77 && get_buffer_data_size(parameter) == old_size,
              "borrowed rejected operation restores binding and preserves allocation size");
        check_error(GL_INVALID_VALUE, "borrowed application rejection is promoted");

        restore_error = GL_INVALID_ENUM;
        operation(which, GL_PARAMETER_BUFFER, false);
        restore_error = GL_NO_ERROR;
        copy_write_binding = 77; // Recover the deliberately failed fake-driver restore.
        check_error(GL_INVALID_VALUE, "restore error cannot replace the application rejection");
        restore_error = GL_INVALID_ENUM;
        operation(which, GL_PARAMETER_BUFFER, true);
        restore_error = GL_NO_ERROR;
        copy_write_binding = 77;
        check_error(GL_NO_ERROR, "restore diagnostics are never promoted to application errors");

        const auto calls_before = call_count(which);
        const auto size_before = get_buffer_data_size(parameter);
        bindings.clear();
        query_error = GL_INVALID_ENUM;
        operation(which, GL_PARAMETER_BUFFER, true);
        query_error = GL_NO_ERROR;
        check(call_count(which) == calls_before && bindings.empty() && copy_write_binding == 77,
              "failed binding query skips application call without changing driver binding");
        check_error(GL_NO_ERROR, "binding query failure remains internal");
        bind_error = GL_INVALID_OPERATION;
        operation(which, GL_PARAMETER_BUFFER, true);
        bind_error = GL_NO_ERROR;
        check(call_count(which) == calls_before && bindings == std::vector<GLuint>({901, 77}) &&
              copy_write_binding == 77 && get_buffer_data_size(parameter) == size_before,
              "failed temporary bind skips application call, restores binding and preserves metadata");
        check_error(GL_NO_ERROR, "binding failure remains internal");
        driver_errors = {GL_INVALID_ENUM, GL_OUT_OF_MEMORY};
        operation(which, GL_PARAMETER_BUFFER, true);
        check(call_count(which) == calls_before + 1, "stale errors do not suppress a valid borrowed operation");
        check_error(GL_NO_ERROR, "borrowed operation does not promote stale diagnostics");
    }

    const auto store_count = stores.size();
    const auto old_size = get_buffer_data_size(parameter);
    GLES.glBufferStorageEXT = nullptr;
    glBufferStorage(GL_PARAMETER_BUFFER, 4096, nullptr, persistent);
    GLES.glBufferStorageEXT = storage;
    check(stores.size() == store_count && get_buffer_data_size(parameter) == old_size,
          "missing storage entry neither calls backend nor changes metadata");
    check_error(GL_INVALID_OPERATION, "missing storage entry reports frontend failure");
    GLES.glMapBufferRange = nullptr;
    check(glMapBufferRange(GL_ARRAY_BUFFER, 0, 32, persistent) == nullptr, "missing map entry returns null");
    GLES.glMapBufferRange = map_range;
    check_error(GL_INVALID_OPERATION, "missing map entry reports frontend failure");
    const auto flush_count = flushes.size();
    GLES.glFlushMappedBufferRange = nullptr;
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 1, 2);
    GLES.glFlushMappedBufferRange = flush;
    check(flushes.size() == flush_count, "missing flush entry does not dispatch");
    check_error(GL_INVALID_OPERATION, "missing flush entry reports frontend failure");

    for (GLenum persistent_error : {GLenum(GL_CONTEXT_LOST), GLenum(GL_INVALID_OPERATION)}) {
        repeating_error = persistent_error;
        events.clear();
        glBufferStorage(GL_ARRAY_BUFFER, 999, nullptr, persistent);
        repeating_error = GL_NO_ERROR;
        check(events.size() <= 34 && get_buffer_data_size(buffer) == 64,
              "repeating backend error is bounded and does not publish allocation metadata");
        check_error(persistent_error, "application error from unavailable context remains visible");
    }
    std::printf("buffer mapping and frontend errors: %s (%d checks)\n", failures ? "FAIL" : "PASS", checks);
    return failures ? 1 : 0;
}
