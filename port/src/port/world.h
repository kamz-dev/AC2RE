#pragma once
// The world as the movement code queries it. PORT: the game queries its physics and guidance data; the port's real
// world will load the owner's install at run time. Tests and the sim use BoxWorld.

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

#include "scimitar/guidance.h"
#include "scimitar/math.h"

namespace port {

class World {
public:
    virtual ~World() = default;
    // Height of the walkable surface under (x, y) at or below `fromZ`, if any.
    virtual std::optional<float> groundBelow(float x, float y, float fromZ) const = 0;
    // The guidance components whose entities overlap the box (min, max): stands in for GuidanceZone::CastWorld's
    // broadphase (PhysicWorkspace::GetOverlappingEntities, PC 0x17C080, or the Havok broadphase) and the entities'
    // components (RE/19_guidance.md 3). A world without guidance has none.
    virtual void GetGuidanceSystemComponents(const scimitar::Vec3& min, const scimitar::Vec3& max,
                                             std::vector<const scimitar::GuidanceSystemComponent*>& out) const {
        (void)min, (void)max, (void)out;
    }
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
    void add(Box b) {
        boxes_.push_back(b);
        addGuidance(b);
    }

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

    // PORT: each box is an entity with one GuidanceSystemComponent: its four top edges, LedgeGrab, between the top
    // face (normal +Z) and the side face, with an EdgeFilter that takes ground/wall edges (the game's data sets its
    // own; RE/19 1). Edges shared by flush boxes are not removed.
    void GetGuidanceSystemComponents(const scimitar::Vec3& min, const scimitar::Vec3& max,
                                     std::vector<const scimitar::GuidanceSystemComponent*>& out) const override {
        for (const Guidance& g : guidance_) {
            if (g.max.x < min.x || g.min.x > max.x || g.max.y < min.y || g.min.y > max.y || g.max.z < min.z ||
                g.min.z > max.z)
                continue;
            out.push_back(g.component.get());
        }
    }

private:
    struct Guidance {
        std::unique_ptr<scimitar::GuidanceSystemComponent> component;
        scimitar::Vec3 min, max;  // its world AABB
    };

    void addGuidance(const Box& b) {
        auto c = std::make_unique<scimitar::GuidanceSystemComponent>();
        // The entity sits at the centre of the top face; its points are local to it.
        const scimitar::Vec3 origin{(b.min.x + b.max.x) * 0.5f, (b.min.y + b.max.y) * 0.5f, b.max.z};
        c->m_PortGlobalMatrix = Gear::Matrix44::Translation(origin);
        const float hx = (b.max.x - b.min.x) * 0.5f, hy = (b.max.y - b.min.y) * 0.5f;
        const scimitar::Vec3 corner[4] = {{-hx, -hy, 0}, {hx, -hy, 0}, {hx, hy, 0}, {-hx, hy, 0}};
        const scimitar::Vec3 side[4] = {{0, -1, 0}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}};
        std::uint32_t index[4];
        for (int i = 0; i < 4; ++i) index[i] = c->PortAddPoint(corner[i]);
        for (int i = 0; i < 4; ++i) {
            scimitar::GuidanceObject o;
            o.SetValid(true);
            o.SetSubType(scimitar::GuidanceObjectSubTypeLedgeGrab);
            o.SetLineIndex0(index[i]);
            o.SetLineIndex1(index[(i + 1) % 4]);
            o.SetLineNormalLeft({0, 0, 1});
            o.SetLineNormalRight(side[i]);
            c->m_GuidanceObjects.push_back(o);
        }
        c->m_EdgeFilter.m_Filter = scimitar::EdgeFilter::EdgeWallGround;
        Guidance g;
        c->PortGetAABBox(g.min, g.max);
        g.component = std::move(c);
        guidance_.push_back(std::move(g));
    }

    bool hasFloor_;
    float floorZ_;
    std::vector<Box> boxes_;
    std::vector<Guidance> guidance_;
};

}  // namespace port
