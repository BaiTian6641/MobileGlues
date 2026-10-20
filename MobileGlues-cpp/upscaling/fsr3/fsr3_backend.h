// MobileGlues - upscaling/fsr3/fsr3_backend.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#pragma once

#include "../mg_fsr_abi.h"

#include <GLES3/gl32.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mg_fsr3 {

// The backend owns every FSR3 intermediate and history texture. The adapter
// supplies only the current frame's color/depth/motion/masks and receives a
// backend-owned output. Handles are frontend GL names in the active context.
struct Inputs {
    uint64_t frame_id = 0;
    uint64_t resource_generation = 0;
    mg_fsr_resource_t color{};
    mg_fsr_resource_t depth{};
    mg_fsr_resource_t motion{};
    mg_fsr_resource_t exposure{};
    mg_fsr_resource_t reactive{};
    mg_fsr_resource_t transparency{};
    mg_fsr_resource_t output{}; // handle 0 -> backend-owned RGBA8 output

    uint32_t render_width = 0;
    uint32_t render_height = 0;
    uint32_t output_width = 0;
    uint32_t output_height = 0;

    bool hdr_color = false;
    bool alpha_premultiplied = false;
    float pre_exposure = 1.0f;

    bool depth_reversed = false;
    bool depth_infinite_far = false;
    bool depth_valid = false;
    float camera_near = 0.1f;
    float camera_far = 1000.0f;
    float camera_fov_y_radians = 1.0471975511965976f;

    float jitter_x = 0.0f;
    float jitter_y = 0.0f;
    float motion_scale_x = 1.0f;
    float motion_scale_y = 1.0f;
    bool motion_valid = false;
    bool motion_includes_jitter = false;
    uint32_t motion_units = 0; // 0 = render pixels, 1 = normalized UV

    float frame_delta_seconds = 1.0f / 60.0f;
    bool reset_history = false;
    uint32_t reset_reason = 0;
};

class Backend {
public:
    Backend();
    ~Backend();

    Backend(const Backend&) = delete;
    Backend& operator=(const Backend&) = delete;

    // Allocates/compiles at the requested extents. Safe to call again after a
    // resize; any failure destroys the partial set and makes the session
    // unavailable. Requires a current GLES context.
    bool initialize(uint32_t render_width, uint32_t render_height, uint32_t output_width, uint32_t output_height);

    bool resize(uint32_t render_width, uint32_t render_height, uint32_t output_width, uint32_t output_height);

    // Executes one FSR3.1 frame. The caller has already produced color/depth/
    // motion. No CPU readback or glFinish is used.
    bool dispatch(const Inputs& inputs);

    void destroy();
    // Drop bookkeeping without issuing GL calls when the owning context is gone.
    void abandon();

    bool ready() const { return initialized_; }
    const std::string& error() const { return error_; }
    GLuint output_texture() const { return output_.texture; }
    uint32_t output_width() const { return output_width_; }
    uint32_t output_height() const { return output_height_; }
    uint32_t render_width() const { return render_width_; }
    uint32_t render_height() const { return render_height_; }

private:
    struct Texture {
        GLuint texture = 0;
        GLenum internal = 0;
        int width = 0;
        int height = 0;
        int levels = 1;
    };
    struct Program;
    struct PassDesc {
        const char* source = nullptr;
        const char* name = nullptr;
        int local_x = 8;
        int local_y = 8;
        bool spd = false;
        bool rcas = false;
    };

    bool create_textures();
    bool create_programs();
    bool create_constants();
    bool upload_defaults();
    void destroy_textures();
    void destroy_programs();
    void set_error(std::string message);

    bool clear_texture(const Texture& tex, int level, GLenum format, GLenum type, const void* value);
    bool clear_float(const Texture& tex, int level, float r, float g = 0.0f, float b = 0.0f, float a = 0.0f);
    bool clear_uint(const Texture& tex, int level, uint32_t r);

    bool run_pass(size_t index, uint32_t width, uint32_t height);
    bool bind_samplers(Program& program);
    bool bind_images(Program& program);
    bool bind_ubo(Program& program);
    bool resolve_image(const std::string& name, Texture*& texture, int& level, GLenum& access, GLenum& format);
    bool resolve_sampler(const std::string& base, Texture*& texture, int& level);

    bool initialized_ = false;
    bool failed_ = false;
    std::string error_;
    uint32_t render_width_ = 0, render_height_ = 0;
    uint32_t output_width_ = 0, output_height_ = 0;

    Texture input_exposure_{};
    Texture input_reactive_{};
    Texture input_transparency_{};
    Texture output_{};

    GLuint frame_color_ = 0;
    GLuint frame_depth_ = 0;
    GLuint frame_motion_ = 0;
    GLuint frame_exposure_ = 0;
    GLuint frame_reactive_ = 0;
    GLuint frame_transparency_ = 0;
    GLuint frame_output_ = 0;
    bool output_owned_ = false;

    Texture acc_[2]{};
    Texture luma_[2]{};
    Texture intermediate_{};
    Texture lh_[2]{};
    Texture iuc_[2]{};
    Texture new_locks_{};
    Texture dilated_reactive_{};
    Texture shading_change_{};
    Texture spd_mips_{};
    Texture farthest_mip1_{};
    Texture spd_atomic_{};
    Texture frame_info_{};
    Texture dilated_motion_{};
    Texture dilated_depth_{};
    Texture recon_depth_{};
    Texture lanczos_lut_{};

    GLuint clear_fbo_ = 0;
    GLuint ubo_fsr_ = 0;
    GLuint ubo_spd_ = 0;
    GLuint ubo_rcas_ = 0;

    struct ImageBinding {
        GLuint unit = 0;
        std::string name;
        GLenum access = GL_READ_WRITE;
        GLenum format = 0;
    };
    struct Program {
        GLuint program = 0;
        const PassDesc* desc = nullptr;
        std::vector<std::pair<GLuint, std::string>> ubos;
        struct SamplerBinding {
            std::string base;
            GLint location = -1;
            GLenum type = GL_SAMPLER_2D;
        };
        std::vector<SamplerBinding> samplers;
        std::vector<ImageBinding> images;
        bool compiled = false;
    };
    std::vector<Program> programs_;

    uint64_t frame_index_ = 0;
    uint32_t previous_render_width_ = 0, previous_render_height_ = 0;
    uint32_t previous_output_width_ = 0, previous_output_height_ = 0;
    float previous_jitter_x_ = 0.0f, previous_jitter_y_ = 0.0f;
    float previous_pre_exposure_ = 1.0f;
    bool first_frame_ = true;
    bool last_reset_ = true;
    bool last_motion_includes_jitter_ = false;

};

} // namespace mg_fsr3
