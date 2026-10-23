#pragma once
// The world as the movement code queries it. PORT: the game queries its physics and guidance data; the port's real
// world will load the owner's install at run time. Tests and the sim use BoxWorld.

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "scimitar/math.h"

namespace port {

// A guidance line as a cast reports it (the game's GuidanceContactObject, 0x60 bytes: the edge P0 +0x10 / P1 +0x20,
// the two faces' normals +0x30 / +0x40, the subtype byte +0x5C & 0x3F; RE/11_ground.md 16).
struct GuidanceLine {
    scimitar::Vec3 p0, p1;
    scimitar::Vec3 faceNormal0, faceNormal1;
    int subType = 0;
};

class World {
public:
    virtual ~World() = default;
    // Height of the walkable surface under (x, y) at or below `fromZ`, if any.
    virtual std::optional<float> groundBelow(float x, float y, float fromZ) const = 0;
    // The guidance lines a GuidanceZoneSphere (GuidanceZone::CastWorld, PC 0x3FEA00) or a GuidanceZoneBox finds. The
    // box is given by its matrix (the axes and the origin) and its local bounds. PORT: the game casts the guidance
    // data of the entities the zone overlaps (CastEntityInternal, not decoded); the port's worlds supply their own
    // lines and test the overlap themselves. A world without guidance finds none.
    virtual void CastGuidanceSphere(const scimitar::Vec3& center, float radius, std::vector<GuidanceLine>& out) const {
        (void)center, (void)radius, (void)out;
    }
    struct OrientedBox {
        scimitar::Vec3 axisX, axisY, axisZ, origin;
        scimitar::Vec3 min, max;  // local bounds
    };
    virtual void CastGuidanceBox(const OrientedBox& box, std::vector<GuidanceLine>& out) const { (void)box, (void)out; }
    // Whether a capsule (segment a-b, radius) touches nothing: stands in for the game's physics query
    // (Human::CheckClearanceCapsuleGlobal, PC 0x934800). A world without collision is always clear.
    virtual bool capsuleIsClear(const scimitar::Vec3& a, const scimitar::Vec3& b, float radius) const {
        (void)a, (void)b, (void)radius;
        return true;
    }
    // scimitar::ContactPoint as a ray cast fills it: +0 the position, +0x10 the surface normal.
    struct ContactPoint {
        scimitar::Vec3 position, normal;
    };
    // The first surface on the segment origin + dir * [0, length], or none when it is clear: stands in for
    // scimitar::ac2::CheckClearanceRayCast (PC 0xF62310, Mac 0xD39510), which returns true when clear. `layer` is the
    // CollisionFilterInfo::Layer (Revelations names: 12 CharacterNoInteractive, 22 MainCharacterNoInteractive, 35
    // OcclusionHeightTest). PORT: the port's worlds are static geometry, which every layer hits. A world without
    // collision is always clear.
    virtual std::optional<ContactPoint> rayCast(const scimitar::Vec3& origin, const scimitar::Vec3& dir, float length,
                                                int layer) const {
        (void)origin, (void)dir, (void)length, (void)layer;
        return std::nullopt;
    }
};

// Axis-aligned boxes standing on an infinite floor (or a void when `hasFloor` is false).
class BoxWorld : public World {
public:
    struct Box {
        scimitar::Vec3 min, max;
    };

    explicit BoxWorld(bool hasFloor = true, float floorZ = 0) : hasFloor_(hasFloor), floorZ_(floorZ) {}
    void add(Box b) { boxes_.push_back(b); }

    std::optional<float> groundBelow(float x, float y, float fromZ) const override {
        std::optional<float> best;
        if (hasFloor_ && floorZ_ <= fromZ) best = floorZ_;
        for (const Box& b : boxes_) {
            if (x >= b.min.x && x <= b.max.x && y >= b.min.y && y <= b.max.y && b.max.z <= fromZ &&
                (!best || b.max.z > *best))
                best = b.max.z;
        }
        return best;
    }

    // Exact for vertical segments (the only kind the movement code casts so far); others are tested by their ends.
    bool capsuleIsClear(const scimitar::Vec3& a, const scimitar::Vec3& b, float radius) const override {
        const float z0 = std::min(a.z, b.z), z1 = std::max(a.z, b.z);
        for (const Box& box : boxes_) {
            for (const scimitar::Vec3& p : {a, b}) {
                const float dx = std::max({box.min.x - p.x, 0.0f, p.x - box.max.x});
                const float dy = std::max({box.min.y - p.y, 0.0f, p.y - box.max.y});
                const float dz = std::max({box.min.z - z1, 0.0f, z0 - box.max.z});
                if (dx * dx + dy * dy + dz * dz < radius * radius) return false;
            }
        }
        return true;
    }

    // The floor and the boxes. PORT: a box the ray starts inside is not hit (as a convex shape's ray cast).
    std::optional<ContactPoint> rayCast(const scimitar::Vec3& origin, const scimitar::Vec3& dir, float length,
                                        int layer) const override {
        (void)layer;
        std::optional<ContactPoint> best;
        float bestT = length;
        if (hasFloor_ && dir.z < 0 && origin.z >= floorZ_) {
            const float t = (floorZ_ - origin.z) / dir.z;
            if (t <= bestT) {
                bestT = t;
                best = ContactPoint{origin + dir * t, {0, 0, 1}};
            }
        }
        for (const Box& b : boxes_) {
            const float o[3] = {origin.x, origin.y, origin.z}, d[3] = {dir.x, dir.y, dir.z};
            const float lo[3] = {b.min.x, b.min.y, b.min.z}, hi[3] = {b.max.x, b.max.y, b.max.z};
            float t0 = -3.4028235e38f, t1 = 3.4028235e38f;
            int axis = -1;
            float sign = 0;
            bool miss = false;
            for (int i = 0; i < 3 && !miss; ++i) {
                if (std::fabs(d[i]) < 1e-9f) {
                    miss = o[i] < lo[i] || o[i] > hi[i];
                    continue;
                }
                float ta = (lo[i] - o[i]) / d[i], tb = (hi[i] - o[i]) / d[i];
                float s = -1;  // entering through the low face
                if (ta > tb) {
                    std::swap(ta, tb);
                    s = 1;
                }
                if (ta > t0) {
                    t0 = ta;
                    axis = i;
                    sign = s;
                }
                t1 = std::min(t1, tb);
                miss = t0 > t1;
            }
            if (miss || axis < 0 || t0 < 0 || t0 > bestT) continue;
            bestT = t0;
            scimitar::Vec3 n;
            (axis == 0 ? n.x : axis == 1 ? n.y : n.z) = sign;
            best = ContactPoint{origin + dir * t0, n};
        }
        return best;
    }

    // PORT: every box's four top edges are guidance lines, with the top face's normal (0, 0, 1) and the side face's
    // outward normal. Edges shared by flush boxes are not removed.
    void CastGuidanceSphere(const scimitar::Vec3& center, float radius, std::vector<GuidanceLine>& out) const override {
        // PORT: a line counts when it comes within the radius (CastEntityInternal's test isn't decoded).
        forEachTopEdge([&](const GuidanceLine& line) {
            if (distanceToSegmentSq(center, line.p0, line.p1) <= radius * radius) out.push_back(line);
        });
    }

    void CastGuidanceBox(const OrientedBox& box, std::vector<GuidanceLine>& out) const override {
        // PORT: a line counts when part of it lies inside the box (CastEntityInternal's test isn't decoded).
        forEachTopEdge([&](const GuidanceLine& line) {
            if (segmentTouchesBox(box, line.p0, line.p1)) out.push_back(line);
        });
    }

private:
    template <class F>
    void forEachTopEdge(F&& f) const {
        for (const Box& b : boxes_) {
            const float z = b.max.z;
            const scimitar::Vec3 c[4] = {{b.min.x, b.min.y, z}, {b.max.x, b.min.y, z}, {b.max.x, b.max.y, z},
                                         {b.min.x, b.max.y, z}};
            const scimitar::Vec3 side[4] = {{0, -1, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}};
            for (int i = 0; i < 4; ++i) f(GuidanceLine{c[i], c[(i + 1) % 4], {0, 0, 1}, side[i], 0});
        }
    }

    static float distanceToSegmentSq(const scimitar::Vec3& p, const scimitar::Vec3& a, const scimitar::Vec3& b) {
        const scimitar::Vec3 ab = b - a;
        const float len2 = scimitar::dot(ab, ab);
        float t = len2 > 0 ? scimitar::dot(p - a, ab) / len2 : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);
        const scimitar::Vec3 d = a + ab * t - p;
        return scimitar::dot(d, d);
    }

    // Clips the segment against the box's three slabs.
    static bool segmentTouchesBox(const OrientedBox& box, const scimitar::Vec3& a, const scimitar::Vec3& b) {
        const scimitar::Vec3 axes[3] = {box.axisX, box.axisY, box.axisZ};
        const float lo[3] = {box.min.x, box.min.y, box.min.z}, hi[3] = {box.max.x, box.max.y, box.max.z};
        float t0 = 0, t1 = 1;
        for (int i = 0; i < 3; ++i) {
            const float pa = scimitar::dot(a - box.origin, axes[i]);
            const float d = scimitar::dot(b - a, axes[i]);
            if (std::fabs(d) < 1e-9f) {
                if (pa < lo[i] || pa > hi[i]) return false;
                continue;
            }
            float ta = (lo[i] - pa) / d, tb = (hi[i] - pa) / d;
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta);
            t1 = std::min(t1, tb);
            if (t0 > t1) return false;
        }
        return true;
    }

    bool hasFloor_;
    float floorZ_;
    std::vector<Box> boxes_;
};

}  // namespace port
