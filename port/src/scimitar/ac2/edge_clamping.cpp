#include "scimitar/ac2/edge_clamping.h"

#include <cmath>
#include <utility>

#include "port/world.h"

namespace scimitar::ac2 {

namespace {
// Gear::Vector4 with the (1, 1, 0, 0) mask the clamping code multiplies by (Mac xmmword_2045630).
Vec3 Flat(const Vec3& v) { return {v.x, v.y, 0}; }

bool NullWithEpsilon(const Vec3& v, float eps) {
    return std::fabs(v.x) <= eps && std::fabs(v.y) <= eps && std::fabs(v.z) <= eps;
}

// scimitar::BaseArray::RemoveAtFast: the last element takes the removed one's place.
template <class T>
void RemoveAtFast(std::vector<T>& v, size_t i) {
    v[i] = v.back();
    v.pop_back();
}
}  // namespace

bool IsEdgeLeveled(const Vec3& p0, const Vec3& p1) {
    const Vec3 d = p0 - p1;
    if (NullWithEpsilon(d, 0.0005f)) return false;
    return !(std::fabs(normalized(d).z) > 0.64278764f);  // sin 40 degrees
}

Vec3 GetLineGlobalFlatNormal(const Vec3& p0, const Vec3& p1, const Vec3& faceNormal0, const Vec3& faceNormal1) {
    const Vec3 f = normalized(cross(p1 - p0, Gear::kBaseK));
    const Vec3& wall = faceNormal1.z > faceNormal0.z ? faceNormal0 : faceNormal1;  // the lower one
    return dot(f, normalized(Flat(wall))) >= 0.0f ? f : f * -1.0f;
}

void FilterSegmentListWithNormalAngle(std::vector<Segment>& segments, const Vec3& direction, float rejectAngle) {
    if (!(std::fabs(rejectAngle) > 0.0005f)) return;
    for (size_t i = 0; i < segments.size();) {
        if (rejectAngle <= Gear::GetAngleAbsolute(segments[i].m_Normal, direction))
            ++i;
        else
            RemoveAtFast(segments, i);  // the last one moves into i and is tested next
    }
}

namespace {
// Gear::EqualWithEpsilon (Mac 0x14485EE): every lane within eps.
bool EqualWithEpsilon(const Vec3& a, const Vec3& b, float eps) {
    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps && std::fabs(a.z - b.z) <= eps;
}

// IntersectLineWithLine (Mac 0x1C2930, PC 0x10FA790): P + t dA = Q + s dB. Parallel lines always fail.
bool IntersectLineWithLine(const Vec3& p, const Vec3& dA, const Vec3& q, const Vec3& dB, float& t, float& s) {
    const Vec3 w = q - p;
    const Vec3 c = cross(dA, dB);
    const bool parallel = std::fabs(c.x) <= 0.001f && std::fabs(c.y) <= 0.001f && std::fabs(c.z) <= 0.001f;
    const float dist = parallel ? length(w - dA * dot(w, dA)) : std::fabs(dot(w, c));
    if (dist > 0.01f || parallel) return false;
    // The axis is chosen on the components truncated to int (cvttss2si): below 1 they all tie and y is used.
    const int iz = std::abs(static_cast<int>(c.z)), ix = std::abs(static_cast<int>(c.x)),
              iy = std::abs(static_cast<int>(c.y));
    if (iz > ix && iz > iy) {
        t = (w.x * dB.y - w.y * dB.x) / c.z;
        s = (w.x * dA.y - w.y * dA.x) / c.z;
    } else if (ix > iy) {
        t = (w.y * dB.z - w.z * dB.y) / c.x;
        s = (w.y * dA.z - w.z * dA.y) / c.x;
    } else {
        t = (w.z * dB.x - w.x * dB.z) / c.y;
        s = (w.z * dA.x - w.x * dA.z) / c.y;
    }
    return true;
}

// IntersectSegmentWithSegment (Mac 0x1C3090, PC 0x10FC800). tA / tB are written even when the result is rejected.
bool IntersectSegmentWithSegment(const Vec3& a0, const Vec3& a1, const Vec3& b0, const Vec3& b1, Vec3& x, float& tA,
                                 float& tB) {
    float t = 0, s = 0;
    if (!IntersectLineWithLine(a0, a1 - a0, b0, b1 - b0, t, s)) return false;
    tA = t;
    tB = s;
    if (t < 0 || t > 1 || s < 0 || s > 1) return false;
    x = a0 + (a1 - a0) * t;
    return true;
}

// OrientSegmentInDirection (Mac 0xD1CABC, PC 0xEC3FF0): swaps the ends so the segments run the same way relative to
// the direction of travel.
void OrientSegmentInDirection(Vec3& p0, Vec3& p1, const Vec3& normal, const Vec3& dir, const Vec3& perp, float scale) {
    const Vec3 d = normalized(Flat(p1 - p0));
    const float angle = Gear::GetAngle(dir, normal, Gear::kBaseK * scale);
    bool swap;
    if (angle == 0.0f)
        swap = dot(d, perp) > 0.0f;
    else if (std::fabs(angle) == 3.1415927f)
        swap = dot(d, perp) < 0.0f;
    else if (!(angle < 0.0f))
        swap = dot(d, dir) > 0.0f;
    else
        swap = dot(d, dir) < 0.0f;
    if (swap) std::swap(p0, p1);
}

// GetBestSegmentAngleTouchingPoint (Mac 0xD372A8, PC 0x1007580): among the segments from `start` with an end at `pt`,
// the one turning least from `dir`. A beaten candidate is deleted from the array.
bool GetBestSegmentAngleTouchingPoint(const Vec3& pt, const Vec3& dir, bool backward, std::vector<Segment>& segs,
                                      size_t start, size_t& index, bool& reversed) {
    size_t n = segs.size();
    bool found = false;
    reversed = false;
    float best = 10000.0f;
    for (size_t i = start; i < n; ++i) {
        bool atP0;
        if (EqualWithEpsilon(pt, segs[i].m_Points[0], 0.001f))
            atP0 = true;
        else if (EqualWithEpsilon(pt, segs[i].m_Points[1], 0.001f))
            atP0 = false;
        else
            continue;
        const Vec3 cd = segs[i].m_Points[1] - segs[i].m_Points[0];
        float a = backward ? Gear::GetAngle(cd, dir, Gear::kBaseK) : Gear::GetAngle(dir, cd, Gear::kBaseK);
        if (backward == atP0) a = 3.1415927f - a;
        if (best > a) {
            if (!found) {
                index = i;
                found = true;
            } else {
                --n;
                if (i != n) std::swap(segs[i], segs[n]);
                RemoveAtFast(segs, index);  // the current one (now last) moves into `index`
            }
            reversed = backward == atP0;
            best = a;
        }
    }
    return found;
}
}  // namespace

bool GetEdgeSegmentNearCharacterPos(const port::World& world, const Vec3& entityPosition, const Vec3& entitySight,
                                    const Vec3& gapPosition, const Vec3& gapNormal, const Vec3& direction,
                                    float radius, const EdgeClampingSettings& settings, std::vector<Segment>& segs,
                                    unsigned maxCount) {
    // The query: a GuidanceZoneBox on the entity (X = sight x K, Y = sight, Z = K), local bounds (-r, -r, -0.65) to
    // (r, r, 0.3), cast with GuidanceZone::CastWorld (contacts, every subtype; PC 0x1007F8D).
    GuidanceZoneBox box;
    box.m_Matrix.m_Rows[0] = normalized(cross(entitySight, Gear::kBaseK));
    box.m_Matrix.m_Rows[1] = entitySight;
    box.m_Matrix.m_Rows[2] = Gear::kBaseK;
    box.m_Matrix.m_Rows[3] = entityPosition;
    box.m_Min = {-radius, -radius, -0.65f};
    box.m_Max = {radius, radius, 0.3f};
    std::vector<GuidanceContactObject> report;
    box.CastWorld(report, false, kGuidanceContactObjectSubType_All, world);
    segs.clear();
    for (const GuidanceContactObject& c : report) {
        if (segs.size() == maxCount) break;
        if (!IsEdgeLeveled(c.m_LineGlobalPoint0, c.m_LineGlobalPoint1)) continue;
        Segment s;
        s.m_Points[0] = Flat(c.m_LineGlobalPoint0);
        s.m_Points[1] = Flat(c.m_LineGlobalPoint1);
        s.m_Normal = normalized(Flat(GetLineGlobalFlatNormal(c.m_LineGlobalPoint0, c.m_LineGlobalPoint1,
                                                             c.m_LineGlobalNormalLeft, c.m_LineGlobalNormalRight)));
        segs.push_back(s);
    }

    // Split crossing segments: the first part keeps p0, the tail is appended.
    size_t count = segs.size();
    if (count > 1) {
        size_t i = 0;
        while (i < count) {
            const Vec3 a0 = segs[i].m_Points[0], a1 = segs[i].m_Points[1];
            bool split = false;
            bool full = false;
            for (size_t j = i + 1; j < count; ++j) {
                const Vec3 b0 = segs[j].m_Points[0], b1 = segs[j].m_Points[1];
                Vec3 x;
                float tA = 0, tB = 0;
                if (!IntersectSegmentWithSegment(a0, a1, b0, b1, x, tA, tB)) continue;
                if (!(std::fabs(tA) <= 0.0005f) && !(std::fabs(tA - 1.0f) <= 0.0005f)) {
                    segs[i].m_Points[1] = x;
                    segs.push_back(Segment{{x, a1}, segs[i].m_Normal});
                    if (++count == maxCount) {
                        full = true;
                        break;
                    }
                    split = true;
                }
                if (!(std::fabs(tB) <= 0.0005f) && !(std::fabs(tB - 1.0f) <= 0.0005f)) {
                    segs[j].m_Points[1] = x;
                    segs.push_back(Segment{{x, b1}, segs[j].m_Normal});
                    if (++count == maxCount) {
                        full = true;
                        break;
                    }
                }
                if (split) break;  // test segment i again, shortened
            }
            if (full) break;
            if (!split) ++i;
        }
    }

    // Orient every segment against the direction of travel.
    const Vec3 dirF = normalized(Flat(direction));
    const Vec3 perp{dirF.y, -dirF.x, 0};
    for (Segment& s : segs) OrientSegmentInDirection(s.m_Points[0], s.m_Points[1], s.m_Normal, dirF, perp, 1.0f);

    // The seed: the segment with the gap's normal closest to the gap's position.
    float best = 10000.0f;
    int seed = -1;
    for (size_t k = 0; k < segs.size(); ++k) {
        if (!EqualWithEpsilon(gapNormal, segs[k].m_Normal, 0.001f)) continue;
        const Vec3 proj = PointSegmentProjection(gapPosition, segs[k].m_Points[0], segs[k].m_Points[1]);
        const float d2 = dot(proj - gapPosition, proj - gapPosition);
        if (best > d2) {
            seed = static_cast<int>(k);
            if (std::fabs(d2) <= 0.0005f) break;
            best = d2;
        }
    }
    if (seed == -1) return false;
    if (seed != 0) std::swap(segs[static_cast<size_t>(seed)], segs[0]);

    // The chain: forward from the seed's p1, then backward from its p0, through segments touching end to end.
    size_t j = 0;
    bool reversed = false;
    while (j + 1 < segs.size()) {
        Vec3 pt = segs[j].m_Points[1];
        Vec3 dir = segs[j].m_Points[1] - segs[j].m_Points[0];
        if (reversed) {
            dir = dir * -1.0f;
            reversed = false;
            pt = segs[j].m_Points[0];
        }
        size_t idx = 0;
        if (!GetBestSegmentAngleTouchingPoint(pt, dir, false, segs, j + 1, idx, reversed)) break;
        std::swap(segs[j + 1], segs[idx]);
        ++j;
    }
    if (j != 0) std::swap(segs[0], segs[j]);
    reversed = false;
    while (j + 1 < segs.size()) {
        Vec3 pt = segs[j].m_Points[0];
        Vec3 dir = segs[j].m_Points[1] - segs[j].m_Points[0];
        if (reversed) {
            dir = dir * -1.0f;
            reversed = false;
            pt = segs[j].m_Points[1];
        }
        size_t idx = 0;
        if (!GetBestSegmentAngleTouchingPoint(pt, dir, true, segs, j + 1, idx, reversed)) break;
        ++j;
        std::swap(segs[j], segs[idx]);
    }
    // (A box-clipping pass follows in the game, behind a static flag that is 0: never runs.)
    if (j + 1 < segs.size()) segs.resize(j + 1);

    FilterSegmentListWithNormalAngle(segs, settings.m_DirectionForRejectAngle, settings.m_RejectAngle);
    return !segs.empty();
}

int DisplacementCollideWithPlane(Vec3& hit, int& index, const Vec3& start, const Vec3& end, float edgeDistance,
                                 const std::vector<Segment>& shifted, const std::vector<Segment>& segments) {
    const float r = edgeDistance + 0.0005f;
    const float r2 = r * r;
    const Vec3 se = end - start;
    if (dot(se, se) < 2.5e-7f) return 0;
    const Vec3 dirN = normalized(se);
    float best = 100000.0f;
    index = -1;
    int type = 0;
    for (size_t i = 0; i < shifted.size(); ++i) {
        const Vec3& n = shifted[i].m_Normal;
        if (dot(n, dirN) <= 0.0f) continue;  // only moving out over the edge
        const float num = dot(n, shifted[i].m_Points[0] - start);
        const float den = dot(n, se);
        if (den == 0.0f) continue;
        const float t = num / den;
        if (t > 0.0f) {
            if (t <= 1.0f) {
                const Vec3 h = start * (1.0f - t) + end * t;
                const Vec3 q = PointSegmentProjection(h, segments[i].m_Points[0], segments[i].m_Points[1]);
                const float dd = dot(q - h, q - h);
                if (best > dd && r2 > dd) {
                    index = static_cast<int>(i);
                    hit = h;
                    best = dd;
                    type = 1;
                }
            }
        } else if (dot(segments[i].m_Normal, segments[i].m_Points[0] - start) / den >= 0.0f) {
            // Already past the pulled-back plane, but not past the edge itself.
            const Vec3 q = PointSegmentProjection(start, segments[i].m_Points[0], segments[i].m_Points[1]);
            const float dd = dot(q - start, q - start);
            if (best > dd && r2 > dd) {
                index = static_cast<int>(i);
                hit = start;
                best = dd;
                type = 2;
            }
        }
    }
    return index == -1 ? 0 : type;
}

bool DisplacementCullingBySegment(Vec3& out, const Vec3& position, const Vec3& d, float edgeDistance,
                                  std::vector<Segment>& segments) {
    size_t n = segments.size();
    if (n == 0) return false;
    if (dot(d, d) < 2.5e-7f) {
        out = {};
        return true;
    }
    const Vec3 p = Flat(position);
    const Vec3 dFlat = Flat(d);
    const Vec3 dirN = normalized(dFlat);
    // The edges pulled back by the edge distance, against their normals.
    std::vector<Segment> shifted = segments;
    for (Segment& s : shifted) {
        const Vec3 back = s.m_Normal * -edgeDistance;
        s.m_Points[0] += back;
        s.m_Points[1] += back;
    }
    Vec3 start = p;
    Vec3 end = p + dFlat;
    bool hadHit = false;
    bool more = false;
    do {
        Vec3 hit;
        int idx = -1;
        const int r = DisplacementCollideWithPlane(hit, idx, start, end, edgeDistance, shifted, segments);
        if (r == 0) break;
        const Vec3 ref = r == 1 ? shifted[idx].m_Points[0] : start;
        start = hit;
        const Vec3 rest = end - start;
        if (dot(rest, rest) > 2.5e-7f) {
            // Slide along the edge: the end projected onto the plane.
            const Vec3 projected = ProjectPointOnPlane(end, ref, shifted[idx].m_Normal, shifted[idx].m_Normal * -1.0f);
            if (hadHit && dot(dirN, rest) < 0.0f) {
                end = start;
                more = false;
            } else {
                end = projected;
                more = true;
            }
            hadHit = true;
        } else {
            end = start;
            more = false;
        }
        if (--n) {
            RemoveAtFast(shifted, static_cast<size_t>(idx));
            RemoveAtFast(segments, static_cast<size_t>(idx));
        }
    } while (more && n);
    if (!hadHit) return false;
    out = end - p;
    return true;
}

}  // namespace scimitar::ac2
