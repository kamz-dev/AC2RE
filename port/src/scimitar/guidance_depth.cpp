#include "scimitar/guidance_depth.h"

#include <algorithm>
#include <cmath>

#include "port/world.h"

namespace scimitar {

namespace {
constexpr float kEpsilon = 0.0005f;

// The compiled float -> u32 conversion of max(x, 0) (truncation).
std::uint32_t Trunc(float x) { return x > 0.0f ? static_cast<std::uint32_t>(x) : 0u; }

bool IsDegenerate(const Vec3& a, const Vec3& b) {
    return std::fabs(b.x - a.x) <= kEpsilon && std::fabs(b.y - a.y) <= kEpsilon && std::fabs(b.z - a.z) <= kEpsilon;
}

// IntersectRangeV (Mac 0x261AE0): each pair sorted; the overlap of [lo1, hi1] and [lo2, hi2].
bool IntersectRangeV(float lo1, float hi1, float lo2, float hi2, float& outLo, float& outHi) {
    if (hi1 < lo1) std::swap(lo1, hi1);
    if (hi2 < lo2) std::swap(lo2, hi2);
    if (!(lo1 <= hi2 && hi1 >= lo2)) return false;
    outLo = std::max(lo1, lo2);
    outHi = std::min(hi1, hi2);
    return true;
}

// GuidanceWorkspacePlane::SetMatrix_Callback (Mac 0x48FE30): Y normalised, X = Y x Z, Z = X x Y.
Gear::Matrix44 OrthonormalisePlane(const Gear::Matrix44& m) {
    Gear::Matrix44 r;
    r.m_Rows[1] = normalized(m.m_Rows[1]);
    r.m_Rows[0] = normalized(cross(r.m_Rows[1], m.m_Rows[2]));
    r.m_Rows[2] = normalized(cross(r.m_Rows[0], r.m_Rows[1]));
    r.m_Rows[3] = m.m_Rows[3];
    return r;
}

// Matrix44::SetRotate(axis, angle) (Mac 0x9F62F0): the right-handed rotation (rows: the turned basis).
Gear::Matrix44 Rotation(const Vec3& axis, float angle) {
    Gear::Matrix44 r;
    r.m_Rows[0] = RotateVector({1, 0, 0}, axis, angle);
    r.m_Rows[1] = RotateVector({0, 1, 0}, axis, angle);
    r.m_Rows[2] = RotateVector({0, 0, 1}, axis, angle);
    return r;
}

// GuidanceDepthDetector::AddTasksToWorkspace (Mac 0x3E8D50): the strip of one line for one settings entry.
GuidanceWorkspacePlane MakePlane(const GuidanceContactObject& c, const GuidanceDepthDetectorSettings& s,
                                 float overshoot, const Gear::Matrix44& workspaceInverse) {
    const Vec3& nL = c.m_LineGlobalNormalLeft;
    const Vec3& nR = c.m_LineGlobalNormalRight;
    const Vec3 nSide = nR.z <= nL.z ? nR : nL;  // the lower one: the side face
    const Vec3 nTop = nR.z <= nL.z ? nL : nR;
    const Vec3 d = c.m_LineGlobalPoint1 - c.m_LineGlobalPoint0;
    const float len = std::sqrt(dot(d, d));
    const Vec3 dir = d * (1.0f / len);
    Vec3 perp = normalized(Vec3{dir.y, -dir.x, 0});  // dir x K
    if (0.0f > dot(nSide, perp)) perp = perp * -1.0f;
    const Vec3 v113 = normalized(cross(perp, nTop));
    const float sign = dot(dir, v113) < 0.0f ? -1.0f : 1.0f;
    const Vec3 pxd = cross(perp, dir);
    const Vec3 zAxis = pxd * -sign;
    const Vec3 offW = perp * s.m_Offset.y + zAxis * s.m_Offset.z + v113 * s.m_Offset.x;
    Gear::Matrix44 b;  // columns (pxd, perp, dir)
    b.m_Rows[0] = pxd;
    b.m_Rows[1] = perp;
    b.m_Rows[2] = dir;
    b.m_Rows[3] = {};
    Gear::Matrix44 p = b * Rotation(dir, sign * s.m_Angle);  // P = R . B: B first
    p.m_Rows[3] = offW + c.m_LineGlobalPoint0 - dir * overshoot;
    GuidanceWorkspacePlane plane;
    plane.m_Matrix = OrthonormalisePlane(p * workspaceInverse);  // Winv . P
    plane.m_Offset = offW;
    plane.m_Length = s.m_MaxDepth;
    plane.m_Height = len + (overshoot + overshoot);
    plane.m_Parent = &c;
    return plane;
}
}  // namespace

bool IntersectFinitePlaneWithTriangleEx(const Gear::Matrix44& plane, float length, float height, const Vec3& v0,
                                        const Vec3& v1, const Vec3& v2, float d0, float d1, float d2, Vec3& a,
                                        Vec3& b) {
    const float dMax = std::max({d0, d1, d2});
    const float dMin = std::min({d0, d1, d2});
    if (dMax * dMin >= 0.0f) return false;  // a vertex on the plane: no cut
    const Vec3& T = plane.m_Rows[3];
    const Vec3& Y = plane.m_Rows[1];
    const Vec3& Z = plane.m_Rows[2];
    const bool m14 = 0.0f < d0 * d1;
    const bool m15 = 0.0f < d2 * d1;
    const Vec3& lone = m15 ? v0 : (m14 ? v2 : v1);
    const float dl = m15 ? d0 : (m14 ? d2 : d1);
    const Vec3& A = m15 ? v1 : v0;
    const float dA = m15 ? d1 : d0;
    const Vec3& B = m15 ? v2 : (m14 ? v1 : v2);
    const float dB = m15 ? d2 : (m14 ? d1 : d2);
    const Vec3 P0 = lone + (A - lone) * (-dl / (dA - dl));
    const Vec3 P1 = lone + (B - lone) * (-dl / (dB - dl));
    const Vec3 D = P1 - P0;
    // Along Y to [0, length]. The game guards the far point's zero coordinates (0x1C644F), not the denominator.
    Vec3 EY = Y * length + T;
    if (EY.x == 0.0f) EY.x = kEpsilon;
    if (EY.y == 0.0f) EY.y = kEpsilon;
    if (EY.z == 0.0f) EY.z = kEpsilon;
    const float denY = dot(Y, D);
    float lo, hi;
    if (!IntersectRangeV(0.0f, 1.0f, dot(EY - P0, Y) / denY, dot(T - P0, Y) / denY, lo, hi)) return false;
    // Along Z to [0, height].
    float denZ = dot(Z, D);
    if (denZ == 0.0f) denZ = kEpsilon;
    if (!IntersectRangeV(lo, hi, dot(Z * height + T - P0, Z) / denZ, dot(T - P0, Z) / denZ, lo, hi)) return false;
    a = P0 + D * lo;
    b = P0 + D * hi;
    return true;
}

// --- GuidanceDepthTrack ---

void GuidanceDepthTrack::SetupTrack(const Gear::Matrix44& workspace, const GuidanceWorkspacePlane& plane,
                                    float resolution, float overshoot) {
    const std::uint32_t n = Trunc(plane.m_Height / resolution) + 1;
    m_Depths.assign(n, plane.m_Length);
    m_Resolution = resolution;
    m_Length = plane.m_Height;
    m_Overshoot = overshoot;
    m_GlobalOffset = plane.m_Offset;
    m_PlaneMatrix = plane.m_Matrix * workspace;  // W . plane
    m_InvPlaneMatrix = m_PlaneMatrix.GetAffineInverse();
}

void GuidanceDepthTrack::AddIntersection(float t, const Vec3& axis, const GuidanceContactObject& contact) {
    m_Intersections.push_back({t, dot(contact.m_LineGlobalNormalLeft, axis) <= 0.0f ? 1u : 0u, 0});
}

void GuidanceDepthTrack::AddSegment(const GuidanceContactObject& contact) {
    const Vec3 a = m_InvPlaneMatrix.TransformVertex(contact.m_LineGlobalPoint0);
    const Vec3 b = m_InvPlaneMatrix.TransformVertex(contact.m_LineGlobalPoint1);
    float z0, y0, z1, y1;
    if (b.z <= a.z) {
        z0 = b.z, y0 = b.y, z1 = a.z, y1 = a.y;
    } else {
        z0 = a.z, y0 = a.y, z1 = b.z, y1 = b.y;
    }
    const float L = m_Length;
    float zc0 = z0 > 0.0f ? z0 : 0.0f;
    zc0 = L > zc0 ? zc0 : L;
    float zc1 = z1 > 0.0f ? z1 : 0.0f;
    zc1 = L > zc1 ? zc1 : L;
    float a0;
    if (std::fabs(y1) <= kEpsilon) {
        y1 = 0.0f;
        if (std::fabs(y0) > kEpsilon) AddIntersection(zc1, m_PlaneMatrix.m_Rows[2], contact);
    }
    a0 = std::fabs(y0);
    if (a0 <= kEpsilon) {
        if (std::fabs(y1) > kEpsilon) AddIntersection(zc0, m_PlaneMatrix.m_Rows[2], contact);
        y0 = 0.0f;
    }
    const std::uint32_t n = static_cast<std::uint32_t>(m_Depths.size());
    const std::uint32_t i0 = std::min(Trunc(zc0 / m_Resolution), n - 1);
    const std::uint32_t i1 = std::min(Trunc(zc1 / m_Resolution), n - 1);
    if (m_Depths[i0] > y0) m_Depths[i0] = y0;
    if (m_Depths[i1] > y1) m_Depths[i1] = y1;
    if (i1 - i0 > 1) {  // unsigned
        const float slope = (y1 - y0) / (zc1 - zc0);
        for (std::uint32_t k = i0 + 1; k < i1; ++k) {
            const float y = ((static_cast<float>(k) + 0.5f) * m_Resolution) * slope + (y0 - zc0 * slope);
            if (m_Depths[k] > y) m_Depths[k] = y;
        }
    }
}

void GuidanceDepthTrack::ProcessIntersections() {
    if (m_Depths.empty()) return;
    std::vector<GuidanceIntersection>& a = m_Intersections;
    a.push_back({0.0f, 0, 0});
    a.push_back({m_Length, 1, 0});
    std::stable_sort(a.begin(), a.end(),
                     [](const GuidanceIntersection& p, const GuidanceIntersection& q) { return p.m_T < q.m_T; });
    // Opposite intersections at the same place cancel (the array keeps its order on removal).
    size_t i = 0;
    while (i < a.size()) {
        while (i + 1 < a.size() && a[i].m_Direction != a[i + 1].m_Direction && a[i].m_T == a[i + 1].m_T)
            a.erase(a.begin() + static_cast<long>(i), a.begin() + static_cast<long>(i) + 2);
        if (i >= a.size()) break;
        if (i > 0 && a[i].m_Direction != a[i - 1].m_Direction && a[i].m_T == a[i - 1].m_T) {
            a.erase(a.begin() + static_cast<long>(i) - 1, a.begin() + static_cast<long>(i) + 1);
            i -= 1;
        } else {
            i += 1;
        }
    }
    // Levels.
    long level = 0, minLevel = 0;
    for (size_t k = 1; k < a.size(); ++k) {
        if (a[k].m_Direction == 0 && a[k - 1].m_Direction == 0)
            level -= 1;
        else if (a[k].m_Direction == 1 && a[k - 1].m_Direction == 1)
            level += 1;
        a[k].m_Level = level;
        minLevel = std::min(minLevel, level);
    }
    // The valid spans: an entry (0) then an exit (1), both at the lowest level.
    std::vector<GuidanceClearanceInterval> intervals;
    if (a.size() >= 2) {  // the game reads a[0], a[1] even with fewer (unreachable: two sentinels are pushed)
        size_t k = 0;
        for (;;) {
            size_t next = k + 1;
            if (a[k].m_Direction == 0 && a[k + 1].m_Direction == 1 && a[k].m_Level == minLevel &&
                a[k + 1].m_Level == minLevel) {
                intervals.push_back({a[k].m_T, a[k + 1].m_T});
                next = k + 2;
            }
            if (a.size() - 1 <= next) break;
            k = next;
        }
    }
    std::vector<bool> mask;
    GetMaskFromValidIntervals(intervals, mask);
    for (size_t b = 0; b < m_Depths.size() && b < mask.size(); ++b)
        if (!mask[b]) m_Depths[b] = 0.0f;
}

void GuidanceDepthTrack::GetMaskFromValidIntervals(const std::vector<GuidanceClearanceInterval>& intervals,
                                                   std::vector<bool>& mask) const {
    mask.assign(Trunc(m_Length / m_Resolution) + 1, false);
    for (const GuidanceClearanceInterval& iv : intervals) {
        for (std::uint32_t b = Trunc(iv.m_T0 / m_Resolution); b <= Trunc(iv.m_T1 / m_Resolution); ++b)
            if (b < mask.size()) mask[b] = true;  // PORT: the game doesn't clamp (in range for valid spans)
    }
}

void GuidanceDepthTrack::GetClearanceIntervals(const std::vector<bool>& mask,
                                               std::vector<GuidanceClearanceInterval>& intervals) const {
    const size_t n = mask.size();
    size_t b = 0;
    while (b < n) {
        if (!mask[b]) {
            ++b;
            continue;
        }
        size_t e = b + 1;
        while (e < n && mask[e]) ++e;
        intervals.push_back({static_cast<float>(b) * m_Resolution, static_cast<float>(e) * m_Resolution});
        b = e + 1;
    }
}

void GuidanceDepthTrack::GetOvershootClearances(float clearance, float& c0, float& c1) const {
    const float ov = m_Overshoot;
    c0 = c1 = ov;
    if (ov == 0.0f || m_Depths.empty()) return;
    const std::uint32_t n = static_cast<std::uint32_t>(m_Depths.size());
    const std::uint32_t i = Trunc(ov / m_Resolution);
    if (clearance > m_Depths[i]) {
        c0 = 0.0f;
    } else {
        for (std::uint32_t k = 1; k <= i; ++k)
            if (clearance > m_Depths[i - k]) {
                c0 = static_cast<float>(k) * m_Resolution;
                break;
            }
    }
    const std::uint32_t j = Trunc((m_Length - ov) / m_Resolution) + 1;
    if (j < n) {
        if (clearance > m_Depths[j]) {
            c1 = 0.0f;
        } else {
            for (std::uint32_t k = 1; j + k < n; ++k)
                if (clearance > m_Depths[j + k]) {
                    c1 = static_cast<float>(k) * m_Resolution;
                    break;
                }
        }
    }
}

void GuidanceDepthTrack::GetClearanceReportFromIntervals(const std::vector<GuidanceClearanceInterval>& intervals,
                                                         const GuidanceContactObject& contact,
                                                         std::vector<GuidanceContactObject>& out) const {
    const Vec3 d = contact.m_LineGlobalPoint1 - contact.m_LineGlobalPoint0;
    const Vec3 dir = d * (1.0f / std::sqrt(dot(d, d)));
    const float ov = m_Overshoot;
    const Vec3 origin = contact.m_LineGlobalPoint0 - dir * ov;
    const float hi = m_Length - ov;
    GuidanceContactObject piece = contact;
    for (const GuidanceClearanceInterval& iv : intervals) {
        float a = iv.m_T0 > ov ? iv.m_T0 : ov;
        a = hi > a ? a : hi;
        float b = iv.m_T1 > ov ? iv.m_T1 : ov;
        b = hi > b ? b : hi;
        if (std::fabs(a - b) <= kEpsilon) continue;
        piece.m_LineGlobalPoint0 = origin + dir * a;
        piece.m_LineGlobalPoint1 = origin + dir * b;
        if (IsDegenerate(piece.m_LineGlobalPoint0, piece.m_LineGlobalPoint1)) continue;
        out.push_back(piece);
    }
}

void GuidanceDepthTrack::FillClearanceInfo(std::uint32_t track, float clearance, GuidanceContactObject& piece) const {
    const Vec3 a = m_InvPlaneMatrix.TransformVertex(piece.m_LineGlobalPoint0 + m_GlobalOffset);
    const Vec3 b = m_InvPlaneMatrix.TransformVertex(piece.m_LineGlobalPoint1 + m_GlobalOffset);
    const bool swapped = a.z > b.z;
    const float zLo = std::min(a.z, b.z), zHi = std::max(a.z, b.z);
    float c0 = zLo, c1 = m_Length - zHi;
    const std::uint32_t n = static_cast<std::uint32_t>(m_Depths.size());
    const std::uint32_t iLo = Trunc(zLo / m_Resolution);
    const std::uint32_t iHi = Trunc(zHi / m_Resolution) + 1;
    if (iLo < n) {  // PORT: the game doesn't bound iLo
        if (clearance > m_Depths[iLo]) {
            c0 = 0.0f;
        } else {
            for (std::uint32_t k = 1; k <= iLo; ++k)
                if (clearance > m_Depths[iLo - k]) {
                    c0 = static_cast<float>(k) * m_Resolution;
                    break;
                }
        }
    }
    if (iHi < n) {
        if (clearance > m_Depths[iHi]) {
            c1 = 0.0f;
        } else {
            for (std::uint32_t k = 1; iHi + k < n; ++k)
                if (clearance > m_Depths[iHi + k]) {
                    c1 = static_cast<float>(k) * m_Resolution;
                    break;
                }
        }
    }
    if (swapped) std::swap(c0, c1);
    if (2 * track + 1 < 4) {
        piece.m_Floats[2 * track] = c0;
        piece.m_Floats[2 * track + 1] = c1;
    }
}

// --- GuidanceDepthDetector ---

void GuidanceDepthDetector::GetDepthReports(const std::vector<GuidanceContactObject>& report,
                                            const port::World& world, std::vector<GuidanceDepthReport>& out) const {
    if (report.empty()) return;
    // The workspace: the endpoints' AABB grown by the largest MaxDepth, centred.
    Vec3 min{3.4028235e38f, 3.4028235e38f, 3.4028235e38f};
    Vec3 max{-3.4028235e38f, -3.4028235e38f, -3.4028235e38f};
    for (const GuidanceContactObject& c : report) {
        for (const Vec3& p : {c.m_LineGlobalPoint0, c.m_LineGlobalPoint1}) {
            if (min.x > p.x) min.x = p.x;
            if (min.y > p.y) min.y = p.y;
            if (min.z > p.z) min.z = p.z;
            if (p.x > max.x) max.x = p.x;
            if (p.y > max.y) max.y = p.y;
            if (p.z > max.z) max.z = p.z;
        }
    }
    float grow = 0.0f;
    for (const GuidanceDepthDetectorSettings& s : m_Settings) grow = std::max(s.m_MaxDepth, grow);
    min = min - Vec3{grow, grow, grow};
    max = max + Vec3{grow, grow, grow};
    const Vec3 centre = min + (max - min) * 0.5f;
    const Gear::Matrix44 W = Gear::Matrix44::Translation(centre);
    const Gear::Matrix44 Winv = W.GetAffineInverse();

    // AddTasksToWorkspace: one task (a list of strips) per settings entry, one strip per line.
    std::vector<std::vector<GuidanceWorkspacePlane>> tasks(m_Settings.size());
    for (size_t i = 0; i < m_Settings.size(); ++i)
        for (const GuidanceContactObject& c : report) tasks[i].push_back(MakePlane(c, m_Settings[i], m_Overshoot, Winv));

    // BroadPhase (layer 5) and ProcessTriangles2: every mesh triangle crossing a strip's plane, clipped to the strip.
    struct Mesh {
        Gear::Matrix44 global;
        const port::World::CollisionMesh* mesh;
    };
    std::vector<Mesh> meshes;
    world.ForEachCollisionMesh(min, max, [&](const Gear::Matrix44& g, const port::World::CollisionMesh& m) {
        meshes.push_back({g, &m});
    });
    std::vector<std::vector<GuidanceContactObject>> results(m_Settings.size());
    for (size_t i = 0; i < tasks.size(); ++i) {
        for (const GuidanceWorkspacePlane& plane : tasks[i]) {
            for (const Mesh& m : meshes) {
                // PORT: the MOPP query over the strip's AABB is replaced by every triangle (a superset).
                const Gear::Matrix44 local = plane.m_Matrix * W * m.global.GetAffineInverse();
                const Vec3 X = local.m_Rows[0];
                const float k0 = -dot(X, local.m_Rows[3]);
                const auto& v = m.mesh->vertices;
                const auto& idx = m.mesh->indices;
                for (size_t t = 0; t + 2 < idx.size(); t += 3) {
                    const Vec3& v0 = v[idx[t]];
                    const Vec3& v1 = v[idx[t + 1]];
                    const Vec3& v2 = v[idx[t + 2]];
                    const float d0 = dot(X, v0) + k0, d1 = dot(X, v1) + k0, d2 = dot(X, v2) + k0;
                    const bool s0 = d0 >= 0.0f, s1 = d1 >= 0.0f, s2 = d2 >= 0.0f;
                    if (s0 && s1 && s2) continue;
                    if (!(s0 || s1 || s2)) continue;
                    Vec3 a, b;
                    if (!IntersectFinitePlaneWithTriangleEx(local, plane.m_Length, plane.m_Height, v0, v1, v2, d0, d1,
                                                            d2, a, b))
                        continue;
                    GuidanceContactObject slice;
                    slice.m_LineGlobalPoint0 = m.global.TransformVertex(a);
                    slice.m_LineGlobalPoint1 = m.global.TransformVertex(b);
                    Vec3 n = cross(v1 - v0, v2 - v0);
                    if (dot(n, n) != 0.0f) n = n * (1.0f / std::sqrt(dot(n, n)));
                    n = m.global.TransformVector(n);
                    slice.m_LineGlobalNormalLeft = slice.m_LineGlobalNormalRight = n;
                    slice.m_ParentContactObject = plane.m_Parent;
                    slice.m_SubType = 0;  // the task's m_SubType
                    if (IsDegenerate(slice.m_LineGlobalPoint0, slice.m_LineGlobalPoint1)) continue;
                    results[i].push_back(slice);
                }
            }
        }
    }

    // FillReports: a track per line, the slices binned into their line's track, then the intersections.
    out.assign(m_Settings.size(), GuidanceDepthReport{});
    for (size_t i = 0; i < tasks.size(); ++i) {
        out[i].m_Tracks.resize(tasks[i].size());
        for (size_t j = 0; j < tasks[i].size(); ++j)
            out[i].m_Tracks[j].SetupTrack(W, tasks[i][j], m_Resolution, m_Overshoot);
    }
    for (size_t i = 0; i < results.size(); ++i) {
        if (results[i].empty()) continue;
        for (const GuidanceContactObject& c : results[i])
            out[i].m_Tracks[static_cast<size_t>(c.m_ParentContactObject - report.data())].AddSegment(c);
        for (GuidanceDepthTrack& track : out[i].m_Tracks) track.ProcessIntersections();
    }
}

void GuidanceDepthDetector::GetCombinedGuidanceReport(const GuidanceContactObject& contact,
                                                      const std::vector<const GuidanceDepthTrack*>& tracks,
                                                      const std::vector<float>& clearances,
                                                      std::vector<GuidanceContactObject>& out) const {
    const size_t n = tracks.size();
    std::vector<GuidanceClearanceInfo> infos(n);
    std::vector<bool> mask;
    const GuidanceDepthTrack* ref = nullptr;
    for (size_t k = 0; k < n; ++k) {
        const float c = clearances[k];
        const GuidanceDepthTrack& t = *tracks[k];
        t.GetOvershootClearances(c, infos[k].m_Clearance0, infos[k].m_Clearance1);
        if (!t.m_Depths.empty()) {
            if (!ref) {
                mask.assign(t.m_Depths.size(), true);
                ref = &t;
            }
            for (size_t b = 0; b < t.m_Depths.size() && b < mask.size(); ++b)
                if (c > t.m_Depths[b]) mask[b] = false;
        }
    }
    if (!mask.empty()) {
        const size_t start = out.size();
        std::vector<GuidanceClearanceInterval> intervals;
        ref->GetClearanceIntervals(mask, intervals);
        ref->GetClearanceReportFromIntervals(intervals, contact, out);
        for (size_t p = start; p < out.size(); ++p)
            for (size_t k = 0; k < n; ++k)
                tracks[k]->FillClearanceInfo(static_cast<std::uint32_t>(k), clearances[k], out[p]);
        return;
    }
    GuidanceContactObject piece = contact;
    for (size_t k = 0; k < n && 2 * k + 1 < 4; ++k) {
        piece.m_Floats[2 * k] = infos[k].m_Clearance0;
        piece.m_Floats[2 * k + 1] = infos[k].m_Clearance1;
    }
    if (!IsDegenerate(piece.m_LineGlobalPoint0, piece.m_LineGlobalPoint1)) out.push_back(piece);
}

void CastGuidanceDepthDetector(const std::vector<GuidanceContactObject>& report, const port::World& world,
                               std::vector<GuidanceDepthReport>& out, float maxDepth, float angle, const Vec3& offset) {
    GuidanceDepthDetector detector;
    detector.m_Resolution = 0.1f;  // HumanGuidance::SetupDepthDetector(0.1, 0.5)
    detector.m_Overshoot = 0.5f;
    detector.m_Settings.push_back({offset, angle, maxDepth});
    detector.GetDepthReports(report, world, out);
}

void CreateOccludedGuidanceReport(const std::vector<GuidanceContactObject>& report,
                                  const std::vector<GuidanceDepthReport>& depthReports, float clearance,
                                  std::vector<GuidanceContactObject>& out) {
    GuidanceDepthDetector detector;  // only its GetCombinedGuidanceReport (stateless here)
    const std::vector<float> clearances{clearance};
    for (size_t i = 0; i < report.size(); ++i) {
        std::vector<const GuidanceDepthTrack*> tracks;
        for (const GuidanceDepthReport& d : depthReports)
            if (i < d.m_Tracks.size()) tracks.push_back(&d.m_Tracks[i]);
        detector.GetCombinedGuidanceReport(report[i], tracks, clearances, out);
    }
}

}  // namespace scimitar
