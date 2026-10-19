// MobileGlues - gl/shader_state.h
// SPDX-License-Identifier: LGPL-2.1-only
#ifndef MOBILEGLUES_SHADER_STATE_H
#define MOBILEGLUES_SHADER_STATE_H

#include <GL/gl.h>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// GL object names are meaningful only within a share group. The previous
// shader bookkeeping was one process-global "last shader touched" record, which
// broke interleaved shader creation and made glBindFragDataLocation patch
// whichever shader happened to be compiled last. These keys put every record
// in the share group that owns it.
struct MgObjectKey {
    unsigned long long group;
    GLuint name;
    bool operator==(const MgObjectKey& other) const { return group == other.group && name == other.name; }
    bool operator<(const MgObjectKey& other) const {
        return group < other.group || (group == other.group && name < other.name);
    }
};

struct MgObjectKeyHash {
    size_t operator()(const MgObjectKey& key) const {
        const size_t h1 = std::hash<unsigned long long>()(key.group);
        const size_t h2 = std::hash<GLuint>()(key.name);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
    }
};

struct MgShaderObject {
    GLenum type = 0;
    std::string converted_source; // translated ESSL, before any frag-data patch
    bool sampler_buffer_emulated = false;
    bool deleted = false;
    std::set<MgObjectKey> attached_programs;
};

struct MgProgramObject {
    std::vector<GLuint> attached_shaders;
    std::unordered_map<std::string, GLuint> frag_data_locations;
    std::unordered_map<std::string, GLuint> storage_block_bindings;
};

extern std::unordered_map<MgObjectKey, MgShaderObject, MgObjectKeyHash> g_mg_shaders;
extern std::unordered_map<MgObjectKey, MgProgramObject, MgObjectKeyHash> g_mg_programs;

MgObjectKey mg_object_key(GLuint name);
MgShaderObject* mg_find_shader(GLuint name);
MgProgramObject* mg_find_program(GLuint name);

#endif // MOBILEGLUES_SHADER_STATE_H
