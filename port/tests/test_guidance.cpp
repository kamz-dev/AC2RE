// Guidance (RE/19_guidance.md): the packed data, the EdgeFilter, the sphere and box casts in contact and cut modes.

#include <cmath>

#include "check.h"
#include "port/world.h"
#include "scimitar/guidance.h"

using scimitar::EdgeFilter;
using scimitar::GuidanceContactObject;
using scimitar::GuidanceObject;
using scimitar::GuidanceSystemComponent;
using scimitar::GuidanceZoneBox;
using scimitar::GuidanceZoneSphere;
using scimitar::Vec3;

namespace {
// A world holding the components it is given, for the casts' broadphase.
struct ComponentWorld : port::World {
    std::vector<GuidanceSystemComponent*> components;
    std::optional<float> groundBelow(float, float, float) const override { return std::nullopt; }
    void GetGuidanceSystemComponents(const Vec3&, const Vec3&,
                                     std::vector<const GuidanceSystemComponent*>& out) const override {
        for (auto* c : components) out.push_back(c);
    }
};

// One line from a to b (local), between a ground face (+Z) and a wall facing `wall`, LedgeGrab.
GuidanceSystemComponent Line(Vec3 a, Vec3 b, Vec3 wall = {0, 1, 0}, std::uint32_t subType = 1) {
    GuidanceSystemComponent c;
    GuidanceObject o;
    o.SetValid(true);
    o.SetSubType(subType);
    o.SetLineIndex0(c.PortAddPoint(a));
    o.SetLineIndex1(c.PortAddPoint(b));
    o.SetLineNormalLeft({0, 0, 1});
    o.SetLineNormalRight(wall);
    c.m_GuidanceObjects.push_back(o);
    c.m_EdgeFilter.m_Filter = EdgeFilter::EdgeWallGround;
    return c;
}

std::vector<GuidanceContactObject> Sphere(const ComponentWorld& w, Vec3 centre, float radius, bool cut = false,
                                          std::uint32_t mask = scimitar::kGuidanceContactObjectSubType_All) {
    GuidanceZoneSphere zone;
    zone.m_Matrix.m_Rows[3] = centre;
    zone.m_Radius = radius;
    std::vector<GuidanceContactObject> report;
    zone.CastWorld(report, cut, mask, w);
    return report;
}
}  // namespace

TEST(guidance_object_packing_matches_the_game) {
    // PC bits, LSB first: Valid 0, SubType 1-5, Index0 6-18, Index1 19-31 (GetLineIndex0/1 Mac 0xA4B360 / 0xA4B350).
    GuidanceObject o;
    o.SetValid(true);
    o.SetSubType(13);
    o.SetLineIndex0(0x1ABC);
    o.SetLineIndex1(0x0123);
    CHECK(o.m_Packing == (1u | (13u << 1) | (0x1ABCu << 6) | (0x0123u << 19)));
    CHECK(o.GetValid());
    CHECK(o.GetSubType() == 13);
    CHECK(o.GetLineIndex0() == 0x1ABC);
    CHECK(o.GetLineIndex1() == 0x0123);
}

TEST(dec_n4_normals_round_trip_at_511ths) {
    const Vec3 n = scimitar::normalized(Vec3{0.3f, -0.8f, 0.52f});
    const Vec3 back = scimitar::LoadDecN4(scimitar::StoreDecN4(n));
    CHECK_NEAR(back.x, n.x, 2.0f / 511.0f);
    CHECK_NEAR(back.y, n.y, 2.0f / 511.0f);
    CHECK_NEAR(back.z, n.z, 2.0f / 511.0f);
    // Exact values: -1 is -511 (0x201 in 10 bits), +1 is 511.
    CHECK(scimitar::StoreDecN4({1, -1, 0}) == (0x1FFu | (0x201u << 10)));
    CHECK_NEAR(scimitar::LoadDecN4(0x201).x, -1.0f, 1e-6f);
}

TEST(compressed_points_are_5_mm_steps_around_zero) {
    GuidanceSystemComponent c;
    const auto i = c.PortAddPoint({1.2345f, -163.84f, 0});
    CHECK(c.m_CompressPoints[0] == 33015);  // (1.2345 + 163.84) / 0.005, rounded
    CHECK(c.m_CompressPoints[1] == 0);
    const Vec3 p = c.GetPoint(i);
    CHECK_NEAR(p.x, 1.235f, 1e-3f);
    CHECK_NEAR(p.y, -163.84f, 1e-3f);
    CHECK_NEAR(p.z, 0.0f, 1e-3f);
}

TEST(edge_filter_classifies_faces_by_slope) {
    EdgeFilter f;  // Reset: no bits
    CHECK(!f.AcceptEdge({0, 0, 1}, {0, 1, 0}));
    f.m_Filter = EdgeFilter::EdgeWallGround;
    CHECK(f.AcceptEdge({0, 0, 1}, {0, 1, 0}));   // ground + wall
    CHECK(f.AcceptEdge({0, 1, 0}, {0, 0, 1}));   // wall + ground
    CHECK(!f.AcceptEdge({0, 1, 0}, {1, 0, 0}));  // wall + wall needs bit 1
    // A 40-degree slope is still ground (cos 40 = 0.77 > 0.707); 50 degrees is a wall.
    CHECK(f.AcceptEdge({0, std::sin(0.698f), std::cos(0.698f)}, {0, 1, 0}));
    CHECK(!f.AcceptEdge({0, std::sin(0.873f), std::cos(0.873f)}, {0, 1, 0}));
    f.m_Filter = EdgeFilter::EdgeWallWall;
    CHECK(f.AcceptEdge({0, 1, 0}, {1, 0, 0}));                  // 90 degrees > 22.5
    CHECK(!f.AcceptEdge({0, 1, 0}, {std::sin(0.3f), std::cos(0.3f), 0}));  // 17 degrees
    f.m_Filter = EdgeFilter::EdgeWallCeiling;
    CHECK(f.AcceptEdge({0, 0, -1}, {0, 1, 0}));
    CHECK(f.AcceptEdge({0, 1, 0}, {0, 0, -1}));
}

TEST(sphere_cast_takes_lines_strictly_within_the_radius) {
    ComponentWorld w;
    GuidanceSystemComponent c = Line({-5, 0, 0}, {5, 0, 0});
    w.components.push_back(&c);
    CHECK(Sphere(w, {0, 0.5f, 0}, 0.75f).size() == 1);
    CHECK(Sphere(w, {0, 0.75f, 0}, 0.75f).empty());  // exactly at the radius: no
    const GuidanceContactObject contact = Sphere(w, {0, 0.5f, 0}, 0.75f)[0];
    CHECK_NEAR(contact.m_LineGlobalPoint0.x, -5.0f, 1e-3f);
    CHECK_NEAR(contact.m_LineGlobalPoint1.x, 5.0f, 1e-3f);
    CHECK_NEAR(contact.m_LineGlobalNormalRight.y, 1.0f, 2e-3f);
    CHECK(contact.m_SubType == 1);
    CHECK(contact.m_Entity == &c);
}

TEST(sphere_cast_follows_the_entity_and_its_rotation) {
    ComponentWorld w;
    GuidanceSystemComponent c = Line({-1, 0, 0}, {1, 0, 0});
    // Turned 90 degrees about Z (X -> Y) and moved to (10, 0, 3): the line runs along Y at x = 10, its wall faces -X.
    c.m_PortGlobalMatrix.m_Rows[0] = {0, 1, 0};
    c.m_PortGlobalMatrix.m_Rows[1] = {-1, 0, 0};
    c.m_PortGlobalMatrix.m_Rows[3] = {10, 0, 3};
    w.components.push_back(&c);
    const auto report = Sphere(w, {10.2f, 0.5f, 3}, 0.5f);
    CHECK(report.size() == 1);
    CHECK_NEAR(report[0].m_LineGlobalPoint0.y, -1.0f, 1e-3f);
    CHECK_NEAR(report[0].m_LineGlobalPoint0.x, 10.0f, 1e-3f);
    CHECK_NEAR(report[0].m_LineGlobalNormalRight.x, -1.0f, 2e-3f);
    CHECK(Sphere(w, {0, 0, 3}, 0.5f).empty());
}

TEST(sphere_cast_filters_by_subtype_validity_filter_and_enable) {
    ComponentWorld w;
    GuidanceSystemComponent ledge = Line({-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, 1);
    GuidanceSystemComponent beam = Line({-1, 0.2f, 0}, {1, 0.2f, 0}, {0, 1, 0}, 2);
    w.components = {&ledge, &beam};
    CHECK(Sphere(w, {0, 0, 0}, 1).size() == 2);
    CHECK(Sphere(w, {0, 0, 0}, 1, false, 1u << 2).size() == 1);  // GuidanceContactObjectSubType_Beam
    beam.m_GuidanceObjects[0].SetValid(false);
    CHECK(Sphere(w, {0, 0, 0}, 1).size() == 1);
    ledge.m_EdgeFilter.m_Filter = 0;  // the reset filter takes nothing
    CHECK(Sphere(w, {0, 0, 0}, 1).empty());
    ledge.m_EdgeFilter.m_Filter = EdgeFilter::EdgeWallGround;
    ledge.m_PortCastEnabled = false;
    CHECK(Sphere(w, {0, 0, 0}, 1).empty());
}

TEST(sphere_cut_keeps_the_part_inside) {
    ComponentWorld w;
    GuidanceSystemComponent c = Line({-5, 0, 0}, {5, 0, 0});
    w.components.push_back(&c);
    // Radius 1 around (0.5, 0, 0): the line from x = -0.5 to 1.5.
    auto report = Sphere(w, {0.5f, 0, 0}, 1.0f, true);
    CHECK(report.size() == 1);
    CHECK_NEAR(report[0].m_LineGlobalPoint0.x, -0.5f, 1e-3f);
    CHECK_NEAR(report[0].m_LineGlobalPoint1.x, 1.5f, 1e-3f);
    // Starting inside: the cut starts at p0.
    report = Sphere(w, {-5, 0, 0}, 1.0f, true);
    CHECK(report.size() == 1);
    CHECK_NEAR(report[0].m_LineGlobalPoint0.x, -5.0f, 1e-3f);
    CHECK_NEAR(report[0].m_LineGlobalPoint1.x, -4.0f, 1e-3f);
}

TEST(box_cast_separating_axes_and_cut) {
    ComponentWorld w;
    GuidanceSystemComponent c = Line({-5, 0, 0}, {5, 0, 0});
    w.components.push_back(&c);
    GuidanceZoneBox box;
    box.m_Matrix.m_Rows[3] = {0, 0.5f, 0};
    box.m_Min = {-1, -1, -1};
    box.m_Max = {1, 1, 1};
    std::vector<GuidanceContactObject> report;
    CHECK(box.CastWorld(report, false, scimitar::kGuidanceContactObjectSubType_All, w));
    CHECK(report.size() == 1);
    CHECK(box.CastWorld(report, true, scimitar::kGuidanceContactObjectSubType_All, w));
    CHECK_NEAR(report[0].m_LineGlobalPoint0.x, -1.0f, 1e-3f);
    CHECK_NEAR(report[0].m_LineGlobalPoint1.x, 1.0f, 1e-3f);
    // Turned 45 degrees about Z: the box reaches sqrt(2) along X at y = 0.
    const float s = std::sqrt(0.5f);
    box.m_Matrix.m_Rows[0] = {s, s, 0};
    box.m_Matrix.m_Rows[1] = {-s, s, 0};
    box.m_Matrix.m_Rows[3] = {0, 0, 0};
    CHECK(box.CastWorld(report, true, scimitar::kGuidanceContactObjectSubType_All, w));
    CHECK_NEAR(report[0].m_LineGlobalPoint1.x, std::sqrt(2.0f), 1e-3f);
    // Moved away along Y past the corner: separated.
    box.m_Matrix.m_Rows[3] = {0, 1.5f, 0};
    CHECK(!box.CastWorld(report, false, scimitar::kGuidanceContactObjectSubType_All, w));
}

TEST(ray_aabb_is_woos_test) {
    Vec3 hit;
    CHECK(scimitar::IntersectRayAABBox({-3, 0.5f, 0.5f}, {1, 0, 0}, {0, 0, 0}, {1, 1, 1}, hit));
    CHECK_NEAR(hit.x, 0.0f, 1e-6f);
    CHECK_NEAR(hit.y, 0.5f, 1e-6f);
    CHECK(!scimitar::IntersectRayAABBox({-3, 0.5f, 0.5f}, {-1, 0, 0}, {0, 0, 0}, {1, 1, 1}, hit));  // behind
    CHECK(scimitar::IntersectRayAABBox({0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 0, 0}, {1, 1, 1}, hit));  // inside
    CHECK_NEAR(hit.x, 0.5f, 1e-6f);
}

TEST(box_world_carries_ledge_grab_guidance_on_each_box) {
    port::BoxWorld world;
    world.add({{-5, -5, 0}, {5, 5, 3}});
    GuidanceZoneSphere zone;
    zone.m_Matrix.m_Rows[3] = {0, 4.5f, 3};
    zone.m_Radius = 0.75f;
    std::vector<GuidanceContactObject> report;
    CHECK(zone.CastWorld(report, false, scimitar::kGuidanceContactObjectSubType_All, world));
    CHECK(report.size() == 1);  // only the north edge is within 0.75 m
    CHECK_NEAR(report[0].m_LineGlobalPoint0.y, 5.0f, 1e-3f);
    CHECK_NEAR(report[0].m_LineGlobalPoint0.z, 3.0f, 1e-3f);
    CHECK(report[0].m_SubType == scimitar::GuidanceObjectSubTypeLedgeGrab);
}
