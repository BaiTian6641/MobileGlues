// MobileGlues - gl/FSR1/FSR1Plan.h
// Copyright (c) 2025-2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v2.1:
//   https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt
// SPDX-License-Identifier: LGPL-2.1-only
// End of Source File Header
#pragma once

// The size, format and pass-order rules of the FSR1 spatial path, in a form the
// host test suite can exercise without a GPU.
//
// Output-first ownership: the EGL surface is the authority. The target equals
// the surface exactly, and the render extent is derived from the target with the
// preset's scale. The old path did the reverse -- it took the surface or the
// application's viewport as the render extent and multiplied it to invent a
// larger target, which upscaled past the surface and then downscaled in the
// blit. Nothing here allocates or talks to GL.
#include <cstdint>

#include "../../config/settings.h"

namespace FSR1Plan {

enum class Stage : uint8_t {
    SceneRender = 0, // application colour, sampled by EASU
    EasuOutput,      // EASU destination, sampled by RCAS
    FinalOutput,     // RCAS destination, blitted to the surface
    Count
};

// One intentional colour policy for every FSR1-owned texture. RGBA8 matches the
// usual EGL surface, is renderable and filterable on every GLES 3 backend, and
// keeps resize from silently changing precision and bandwidth the way the old
// RGBA8-init/RGBA32F-recreate pair did. All three stages use it.
enum class ColorFormat : uint8_t { RGBA8 = 0 };

enum class Pass : uint8_t { Easu = 0, Rcas = 1 };

struct Extent {
    int width = 0;
    int height = 0;
};

constexpr bool operator==(const Extent& a, const Extent& b) {
    return a.width == b.width && a.height == b.height;
}
constexpr bool operator!=(const Extent& a, const Extent& b) {
    return !(a == b);
}
constexpr bool Valid(const Extent& e) {
    return e.width > 0 && e.height > 0;
}

struct Plan {
    Extent output;       // EGL surface size, the authority
    Extent target;       // final RCAS destination and blit source; == output
    Extent intermediate; // EASU destination and RCAS source; == target
    Extent render;       // application scene texture; output / scale
};

// The amount the target is larger than the render extent. Rational so host
// tests and the renderer cannot disagree about rounding.
struct Scale {
    int numerator = 1;   // target = render * numerator / denominator
    int denominator = 1;
};

constexpr Scale ScaleFor(FSR1_Quality_Preset preset) {
    switch (preset) {
    case FSR1_Quality_Preset::UltraQuality:
        return {13, 10}; // 1.3x
    case FSR1_Quality_Preset::Quality:
        return {3, 2}; // 1.5x
    case FSR1_Quality_Preset::Balanced:
        return {17, 10}; // 1.7x
    case FSR1_Quality_Preset::Performance:
        return {2, 1}; // 2.0x
    case FSR1_Quality_Preset::Disabled:
    case FSR1_Quality_Preset::MaxValue:
    default:
        // Off never reaches the pass. 1:1 is the honest answer for a value that
        // is not a usable preset: do not invent a silent upscale.
        return {1, 1};
    }
}

// Round output / scale to the nearest integer pixel and clamp into [1, output].
// Even alignment is deliberately not forced: the target is the surface and an
// odd surface must stay odd rather than change the presented extent.
inline int DeriveRenderExtent(int outputExtent, Scale scale) {
    if (outputExtent <= 0 || scale.numerator <= 0 || scale.denominator <= 0) return 0;
    const long long rounded =
        (static_cast<long long>(outputExtent) * scale.denominator + scale.numerator / 2) / scale.numerator;
    if (rounded < 1) return 1;
    if (rounded > outputExtent) return outputExtent;
    return static_cast<int>(rounded);
}

inline Plan MakePlan(FSR1_Quality_Preset preset, int outputWidth, int outputHeight) {
    Plan plan;
    if (outputWidth <= 0 || outputHeight <= 0) return plan;

    plan.output = {outputWidth, outputHeight};
    plan.target = plan.output;
    plan.intermediate = plan.target;
    const Scale scale = ScaleFor(preset);
    plan.render = {DeriveRenderExtent(outputWidth, scale), DeriveRenderExtent(outputHeight, scale)};
    return plan;
}

// The two passes are separate render-target changes; this table is the
// dataflow contract the renderer and the tests share.
struct PassIo {
    Pass pass;
    Stage input;
    Stage output;
};

constexpr PassIo kPasses[] = {
    {Pass::Easu, Stage::SceneRender, Stage::EasuOutput},
    {Pass::Rcas, Stage::EasuOutput, Stage::FinalOutput},
};

constexpr const char* StageName(Stage stage) {
    switch (stage) {
    case Stage::SceneRender:
        return "scene-render";
    case Stage::EasuOutput:
        return "easu-output";
    case Stage::FinalOutput:
        return "final-output";
    default:
        return "unknown";
    }
}

constexpr const char* PassName(Pass pass) {
    return pass == Pass::Easu ? "easu" : "rcas";
}

struct StageFormat {
    Stage stage;
    ColorFormat format;
};

constexpr StageFormat kStageFormats[] = {
    {Stage::SceneRender, ColorFormat::RGBA8},
    {Stage::EasuOutput, ColorFormat::RGBA8},
    {Stage::FinalOutput, ColorFormat::RGBA8},
};

constexpr ColorFormat FormatOf(Stage stage) {
    for (const StageFormat& entry : kStageFormats) {
        if (entry.stage == stage) return entry.format;
    }
    return ColorFormat::RGBA8;
}

constexpr Extent ExtentOf(const Plan& plan, Stage stage) {
    switch (stage) {
    case Stage::SceneRender:
        return plan.render;
    case Stage::EasuOutput:
        return plan.intermediate;
    case Stage::FinalOutput:
        return plan.target;
    default:
        return {};
    }
}

// The invariants S1 needs from the sizing and dataflow side. The test suite
// calls this on a grid of extents and presets; the renderer links the same
// table, so a divergence is a source change, not a silent runtime difference.
inline bool PlanIsCoherent(const Plan& plan) {
    if (!Valid(plan.output) || !Valid(plan.target) || !Valid(plan.intermediate) || !Valid(plan.render)) return false;
    if (plan.target != plan.output) return false;
    if (plan.intermediate != plan.target) return false;
    if (plan.render.width > plan.output.width || plan.render.height > plan.output.height) return false;
    if (kPasses[0].pass != Pass::Easu || kPasses[1].pass != Pass::Rcas) return false;
    if (kPasses[0].output != kPasses[1].input) return false; // RCAS consumes EASU output
    if (kPasses[0].input != Stage::SceneRender) return false;
    if (kPasses[1].output != Stage::FinalOutput) return false;
    for (const PassIo& io : kPasses) {
        if (FormatOf(io.input) != FormatOf(io.output)) return false;
        if (!Valid(ExtentOf(plan, io.input)) || !Valid(ExtentOf(plan, io.output))) return false;
    }
    if (FormatOf(Stage::SceneRender) != ColorFormat::RGBA8) return false;
    if (FormatOf(Stage::EasuOutput) != ColorFormat::RGBA8) return false;
    if (FormatOf(Stage::FinalOutput) != ColorFormat::RGBA8) return false;
    return true;
}

// Bits of GL state the injected path saves and restores. Kept pure so the test
// suite can assert the mask still covers everything the pass depends on.
enum StateBit : uint32_t {
    STATE_PROGRAM = 1u << 0,
    STATE_VAO = 1u << 1,
    STATE_ARRAY_BUFFER = 1u << 2,
    STATE_TEXTURE = 1u << 3, // active unit + unit 0's GL_TEXTURE_2D binding
    STATE_FRAMEBUFFER = 1u << 4,
    STATE_RENDERBUFFER = 1u << 5,
    STATE_VIEWPORT = 1u << 6,
    STATE_SCISSOR = 1u << 7,
    STATE_BLEND_ENABLE = 1u << 8,
    STATE_BLEND_FUNCS = 1u << 9,
    STATE_BLEND_EQUATION = 1u << 10,
    STATE_DEPTH = 1u << 11, // test enable, write mask and function
    STATE_CULL = 1u << 12,
    STATE_COLOR_MASK = 1u << 13,
    STATE_CLEAR_COLOR = 1u << 14,
    STATE_STENCIL = 1u << 15,
    STATE_RASTERIZER_DISCARD = 1u << 16,
    // Unit 0's sampler object. A sampler bound there overrides the texture's own
    // min/mag filter, wrap modes and compare mode, so EASU would sample through
    // the application's sampler instead of the texture parameters the FSR1
    // textures were created with. The guard reads GL_SAMPLER_BINDING for unit 0,
    // binds sampler 0 for the internal passes and puts the application's back.
    STATE_SAMPLER = 1u << 17,
};

// Everything ApplyFSR touches or relies on. A body that grows a state change
// without growing this mask is a bug the host test catches.
constexpr uint32_t kApplyFSRState =
    STATE_PROGRAM | STATE_VAO | STATE_ARRAY_BUFFER | STATE_TEXTURE | STATE_SAMPLER | STATE_FRAMEBUFFER |
    STATE_VIEWPORT | STATE_SCISSOR | STATE_BLEND_ENABLE | STATE_BLEND_FUNCS | STATE_BLEND_EQUATION | STATE_DEPTH |
    STATE_CULL | STATE_COLOR_MASK | STATE_CLEAR_COLOR | STATE_STENCIL | STATE_RASTERIZER_DISCARD;

// InitFSRResources creates its objects under this mask. It binds textures,
// framebuffers and a renderbuffer, and it may set a viewport while building --
// the viewport is in the set so a build can never leave GL_VIEWPORT describing
// the FSR1 target instead of the application's scene. Samplers are not touched
// here: no draw happens during creation.
constexpr uint32_t kResourceCreationState =
    STATE_PROGRAM | STATE_TEXTURE | STATE_FRAMEBUFFER | STATE_RENDERBUFFER | STATE_VAO | STATE_ARRAY_BUFFER |
    STATE_VIEWPORT;

// RecreateFSRTargets deletes and rebuilds the three targets. Same reasoning for
// the viewport: the rebuild must not hand the application a viewport of the
// render extent after it was only asked to resize a surface.
constexpr uint32_t kRecreateState = STATE_TEXTURE | STATE_FRAMEBUFFER | STATE_RENDERBUFFER | STATE_VIEWPORT;

// Priming contract for the render target, pure so the host suite can drive the
// state transitions without GL.
//
// "Primed" is persistent: once the application has redirected a draw bind of
// framebuffer 0 to the render target, it keeps drawing there every frame even
// when it does not bind again, so ApplyFSR upscales on every present until the
// object set is built unusably or the path is disabled.
//
// A rebuild replaces the object names with fresh storage. Priming survives it:
// the driver's draw binding is restored to the replacement, so a bind-once
// application draws a complete frame into it before the next present and an
// application that binds again does so before drawing. There is no "was it
// drawn" signal to consult, and holding a present on "no bind" would discard a
// valid frame, so the next presented frame is upscaled whenever the path was
// primed before the rebuild. An unprimed path stays unprimed and waits for its
// first redirect bind.
enum class PresentDecision : uint8_t {
    Wait,    // never primed: present the surface as drawn
    Upscale, // primed: the render target is where the application draws
};

constexpr PresentDecision DecidePresent(bool primed) {
    return primed ? PresentDecision::Upscale : PresentDecision::Wait;
}

} // namespace FSR1Plan
