// MobileGlues - tests/fsr1_core_test.cpp
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
//
// Host checks for the FSR1 spatial path: output-first sizing, the
// EASU -> intermediate -> RCAS dataflow, one colour format across stages, and
// the state mask the injected pass restores. No GPU, EGL or GLES is involved.
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "gl/FSR1/FSR1Plan.h"
#include "gl/FSR1/FSRShaderSource.h"

using namespace FSR1Plan;

static int g_failures = 0;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void check_plan_for_preset(FSR1_Quality_Preset preset, int width, int height) {
    const Plan plan = MakePlan(preset, width, height);
    CHECK(Valid(plan.output));
    CHECK(plan.output.width == width && plan.output.height == height);
    // The target is the surface. This is the whole output-first rule.
    CHECK(plan.target == plan.output);
    CHECK(plan.intermediate == plan.target);
    // The scene renders inside the output.
    CHECK(plan.render.width > 0 && plan.render.height > 0);
    CHECK(plan.render.width <= width && plan.render.height <= height);
    if (preset != FSR1_Quality_Preset::Disabled) {
        // Every usable preset actually reduces scene work at realistic sizes.
        if (width >= 64 && height >= 64) {
            CHECK(plan.render.width < width || plan.render.height < height);
        }
    }
    CHECK(PlanIsCoherent(plan));
}

static void test_sizing() {
    // The specific pair the old path got wrong: output-first must derive render
    // from the 1920x1080 surface, not multiply it to 2880x1620.
    {
        const Plan plan = MakePlan(FSR1_Quality_Preset::Quality, 1920, 1080);
        CHECK(plan.target.width == 1920 && plan.target.height == 1080);
        CHECK(plan.render.width == 1280 && plan.render.height == 720);
        CHECK(plan.intermediate.width == 1920 && plan.intermediate.height == 1080);
        CHECK(PlanIsCoherent(plan));
    }
    {
        const Plan plan = MakePlan(FSR1_Quality_Preset::Performance, 2400, 1080);
        CHECK(plan.target.width == 2400 && plan.target.height == 1080);
        CHECK(plan.render.width == 1200 && plan.render.height == 540);
    }
    {
        const Plan plan = MakePlan(FSR1_Quality_Preset::UltraQuality, 1920, 1080);
        CHECK(plan.target.width == 1920 && plan.target.height == 1080);
        CHECK(plan.render.width == 1477); // round(1920 / 1.3)
        CHECK(plan.render.height == 831); // round(1080 / 1.3)
    }
    {
        const Plan plan = MakePlan(FSR1_Quality_Preset::Balanced, 1920, 1080);
        CHECK(plan.target.width == 1920 && plan.target.height == 1080);
        CHECK(plan.render.width == 1129); // round(1920 / 1.7)
        CHECK(plan.render.height == 635); // round(1080 / 1.7)
    }
    {
        // Off is never asked to build a target; if a caller does, it must not
        // invent an upscale.
        const Plan plan = MakePlan(FSR1_Quality_Preset::Disabled, 1280, 720);
        CHECK(plan.target == plan.output);
        CHECK(plan.render == plan.output);
        CHECK(PlanIsCoherent(plan));
    }
    {
        // Odd extents stay odd: the presented target must not be silently
        // snapped away from the surface.
        const Plan plan = MakePlan(FSR1_Quality_Preset::Quality, 1919, 1081);
        CHECK(plan.target.width == 1919 && plan.target.height == 1081);
        CHECK(plan.output.width == 1919 && plan.output.height == 1081);
        CHECK(PlanIsCoherent(plan));
    }
    {
        // Tiny surfaces clamp to at least one pixel instead of allocating zero.
        const Plan plan = MakePlan(FSR1_Quality_Preset::Performance, 1, 1);
        CHECK(plan.render.width == 1 && plan.render.height == 1);
        CHECK(PlanIsCoherent(plan));
    }
    {
        // A degenerate surface is not a plan; it must not report success.
        const Plan plan = MakePlan(FSR1_Quality_Preset::Quality, 0, 1080);
        CHECK(!Valid(plan.output));
        CHECK(!PlanIsCoherent(plan));
    }

    const int widths[] = {1, 2, 17, 640, 1279, 1280, 1920, 2560, 2880, 3840};
    const int heights[] = {1, 3, 19, 360, 719, 720, 1080, 1200, 1585, 2160};
    for (FSR1_Quality_Preset preset :
         {FSR1_Quality_Preset::UltraQuality, FSR1_Quality_Preset::Quality, FSR1_Quality_Preset::Balanced,
          FSR1_Quality_Preset::Performance}) {
        for (int width : widths) {
            for (int height : heights) {
                check_plan_for_preset(preset, width, height);
            }
        }
    }
}

static void test_dataflow() {
    // The two passes are ordered and wired through a shared stage: EASU's
    // destination is RCAS's source. A version where RCAS reads the scene texture
    // again fails here.
    CHECK(kPasses[0].pass == Pass::Easu);
    CHECK(kPasses[1].pass == Pass::Rcas);
    CHECK(kPasses[0].input == Stage::SceneRender);
    CHECK(kPasses[0].output == Stage::EasuOutput);
    CHECK(kPasses[1].input == Stage::EasuOutput);
    CHECK(kPasses[1].output == Stage::FinalOutput);
    CHECK(kPasses[0].output == kPasses[1].input);

    const Plan plan = MakePlan(FSR1_Quality_Preset::Quality, 1920, 1080);
    CHECK(ExtentOf(plan, Stage::SceneRender) == plan.render);
    CHECK(ExtentOf(plan, Stage::EasuOutput) == plan.target);
    CHECK(ExtentOf(plan, Stage::FinalOutput) == plan.output);
    // EASU and RCAS run at the same extent so RCAS's 1:1 texelFetch addresses
    // the texel it just wrote.
    CHECK(ExtentOf(plan, Stage::EasuOutput) == ExtentOf(plan, Stage::FinalOutput));

    // One format policy for every stage, including across a resize.
    for (const StageFormat& entry : kStageFormats) {
        CHECK(entry.format == ColorFormat::RGBA8);
    }
    CHECK(FormatOf(Stage::SceneRender) == ColorFormat::RGBA8);
    CHECK(FormatOf(Stage::EasuOutput) == ColorFormat::RGBA8);
    CHECK(FormatOf(Stage::FinalOutput) == ColorFormat::RGBA8);

    const Plan resized = MakePlan(FSR1_Quality_Preset::Quality, 2560, 1440);
    for (const StageFormat& entry : kStageFormats) {
        CHECK(entry.format == FormatOf(entry.stage));
    }
    CHECK(PlanIsCoherent(resized));
    CHECK(FormatOf(Stage::SceneRender) == FormatOf(Stage::FinalOutput));
}

// ---------------------------------------------------------------------------
// Executable dataflow model.
//
// The two shader stages are replaced by CPU stand-ins with the same addressing
// contract: EASU writes a target-extent image from a render-extent one, and RCAS
// reads its single input at the coordinates gl_FragCoord would give it in the
// output. The wiring comes from kPasses, so a change that pointed RCAS back at
// the scene texture changes the observable result and the extents here.
// ---------------------------------------------------------------------------

struct Image {
    int width = 0;
    int height = 0;
    std::vector<int> pixels;

    Image() = default;
    Image(int w, int h, int value) : width(w), height(h), pixels(static_cast<size_t>(w) * h, value) {}

    int& at(int x, int y) { return pixels[static_cast<size_t>(y) * width + x]; }
    int at(int x, int y) const { return pixels[static_cast<size_t>(y) * width + x]; }

    int clamped(int x, int y) const {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x >= width) x = width - 1;
        if (y >= height) y = height - 1;
        return at(x, y);
    }

    bool operator==(const Image& other) const {
        return width == other.width && height == other.height && pixels == other.pixels;
    }
};

// A single-pixel vertical edge, deliberately not symmetric: the left side is
// dark and the right side bright. Any rerouting of the sampling boundary moves
// values in a way a flat image would hide.
static Image MakeEdgeScene(int width, int height) {
    Image scene(width, height, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) scene.at(x, y) = (x < width / 2) ? 16 : 240;
    }
    return scene;
}

// Nearest-neighbour spatial upscale. It is not EASU, but it is a spatial
// upscale: the source edge becomes a target-extent edge at a different texel
// index, which is all this test needs to tell the intermediate from the source.
static Image EasuStandIn(const Image& scene, int outWidth, int outHeight) {
    Image out(outWidth, outHeight, 0);
    for (int y = 0; y < outHeight; ++y) {
        const int sy = static_cast<int>((static_cast<long long>(y) * scene.height) / outHeight);
        for (int x = 0; x < outWidth; ++x) {
            const int sx = static_cast<int>((static_cast<long long>(x) * scene.width) / outWidth);
            out.at(x, y) = scene.clamped(sx, sy);
        }
    }
    return out;
}

// Cross-shaped sharpen. The output extent is the extent RCAS writes; the input
// is addressed with those same coordinates, exactly like texelFetch(gl_FragCoord)
// after clamping at the border.
static Image RcasStandIn(const Image& sampled, int outWidth, int outHeight) {
    Image out(outWidth, outHeight, 0);
    for (int y = 0; y < outHeight; ++y) {
        for (int x = 0; x < outWidth; ++x) {
            const int center = sampled.clamped(x, y);
            const int sum = sampled.clamped(x - 1, y) + sampled.clamped(x + 1, y) + sampled.clamped(x, y - 1) +
                            sampled.clamped(x, y + 1);
            int value = 5 * center - sum;
            if (value < 0) value = 0;
            if (value > 255) value = 255;
            out.at(x, y) = value;
        }
    }
    return out;
}

// Runs the pass table. The input of each pass is the buffer its PassIo names.
static Image RunPipeline(const Plan& plan, const Image& scene) {
    Image buffers[static_cast<int>(Stage::Count)];
    buffers[static_cast<int>(Stage::SceneRender)] = scene;

    for (const PassIo& io : kPasses) {
        const Image& input = buffers[static_cast<int>(io.input)];
        Image output;
        if (io.pass == Pass::Easu) {
            output = EasuStandIn(input, ExtentOf(plan, io.output).width, ExtentOf(plan, io.output).height);
        } else {
            // RCAS is 1:1: its output extent is the input extent it was given.
            output = RcasStandIn(input, input.width, input.height);
        }
        CHECK(output.width == ExtentOf(plan, io.output).width);
        CHECK(output.height == ExtentOf(plan, io.output).height);
        buffers[static_cast<int>(io.output)] = output;
    }
    return buffers[static_cast<int>(Stage::FinalOutput)];
}

static void test_synthetic_edge_dataflow() {
    const Plan plan = MakePlan(FSR1_Quality_Preset::Performance, 16, 8);
    CHECK(plan.render.width == 8 && plan.render.height == 4);
    CHECK(plan.target.width == 16 && plan.target.height == 8);

    const Image scene = MakeEdgeScene(plan.render.width, plan.render.height);
    const Image intermediate = EasuStandIn(scene, plan.intermediate.width, plan.intermediate.height);
    const Image expected = RcasStandIn(intermediate, plan.intermediate.width, plan.intermediate.height);

    const Image actual = RunPipeline(plan, scene);
    // The final image came from the EASU intermediate at target extent...
    CHECK(actual.width == plan.target.width);
    CHECK(actual.height == plan.target.height);
    CHECK(actual == expected);

    // ...and the intermediate is a real, distinct EASU result: the 1px source
    // edge is still present, at the target extent, not copied through at the
    // source's texel numbering.
    CHECK(intermediate.at(plan.target.width / 2 - 1, 4) == 16);
    CHECK(intermediate.at(plan.target.width / 2, 4) == 240);

    // What the discarded-EASU defect did: run RCAS directly on the source while
    // keeping gl_FragCoord at target extent. The source and the intermediate do
    // not agree at the boundary, so this is observably a different image.
    const Image buggy = RcasStandIn(scene, plan.target.width, plan.target.height);
    CHECK(!(buggy == actual));

    int differing = 0;
    for (int y = 0; y < plan.target.height; ++y) {
        for (int x = 0; x < plan.target.width; ++x) {
            if (buggy.at(x, y) != actual.at(x, y)) ++differing;
        }
    }
    CHECK(differing > 0);
}

static bool source_contains(const char* haystack, const char* needle) {
    return std::strstr(haystack, needle) != nullptr;
}

static int source_count(const char* haystack, const char* needle) {
    int count = 0;
    const size_t length = std::strlen(needle);
    for (const char* at = std::strstr(haystack, needle); at != nullptr; at = std::strstr(at + length, needle)) ++count;
    return count;
}

static void test_shader_source() {
    // The shader is data, but the old defects were in it. These assertions pin
    // the corrected interface without needing a GLSL compiler on the host.
    CHECK(source_contains(FSR_FSSource, "uniform int uPass;"));
    CHECK(source_contains(FSR_FSSource, "uniform vec2 uInputSize;"));
    CHECK(source_contains(FSR_FSSource, "uniform vec2 uOutputSize;"));

    // EASU derives the output extent from the pass uniform, not from the input
    // texture size (the discarded-output bug).
    CHECK(source_contains(FSR_FSSource, "outputSizeInPixelsX = uOutputSize.x"));
    CHECK(source_contains(FSR_FSSource, "inputSizeInPixelsX = uInputSize.x"));
    CHECK(!source_contains(FSR_FSSource, "float(texSize.x)"));
    CHECK(!source_contains(FSR_FSSource, "uniform vec2 uViewportSize"));

    // Both passes address the framebuffer they write.
    CHECK(source_contains(FSR_FSSource, "AU2 ip = AU2(gl_FragCoord.xy);"));
    CHECK(source_contains(FSR_FSSource, "if (uPass == 0)"));

    // RCAS reads the texture bound for its pass, clamped at the border, and its
    // result reaches the output.
    CHECK(source_contains(FSR_FSSource, "clamp(ASU2(p), ASU2(0, 0), last)"));
    CHECK(source_contains(FSR_FSSource, "oFragColor = vec4(sharpenedColor, 1.0);"));

    // Both passes sample the single uInputTex binding: EASU through the
    // textureGather taps (which honour the texture's filtering/wrap and the
    // sampler object on unit 0), RCAS through texelFetch. There is exactly one
    // sampler uniform, so the pass switch is a texture rebind on unit 0 -- which
    // is why the renderer binds sampler 0 there and restores the application's.
    CHECK(source_contains(FSR_FSSource, "textureGather(uInputTex,"));
    CHECK(source_contains(FSR_FSSource, "texelFetch(uInputTex,"));
    CHECK(source_count(FSR_FSSource, "uniform sampler") == 1);

    // The EASU result is not thrown away: it is the fragment result of pass 0.
    const char* easu = std::strstr(FSR_FSSource, "oFragColor = vec4(color, 1.0);");
    CHECK(easu != nullptr);
    const char* rcas = std::strstr(FSR_FSSource, "oFragColor = vec4(sharpenedColor, 1.0);");
    CHECK(rcas != nullptr);
    if (easu != nullptr && rcas != nullptr) CHECK(easu < rcas);

    // Ordering within main: EASU call precedes RCAS call.
    const char* easu_call = std::strstr(FSR_FSSource, "FsrEasuF(");
    const char* rcas_call = std::strstr(FSR_FSSource, "FsrRcasF(");
    CHECK(easu_call != nullptr && rcas_call != nullptr);
    if (easu_call != nullptr && rcas_call != nullptr) CHECK(easu_call < rcas_call);
}

static void test_state_mask() {
    // Every piece of state the audited pass was missing is in the mask the
    // renderer uses. A future body that changes one without restoring it has to
    // remove it from this list as well, which is visible in review and here.
    const uint32_t required[] = {
        STATE_PROGRAM,       STATE_VAO,         STATE_ARRAY_BUFFER,   STATE_TEXTURE,
        STATE_FRAMEBUFFER,   STATE_VIEWPORT,    STATE_SCISSOR,        STATE_BLEND_ENABLE,
        STATE_BLEND_FUNCS,   STATE_BLEND_EQUATION, STATE_DEPTH,       STATE_CULL,
        STATE_COLOR_MASK,    STATE_CLEAR_COLOR, STATE_STENCIL,        STATE_RASTERIZER_DISCARD,
        STATE_SAMPLER,
    };
    for (uint32_t bit : required) {
        CHECK((kApplyFSRState & bit) != 0);
    }
    CHECK((kApplyFSRState & STATE_TEXTURE) != 0); // active unit + unit 0 binding
    CHECK((kApplyFSRState & STATE_SAMPLER) != 0); // unit 0 sampler object override
    // Rasterizer discard and stencil are written by ApplyFSR; a mask that lost
    // either would leave the application's rasterizer discarding its next draw
    // or stencil test disabled.
    CHECK((kApplyFSRState & STATE_STENCIL) != 0);
    CHECK((kApplyFSRState & STATE_RASTERIZER_DISCARD) != 0);

    // Resource creation binds and sets a viewport, and a rebuild does the same.
    // The viewport must be in both sets or a resize/init leaves GL_VIEWPORT
    // describing the FSR1 target instead of the application's scene.
    CHECK((kResourceCreationState & STATE_VIEWPORT) != 0);
    CHECK((kRecreateState & STATE_VIEWPORT) != 0);
    CHECK((kRecreateState & STATE_FRAMEBUFFER) != 0);
    CHECK((kRecreateState & STATE_TEXTURE) != 0);
    CHECK((kRecreateState & STATE_RENDERBUFFER) != 0);
}

// The source-level invariants the masks cannot express on their own, checked as
// text so the host test fails if a future edit reopens one of them. Paths are
// resolved from this file, so the test works from any working directory.
static std::string read_source(const char* relative_from_gl) {
    std::string path = __FILE__;
    const size_t slash = path.find_last_of("/\\");
    path = (slash == std::string::npos ? std::string(".") : path.substr(0, slash)) + "/../gl/";
    path += relative_from_gl;
    std::FILE* file = std::fopen(path.c_str(), "rb");
    CHECK(file != nullptr);
    if (file == nullptr) return {};
    std::string text;
    char buffer[4096];
    size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) text.append(buffer, got);
    std::fclose(file);
    return text;
}

// The render-target priming transitions, driven without GL. These are the rules
// ApplyFSR and gl/framebuffer.cpp implement; a change that reintroduces the
// bind-once freeze or presents an empty post-rebuild texture fails here.
static void test_priming_state() {
    // Never bound: wait, whether or not a rebuild left a hold behind.
    CHECK(DecidePresent(false, false) == PresentDecision::Wait);
    CHECK(DecidePresent(false, true) == PresentDecision::Wait);

    // One redirect bind primes the target. Every following frame upscales, with
    // or without another bind -- this is the bind-once multi-frame case.
    bool primed = true;
    CHECK(DecidePresent(primed, false) == PresentDecision::Upscale);
    CHECK(DecidePresent(primed, false) == PresentDecision::Upscale);
    CHECK(DecidePresent(primed, false) == PresentDecision::Upscale);

    // A rebuild keeps priming and holds exactly one present.
    bool skip_present = SkipPresentAfterRebuild(primed);
    CHECK(skip_present);
    CHECK(DecidePresent(primed, skip_present) == PresentDecision::SkipOne);
    skip_present = false; // the hold is consumed, or a bind dropped it
    CHECK(DecidePresent(primed, skip_present) == PresentDecision::Upscale);
    CHECK(DecidePresent(primed, skip_present) == PresentDecision::Upscale);

    // A rebuild of a target that was never primed changes nothing.
    CHECK(!SkipPresentAfterRebuild(false));
    CHECK(DecidePresent(false, false) == PresentDecision::Wait);

    // The path being disabled abandons both facts.
    primed = false;
    skip_present = false;
    CHECK(DecidePresent(primed, skip_present) == PresentDecision::Wait);
}

static void test_source_invariants() {
    const std::string fsr = read_source("FSR1/FSR1.cpp");
    const std::string fb = read_source("framebuffer.cpp");
    CHECK(!fsr.empty());
    CHECK(!fb.empty());

    // The guard queries unit 0's sampler and the pass binds sampler 0, so an
    // application sampler object cannot override the textures' filtering.
    CHECK(source_contains(fsr.c_str(), "GL_SAMPLER_BINDING"));
    CHECK(source_contains(fsr.c_str(), "GLES.glBindSampler(0, 0);"));

    // All three masks are the ones the bodies actually use.
    CHECK(source_contains(fsr.c_str(), "GLStateGuard state(kApplyFSRState);"));
    CHECK(source_contains(fsr.c_str(), "GLStateGuard state(kResourceCreationState);"));
    CHECK(source_contains(fsr.c_str(), "GLStateGuard state(kRecreateState);"));

    // Priming is persistent: ApplyFSR decides through the pure helper and never
    // clears g_dirty after presenting, so a bind-once application keeps getting
    // upscaled. A rebuild preserves priming and holds exactly one present; a
    // redirect bind after the rebuild drops that hold; build/failure clear both.
    CHECK(source_contains(fsr.c_str(), "DecidePresent(FSR1_Context::g_dirty, FSR1_Context::g_skipNextPresent)"));
    CHECK(source_contains(fsr.c_str(), "SkipPresentAfterRebuild(FSR1_Context::g_dirty)"));
    CHECK(source_contains(fb.c_str(), "FSR1_Context::g_skipNextPresent = false;"));
    const size_t apply_at = fsr.find("void ApplyFSR()");
    const size_t check_at = fsr.find("void CheckResolutionChange(");
    if (apply_at != std::string::npos && check_at != std::string::npos && check_at > apply_at) {
        const std::string apply_body = fsr.substr(apply_at, check_at - apply_at);
        // Clearing the persistent flag here is the bind-once freeze.
        CHECK(!source_contains(apply_body.c_str(), "g_dirty = false"));
        CHECK(source_contains(apply_body.c_str(), "DecidePresent("));
        CHECK(source_contains(apply_body.c_str(), "g_skipNextPresent = false;"));
    } else {
        CHECK(false);
    }
    // The rebuild arm must not clear the primed flag either (only the failure
    // arm does, and that arm is inside RecreateFSRTargets before the success
    // code). Count the clears: init failure, init success/fresh build, recreate
    // failure, resize failure -- but not ApplyFSR.
    CHECK(source_count(fsr.c_str(), "FSR1_Context::g_dirty = false;") >= 4);

    // The dual draw-FBO tracker is updated through the republish API, never by
    // touching one of the two fields directly. If this call disappears, the
    // resize path leaves gl/framebuffer.cpp naming a deleted framebuffer.
    CHECK(source_contains(fsr.c_str(), "mg_framebuffer_republish_draw_binding("));
    CHECK(!source_contains(fsr.c_str(), "set_gl_state_current_draw_fbo("));
    CHECK(source_contains(fb.c_str(), "void mg_framebuffer_republish_draw_binding(GLuint framebuffer)"));
    // ...and that function writes both trackers.
    const size_t fn = fb.find("void mg_framebuffer_republish_draw_binding");
    if (fn != std::string::npos) {
        const std::string body = fb.substr(fn, fb.find('}', fn) - fn);
        CHECK(source_contains(body.c_str(), "set_gl_state_current_draw_fbo(framebuffer)"));
        CHECK(source_contains(body.c_str(), "current_draw_fbo = framebuffer"));
    }

    // A resize failure deletes the program and the quad as well as the targets:
    // init failure plus the resize-failure arm.
    size_t destroys = 0;
    for (size_t at = fsr.find("DestroyProgramAndQuad();"); at != std::string::npos;
         at = fsr.find("DestroyProgramAndQuad();", at + 1)) {
        ++destroys;
    }
    CHECK(destroys >= 2);
}

int main() {
    test_sizing();
    test_dataflow();
    test_synthetic_edge_dataflow();
    test_shader_source();
    test_state_mask();
    test_priming_state();
    test_source_invariants();

    if (g_failures != 0) {
        std::printf("fsr1_core_test: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("fsr1_core_test: all checks passed\n");
    return 0;
}
