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
constexpr Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalized(Vec3 v) {
    float l = length(v);
    return l > 1e-6f ? v * (1.0f / l) : Vec3{};
}

// Yaw in radians, 0 = +Y (forward), counter-clockwise seen from above. PORT: the game's convention is not checked.
inline Vec3 forwardFromYaw(float yaw) { return {-std::sin(yaw), std::cos(yaw), 0}; }
inline float yawFromDirection(Vec3 d) { return std::atan2(-d.x, d.y); }

}  // namespace scimitar

namespace Gear {

// Gear::GetAngle(a, b, axis): PC 0x53590 (-> 0x53050, 0x52F90), Mac 0x9D08C0. The angle between the normalised a and
// b (0 when their dot is >= 1, pi when <= -1), negative when dot(cross(a, b), axis) < 0: positive when b is
// counter-clockwise from a about the axis.
inline float GetAngle(const scimitar::Vec3& a, const scimitar::Vec3& b, const scimitar::Vec3& axis) {
    const scimitar::Vec3 na = scimitar::normalized(a), nb = scimitar::normalized(b);
    const float c = scimitar::dot(na, nb);
    if (c >= 1.0f) return 0.0f;
    if (c <= -1.0f) return 3.1415927f;
    const float angle = std::acos(c);
    return scimitar::dot(scimitar::cross(na, nb), axis) < 0.0f ? -angle : angle;
}

// Gear::GetAngleAbsolute(a, b): Mac 0x144A27C (inlined on PC as acos of the clamped dot of the normalised vectors).
inline float GetAngleAbsolute(const scimitar::Vec3& a, const scimitar::Vec3& b) {
    const float c = scimitar::dot(scimitar::normalized(a), scimitar::normalized(b));
    if (c >= 1.0f) return 0.0f;
    if (c <= -1.0f) return 3.1415927f;
    return std::acos(c);
}

inline constexpr scimitar::Vec3 kBaseK{0, 0, 1};  // Vector4::GetBaseK

}  // namespace Gear
