// Original, standalone design examples for the FSR3 handoff.
// These do not call MobileGlues, JNI, Minecraft or GL. They are not product tests.
// Build/run instructions and intentionally omitted responsibilities are in
// ../implementation-notes.md.
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>

namespace example {

enum class Route { Native, SceneTemporal };

struct Eligibility {
    bool world = false;
    bool shaders = false;
    bool profile_qualified = false;
    bool protocol_ready = false;
    bool resources_ready = false;
    bool temporal_requested = false;
};

Route choose_route(const Eligibility& e) {
    return e.world && e.shaders && e.profile_qualified && e.protocol_ready &&
                   e.resources_ready && e.temporal_requested
               ? Route::SceneTemporal
               : Route::Native;
}

struct StreamKey {
    std::uint64_t context, surface, resources, profile, projection;
    bool operator==(const StreamKey&) const = default;
};

struct CameraSample {
    StreamKey stream{};
    std::array<double, 3> position{};
    double seconds = 0.0; // A monotonic timestamp; not wall-clock time.
    bool valid = false;
    bool reset = false;
};

bool finite_sample(const CameraSample& s) {
    if (!s.valid || !std::isfinite(s.seconds)) return false;
    for (const double x : s.position)
        if (!std::isfinite(x)) return false;
    return true;
}

bool can_reproject(const std::optional<CameraSample>& previous,
                   const CameraSample& current,
                   double maximum_gap_seconds,
                   double teleport_distance) {
    if (!previous || !finite_sample(*previous) || !finite_sample(current) ||
        current.reset || !(previous->stream == current.stream) ||
        !std::isfinite(maximum_gap_seconds) || maximum_gap_seconds <= 0.0 ||
        !std::isfinite(teleport_distance) || teleport_distance <= 0.0)
        return false;
    const double dt = current.seconds - previous->seconds;
    if (!(dt > 0.0 && dt <= maximum_gap_seconds)) return false;
    const auto& p = previous->position;
    const auto& c = current.position;
    // Subtract in double precision before any GPU float conversion.
    const double distance = std::hypot(c[0] - p[0], c[1] - p[1], c[2] - p[2]);
    return distance <= teleport_distance;
}

struct Vec2 { double x, y; };
using Clip = std::array<double, 4>;

// Unjittered clip positions of THE SAME SURFACE POINT at both frame times.
// Result: current-to-previous render pixels, origin bottom-left, +Y up.
// A producer with another origin must convert exactly once at the boundary.
std::optional<Vec2> motion_pixels(const Clip& current, const Clip& previous,
                                  double render_width, double render_height) {
    if (!std::isfinite(render_width) || render_width <= 0.0 ||
        !std::isfinite(render_height) || render_height <= 0.0)
        return std::nullopt;
    for (const Clip* p : {&current, &previous}) {
        for (const double x : *p)
            if (!std::isfinite(x)) return std::nullopt;
        if ((*p)[3] <= 1e-8) return std::nullopt;
    }
    Vec2 result{
        (previous[0] / previous[3] - current[0] / current[3]) * 0.5 * render_width,
        (previous[1] / previous[3] - current[1] / current[3]) * 0.5 * render_height
    };
    if (!std::isfinite(result.x) || !std::isfinite(result.y)) return std::nullopt;
    return result;
}

} // namespace example

int main() {
    using namespace example;
    int checks = 0;
    const auto check = [&checks](bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
        ++checks;
    };
    const auto close = [](double a, double b) { return std::abs(a - b) < 1e-9; };
    Eligibility e{true, true, true, true, true, true};
    check(choose_route(e) == Route::SceneTemporal, "qualified world");
    e.world = false;
    check(choose_route(e) == Route::Native, "title/menu");
    e.world = true; e.protocol_ready = false;
    check(choose_route(e) == Route::Native, "protocol mismatch");
    e.protocol_ready = true; e.profile_qualified = false;
    check(choose_route(e) == Route::Native, "unknown pack");
    e.profile_qualified = true; e.temporal_requested = false;
    check(choose_route(e) == Route::Native, "Off");

    CameraSample current{{1, 1, 1, 1, 1}, {1e9, 64.0, 0.0}, 1.0, true, false};
    std::optional<CameraSample> previous;
    const auto continuous = [&] { return can_reproject(previous, current, 0.25, 64.0); };
    check(!continuous(), "first frame resets");
    previous = current; current.seconds += 0.016; current.position[0] += 0.125;
    check(continuous(), "second frame recovers at large coordinates");
    check(close(current.position[0] - previous->position[0], 0.125), "double delta");
    current.reset = true;
    check(!continuous(), "cut resets");
    previous = current; current.seconds += 0.016; current.reset = false;
    check(continuous(), "frame after cut can recover");
    previous = current; current.seconds += 1.0;
    check(!continuous(), "time gap resets");
    previous = current; current.seconds += 0.016;
    check(continuous(), "frame after gap can recover");
    current.position[0] += 100.0;
    check(!continuous(), "teleport resets");
    previous = current; current.seconds += 0.016; ++current.stream.resources;
    check(!continuous(), "resize generation resets");
    previous = current; current.seconds += 0.016; ++current.stream.context;
    check(!continuous(), "context switch resets");
    previous = current; current.seconds += 0.016;
    current.position[0] = std::numeric_limits<double>::quiet_NaN();
    check(!continuous(), "invalid sample resets");
    previous = current; current.position[0] = 1e9; current.seconds += 0.016;
    check(!continuous(), "invalid previous sample cannot reproject");

    // In a 100x80 render grid, 0.04 NDC is two pixels; 0.075 is three.
    const Clip before{0.0, 0.0, 0.0, 1.0};
    const auto right = motion_pixels({0.04, 0.0, 0.0, 1.0}, before, 100, 80);
    check(right && close(right->x, -2.0) && close(right->y, 0.0), "rightward motion");
    const auto up = motion_pixels({0.0, 0.075, 0.0, 1.0}, before, 100, 80);
    check(up && close(up->x, 0.0) && close(up->y, -3.0), "upward motion");
    check(!motion_pixels({0.0, 0.0, 0.0, -1.0}, before, 100, 80), "behind camera");
    check(!motion_pixels(before, before, 0, 80), "empty render grid");
    std::cout << "Illustrative contract checks: " << checks
              << " PASS (no product/JNI/GPU execution)\n";
}
