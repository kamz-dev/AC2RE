#pragma once
// scimitar::ResponseCurve: a piecewise-linear curve through (x, y) keys. GetOutput: Mac 0x507D10 (PC: called through
// the curve object's vftable, e.g. HumanGround +0x81C in UpdateMoveCycleMain 0x61BB60). Keys are added in order of x
// (HumanGround::LocalInit PC 0x61F920 adds them that way); the port doesn't sort them either.

#include <limits>
#include <vector>

namespace scimitar {

class ResponseCurve {
public:
    void AddKey(float x, float y) { m_Keys.push_back({x, y}); }
    void Clear() { m_Keys.clear(); }
    size_t GetKeyCount() const { return m_Keys.size(); }

    // As the game computes it: the last segment whose ends bracket x gives the output; 0 below the first key, above
    // the last, and with fewer than two keys. A vertical segment's slope is FLT_MAX (0 if it is also flat).
    float GetOutput(float x) const {
        if (m_Keys.size() < 2) return 0.0f;
        float out = 0.0f;
        for (size_t i = 0; i + 1 < m_Keys.size(); ++i) {
            const Key& a = m_Keys[i];
            const Key& b = m_Keys[i + 1];
            if (x < a.x || b.x < x) continue;
            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float slope = dx == 0.0f ? (dy == 0.0f ? 0.0f : std::numeric_limits<float>::max()) : dy / dx;
            out = (x - a.x) * slope + a.y;
        }
        return out;
    }

private:
    struct Key {
        float x, y;
    };
    std::vector<Key> m_Keys;
};

}  // namespace scimitar
