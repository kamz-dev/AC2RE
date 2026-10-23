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

// scimitar::RotateVector(vec, axis, angle): PC 0xC6E90 (-> 0xC5530 the quaternion, 0xC5620 the rotation), Mac
// 0x2126722 (thunk). The normalised `vec` turned by `angle` about `axis` (right-handed: a positive angle turns
// counter-clockwise seen from the axis' tip), normalised again. A null axis is the identity quaternion.
inline Vec3 RotateVector(const Vec3& vec, const Vec3& axis, float angle) {
    const float axisLength = length(axis);
    Vec3 q;  // the quaternion's vector part, w its scalar part
    float w = 1;
    if (axisLength != 0) {
        q = axis * (std::sin(angle * 0.5f) / axisLength);
        w = std::cos(angle * 0.5f);
    }
    const Vec3 v = vec * (1.0f / length(vec));
    const Vec3 r = (q * dot(q, v) + cross(q, v) * w + v * (w * w - 0.5f)) * 2.0f;
    return r * (1.0f / length(r));
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

// Gear::Matrix44<float>, as the engine stores an entity's or a zone's frame: rows 0-2 the axes (X, Y, Z), row 3 the
// position (TransformVertex: x·row0 + y·row1 + z·row2 + row3). PORT: only the affine part the port uses.
struct Matrix44 {
    scimitar::Vec3 m_Rows[4] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, 0}};

    static Matrix44 Translation(const scimitar::Vec3& t) {
        Matrix44 m;
        m.m_Rows[3] = t;
        return m;
    }
    scimitar::Vec3 TransformVector(const scimitar::Vec3& v) const {
        return m_Rows[0] * v.x + m_Rows[1] * v.y + m_Rows[2] * v.z;
    }
    scimitar::Vec3 TransformVertex(const scimitar::Vec3& v) const { return TransformVector(v) + m_Rows[3]; }
    // Matrix44::SetAffineInverse: the transpose of the rotation, and the position brought back through it (the axes
    // are orthonormal).
    Matrix44 GetAffineInverse() const {
        Matrix44 r;
        for (int i = 0; i < 3; ++i) {
            r.m_Rows[i] = {i == 0 ? m_Rows[0].x : i == 1 ? m_Rows[0].y : m_Rows[0].z,
                           i == 0 ? m_Rows[1].x : i == 1 ? m_Rows[1].y : m_Rows[1].z,
                           i == 0 ? m_Rows[2].x : i == 1 ? m_Rows[2].y : m_Rows[2].z};
        }
        r.m_Rows[3] = r.TransformVector(m_Rows[3]) * -1.0f;
        return r;
    }
    // Matrix44::operator*= as the engine composes frames: (this * other) maps a point through `this`, then `other`.
    Matrix44 operator*(const Matrix44& other) const {
        Matrix44 r;
        for (int i = 0; i < 3; ++i) r.m_Rows[i] = other.TransformVector(m_Rows[i]);
        r.m_Rows[3] = other.TransformVertex(m_Rows[3]);
        return r;
    }
};

}  // namespace Gear
