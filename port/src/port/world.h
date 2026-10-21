#pragma once
// The world as the movement code queries it. PORT: the game queries its physics and guidance data; the port's real
// world will load the owner's install at run time. Tests and the sim use BoxWorld.

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "scimitar/math.h"

namespace port {

class World {
public:
    virtual ~World() = default;
    // Height of the walkable surface under (x, y) at or below `fromZ`, if any.
    virtual std::optional<float> groundBelow(float x, float y, float fromZ) const = 0;
    // Whether a capsule (segment a-b, radius) touches nothing: stands in for the game's physics query
    // (Human::CheckClearanceCapsuleGlobal, PC 0x934800). A world without collision is always clear.
    virtual bool capsuleIsClear(const scimitar::Vec3& a, const scimitar::Vec3& b, float radius) const {
        (void)a, (void)b, (void)radius;
        return true;
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

private:
    bool hasFloor_;
    float floorZ_;
    std::vector<Box> boxes_;
};

}  // namespace port
