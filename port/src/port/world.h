#pragma once
// The world as the movement code queries it. PORT: the game queries its physics and guidance data; the port's real
// world will load the owner's install at run time. Tests and the sim use BoxWorld.

#include <optional>
#include <vector>

#include "scimitar/math.h"

namespace port {

class World {
public:
    virtual ~World() = default;
    // Height of the walkable surface under (x, y) at or below `fromZ`, if any.
    virtual std::optional<float> groundBelow(float x, float y, float fromZ) const = 0;
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

private:
    bool hasFloor_;
    float floorZ_;
    std::vector<Box> boxes_;
};

}  // namespace port
