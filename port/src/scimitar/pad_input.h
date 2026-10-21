#pragma once
// scimitar::PadInput: the pad's per-stick processing (RE/21_input.md). PC: ctor 0x1E8B00 [Mac 0x820150], Update
// 0x1E8740 [0x81FC30], ComputeRelativeInput 0x1E84A0 [0x81F570]. The player's PadInput is AssassinTask +0x80
// (ControlScheme +0x10), live 2026-10-21.
//
// The port has the parts AssassinTask reads: the length (capped at 1) and the relative / damped directions. "Damped"
// is not a time filter: the stick's angle from forward goes through a response curve that pulls directions near
// forward towards it (at most 10 degrees, at 30); live, the two directions differed by at most 10.0 degrees.

#include <cmath>

#include "scimitar/math.h"
#include "scimitar/response_curve.h"

namespace scimitar {

class PadInput {
public:
    static constexpr int kStickCount = 2;  // Pad::PadStick: 0 the movement stick, 1 the camera stick

    // PC 0x1E8B00: the snap cone and the curves (stick 0 keys (x = |angle| / pi, y = angle in radians); stick 1 is
    // the identity).
    PadInput() {
        m_SnapToForwardCos[0] = 0.99619472f;  // +0xA8 (Mac +0xA4): cos 5 degrees
        m_SnapToForwardCos[1] = 1.0f;
        ResponseCurve& c = m_StickAngleCurve[0];  // +0x600
        c.AddKey(0.0f, 0.0f);
        c.AddKey(0.027777776f, 0.017453292f);
        c.AddKey(0.055555552f, 0.08726646f);
        c.AddKey(0.1111111f, 0.17453292f);
        c.AddKey(0.16666667f, 0.34906584f);
        c.AddKey(0.22222221f, 0.69813168f);
        c.AddKey(1.0f, 3.1415927f);
        m_StickAngleCurve[1].AddKey(0.0f, 0.0f);  // +0x68C
        m_StickAngleCurve[1].AddKey(1.0f, 3.1415927f);
    }

    struct RelativeInput {
        Vec3 raw;       // AddRawRelativeStickValue
        Vec3 relative;  // AddRelativeStickValue: ControlScheme::GetRelativeStick
        Vec3 damped;    // AddDampedRelativeStickValue: ControlScheme::GetDampedRelativeStick
    };

    // PC 0x1E84A0. `stick` is the stick in stick space (+Y forward); `cameraYaw` stands in for
    // CameraProxy::RemapPadInput (PC 0x52740), PORT: taken as a rotation about +Z by the camera's heading.
    RelativeInput ComputeRelativeInput(const Vec3& stick, int stickIndex, float cameraYaw) const {
        RelativeInput out;
        Vec3 s = normalized(Vec3{stick.x, stick.y, 0});
        out.raw = Remap(s, cameraYaw);
        // Within the cone around forward the stick becomes forward (PC: always; Mac gates this on a flag).
        if (s.y > m_SnapToForwardCos[stickIndex]) s = Vec3{0, 1, 0};
        out.relative = Remap(s, cameraYaw);
        out.damped = out.relative;
        if (!(std::fabs(s.x) <= 0.0005f && std::fabs(s.y) <= 0.0005f)) {
            // Gear::GetAngle(stick, BaseJ, BaseK), then forward turned by curve(|angle| / pi) on the stick's side.
            const float angle = yawFromDirection(s);
            const float sign = angle < 0.0f ? -1.0f : 1.0f;
            const float turned = m_StickAngleCurve[stickIndex].GetOutput(std::fabs(angle) / 3.1415927f) * sign;
            out.damped = Remap(forwardFromYaw(turned), cameraYaw);
        }
        return out;
    }

    // PadInput::Update (PC 0x1E8740): the stored length is capped at 1.
    static float CapLength(float length) { return length > 1.0f ? 1.0f : length; }

private:
    static Vec3 Remap(const Vec3& v, float cameraYaw) {
        const float c = std::cos(cameraYaw), s = std::sin(cameraYaw);
        return {v.x * c - v.y * s, v.x * s + v.y * c, 0};
    }

    float m_SnapToForwardCos[kStickCount];
    ResponseCurve m_StickAngleCurve[kStickCount];
};

}  // namespace scimitar
