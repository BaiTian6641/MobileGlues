// SPDX-License-Identifier: LGPL-2.1-only
#include "../gl/image_uniform_write.h"
#include <cassert>
#include <cstdio>

int main() {
    MgProgramObject state;
    state.image_uniform_units = {{3, 0}, {4, 1}, {9, 2}};
    state.image_uniform_array_remaining = {{3, 1}, {4, 1}, {9, 0}};
    state.image_uniform_names = {{3, "images"}, {4, "images"}, {9, "scalar"}};
    mg_finalize_image_uniform_arrays(state);
    assert(state.image_uniform_array_remaining.at(3) == 2);
    assert(state.image_uniform_array_remaining.at(4) == 1);
    assert(state.image_uniform_array_remaining.at(9) == 0);
    GLenum error = GL_NO_ERROR;
    const GLint units[] = {5, 6, -1};
    assert(mg_update_image_uniforms(state, 3, 2, units, 8, error));
    assert(error == GL_NO_ERROR && state.image_unit_remap.at(5) == 0 && state.image_unit_remap.at(6) == 1);
    const auto unchanged = state.image_unit_remap;
    const GLint invalid[] = {7, 8};
    assert(mg_update_image_uniforms(state, 3, 2, invalid, 8, error));
    assert(error == GL_INVALID_VALUE && state.image_unit_remap == unchanged);
    const GLint negative = -1;
    assert(mg_update_image_uniforms(state, 9, 1, &negative, 8, error));
    assert(error == GL_INVALID_VALUE && state.image_unit_remap == unchanged);
    assert(mg_update_image_uniforms(state, 9, 2, units, 8, error));
    assert(error == GL_INVALID_OPERATION && state.image_unit_remap == unchanged);
    assert(mg_update_image_uniforms(state, 3, -1, nullptr, 8, error));
    assert(error == GL_INVALID_VALUE && state.image_unit_remap == unchanged);
    assert(mg_update_image_uniforms(state, 3, 0, nullptr, 8, error));
    assert(error == GL_NO_ERROR && state.image_unit_remap == unchanged);
    // Excess array values, including invalid trailing values, are ignored.
    assert(mg_update_image_uniforms(state, 3, 3, units, 8, error));
    assert(error == GL_NO_ERROR && state.image_unit_remap == unchanged);
    assert(mg_update_image_uniforms(state, 4, 2, units, 8, error));
    assert(error == GL_NO_ERROR && state.image_unit_remap.at(5) == 1);
    assert(!mg_update_image_uniforms(state, -1, 1, units, 8, error));
    assert(!mg_update_image_uniforms(state, 8, 1, units, 8, error));
    puts("Image uniform write validation: passed");
}
