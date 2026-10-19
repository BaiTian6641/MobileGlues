// MobileGlues - gl/shader_state.cpp
// SPDX-License-Identifier: LGPL-2.1-only
#include "shader_state.h"

#include "../egl/context.h"

std::unordered_map<MgObjectKey, MgShaderObject, MgObjectKeyHash> g_mg_shaders;
std::unordered_map<MgObjectKey, MgProgramObject, MgObjectKeyHash> g_mg_programs;

MgObjectKey mg_object_key(GLuint name) {
    return MgObjectKey{mg_current_share_group_id(), name};
}

MgShaderObject* mg_find_shader(GLuint name) {
    auto it = g_mg_shaders.find(mg_object_key(name));
    return it == g_mg_shaders.end() ? nullptr : &it->second;
}

MgProgramObject* mg_find_program(GLuint name) {
    auto it = g_mg_programs.find(mg_object_key(name));
    return it == g_mg_programs.end() ? nullptr : &it->second;
}
