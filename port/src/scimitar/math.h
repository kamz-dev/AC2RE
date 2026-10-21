#pragma once
// Vector math. Game space is Z-up like AC1 (hypothesis for AC2; LIVE: confirm).
// PORT: rename to the engine's own vector type and operations once identified in the symbols.

#include <cmath>

namespace scimitar {

struct Vec3 {
    float x = 0, y = 0, z = 0;

    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }
    constexpr bool operator==(const Vec3&) const = default;
};

constexpr float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalized(Vec3 v) {
    float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : Vec3{};
}

// Yaw in radians, 0 = +Y (forward), counter-clockwise seen from above. PORT: the game's convention is not checked.
inline Vec3 forwardFromYaw(float yaw) { return {-std::sin(yaw), std::cos(yaw), 0}; }
inline float yawFromDirection(Vec3 d) { return std::atan2(-d.x, d.y); }

}  // namespace scimitar
