#include "scimitar/ac2/human.h"

#include <cmath>
#include <optional>

#include "port/input.h"
#include "port/world.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/ac2/human_in_air.h"

namespace scimitar::ac2 {

std::string_view HumanContextName(HumanContextId id) {
    switch (id) {
        case HumanContextId::DebugContext: return "DebugContext";
        case HumanContextId::Ground: return "Ground";
        case HumanContextId::Ladder: return "Ladder";
        case HumanContextId::InAir: return "InAir";
        case HumanContextId::Ledge: return "Ledge";
        case HumanContextId::Climb: return "Climb";
        case HumanContextId::Walling: return "Walling";
        case HumanContextId::NarrowObject: return "NarrowObject";
        case HumanContextId::Swim: return "Swim";
        case HumanContextId::Riding: return "Riding";
        case HumanContextId::Boat: return "Boat";
        case HumanContextId::Cart: return "Cart";
        case HumanContextId::GliderPilot: return "GliderPilot";
        case HumanContextId::Wagon: return "Wagon";
        case HumanContextId::CannonOperator: return "CannonOperator";
        case HumanContextId::RagdollGround: return "RagdollGround";
        case HumanContextId::Vocalization: return "Vocalization";
        case HumanContextId::CarriedBody: return "CarriedBody";
        case HumanContextId::LookAt: return "LookAt";
        case HumanContextId::Kiosk: return "Kiosk";
        case HumanContextId::Dead: return "Dead";
        case HumanContextId::HayStack: return "HayStack";
        case HumanContextId::UpperBody: return "UpperBody";
        case HumanContextId::None: break;
    }
    return "?";
}

HumanContextBundle::~HumanContextBundle() = default;

template <class T>
T& HumanContextBundle::CreateContextOnDemand(HumanContextId id) {
    auto& slot = m_Contexts[static_cast<size_t>(HumanContextBundleSlot(id))];
    if (!slot) slot = std::make_unique<T>(m_Human);
    return static_cast<T&>(*slot);
}

HumanGround& HumanContextBundle::GetGround() { return CreateContextOnDemand<HumanGround>(HumanContextId::Ground); }
HumanInAir& HumanContextBundle::GetInAir() { return CreateContextOnDemand<HumanInAir>(HumanContextId::InAir); }

ActorContext* HumanContextBundle::Get(HumanContextId id) {
    switch (id) {
        case HumanContextId::Ground: return &GetGround();
        case HumanContextId::InAir: return &GetInAir();
        default: return nullptr;  // not implemented yet
    }
}

Human::Human() {
    SetPortHeading(kPortHeadingNormal);
    SetInitialContext(kMainTrack, HumanContextId::Ground, [this] { InitMainTrack(); });
    AIActor::Enter();
}

ActorContext* Human::GetContextByID(long track, HumanContextId id) {
    // PORT: only the main track has contexts in the port.
    return track == kMainTrack ? m_ContextBundle.Get(id) : nullptr;
}

void Human::InitMainTrack() {
    TransitionSetupDataToMovement{HumanGroundData::MovementState_WaitLow, 0.0f, 0.5f,
                                  TransitionToMotionType_BlendWalkToJog}
        .SetupTransitionData(m_GroundData);
}

void Human::SetPortHeading(const PortHeadingSettings& heading) {
    // PORT: the initial heading rotation. The player's controller sets the speeds every ground frame
    // (AssassinTask::UpdateGroundMovement); who sets the angles isn't decoded (live values, RE/20 row 6).
    IHumanGround& ground = m_ContextBundle.GetGround();
    ground.SetHeadingMinRotationSpeed(heading.minRotationSpeed);
    ground.SetHeadingMaxRotationSpeed(heading.maxRotationSpeed);
    ground.SetHeadingMinRotationAngle(heading.minRotationAngle);
    ground.SetHeadingMaxRotationAngle(heading.maxRotationAngle);
}
Human::~Human() = default;

void Human::ForceContext(HumanContextId id) {
    if (ActorContext* context = m_ContextBundle.Get(id)) PortForceContext(kMainTrack, context);
    m_Tracks[kMainTrack].m_PreviousContextID = id;
    m_PortTimeInContext = 0;
    m_PortTimedContext = GetCurrentContext();
}

void Human::Update(const port::MoveInput& input, const port::World& world, float dt) {
    // The player's controller (AssassinTask, port/src/scimitar/ac2/assassin_task.h) or an AI drives the contexts
    // through their interfaces before this runs.
    m_PortWorld = &world;
    m_PortRootMotion = {};
    const PortUpdateArgs args{input, world, dt};
    // AIActor::Update (PC 0x7AC40): every track's Update(0). A context switches inside its own update or inside a
    // controller's command (AIActor::TransitToContext, at once); a track that switched before this is skipped once.
    UpdateTracks(0, args);
    // Then EdgeClampingComputation and UpdateUnbalanceIgnoreList (PC 0x92E420; not ported).
    EdgeClampingComputation();
    // PORT: the physics step (the root motion applied, the ground test) is the main track's Update(1), which the game
    // runs from Human::UpdateAfterPhysics (PC 0x922390 -> AIActor::UpdateAfterPhysics PC 0x7AC50) after physics.
    AIActor::UpdateAfterPhysics(args);

    if (GetCurrentContext() != m_PortTimedContext) {
        m_PortTimedContext = GetCurrentContext();
        m_PortTimeInContext = 0;
    } else {
        m_PortTimeInContext += dt;
    }
}

// --- Gaps and edge clamping (RE/11_ground.md 16) ---

namespace {
bool IsNullWithEpsilon(const Vec3& v) {  // Gear::IsNullWithEpsilon, PC 0x2B3130: every lane within 0.001
    return std::fabs(v.x) <= 0.001f && std::fabs(v.y) <= 0.001f && std::fabs(v.z) <= 0.001f;
}
bool NullWithEpsilon(const Vec3& v, float eps) {
    return std::fabs(v.x) <= eps && std::fabs(v.y) <= eps && std::fabs(v.z) <= eps;
}

// IntersectSegmentWithInfinitePlane (Mac 0x1C2E50, PC 0x10FA690).
bool IntersectSegmentWithInfinitePlane(Vec3& hit, const Vec3& p0, const Vec3& p1, const Vec3& planePoint,
                                       const Vec3& planeNormal) {
    const float den = dot(p1 - p0, planeNormal);
    if (den == 0.0f) return false;
    const float t = dot(planePoint - p0, planeNormal) / den;
    if (t < 0.0f || t > 1.0f) return false;
    hit = p0 * (1.0f - t) + p1 * t;
    return true;
}
}  // namespace

namespace {
// CollisionFilterInfo::Layer values the depth casts use (Revelations names). Human::GetLayer (Mac 0x12C3898; inlined
// on PC) turns 12 into 22 when the entity's +0xA8 & 7 is 1 (the player, hypothesis).
constexpr int kLayerCharacterNoInteractive = 12;
constexpr int kLayerOcclusionHeightTest = 35;

// GetDepthInFrontOfEdge's slope test of a ray's contact (PC 0x92567D / 0x925DC4): 0 when the normal is +-K within
// `eps`, else Gear::GetAngle(K, normal, K x flat), the slope rising away from `flat` (the flat normal, normalised).
float SlopeAngle(const Vec3& normal, float eps, Vec3& flat, Vec3& axis) {
    const Vec3 k = Gear::kBaseK;
    if (NullWithEpsilon(normal - k, eps) || NullWithEpsilon(normal + k, eps)) return 0.0f;
    flat = normalized(Vec3{normal.x, normal.y, 0});
    axis = cross(k, flat);
    return Gear::GetAngle(k, normal, axis);
}
}  // namespace

float Human::GetDepthInFrontOfEdge(const Vec3& position, const Vec3& edge, const Vec3& edgeNormal, float maxDepth,
                                   float frontOffset, float gapRadius) const {
    if (!m_PortWorld) return maxDepth;  // PORT: no world, nothing to cast against
    const port::World& world = *m_PortWorld;
    const Vec3 k = Gear::kBaseK;
    const Vec3 down = k * -1.0f;
    // The radius of the gap search where a ray lands grows from gapRadius to at least 0.2 over the first 3 m.
    const float maxGapRadius = gapRadius < 0.2f ? 0.2f : gapRadius;
    const bool nullNormal = NullWithEpsilon(edgeNormal, 0.0005f);
    Vec3 dir = edgeNormal;
    if (!nullNormal) dir = normalized(Vec3{edgeNormal.x, edgeNormal.y, 0});

    // 1. The edge itself (PC 0x9252D9): a ray from 0.05 behind the edge, turned 50 degrees down, to `frontOffset` past
    //    it at the edge's height; then one from 1 m above `position` to that point. Something in the way is the depth.
    if (!nullNormal) {
        const Vec3 front = edge + dir * frontOffset;
        const Vec3 back = front - dir * (frontOffset + 0.05f);
        const Vec3 ray = RotateVector(dir, cross(dir, k), -0.87266463f);
        const Vec3 origin = ProjectPointOnPlane(front, back, dir, ray);
        if (const auto c = world.rayCast(origin, ray, length(front - origin), kLayerCharacterNoInteractive))
            return edge.z - c->position.z;
        const Vec3 head = position + k;
        const float headLength = length(front - head);
        if (const auto c = world.rayCast(head, (front - head) * (1.0f / headLength), headLength,
                                         kLayerCharacterNoInteractive))
            return edge.z - c->position.z;
    }

    // Where a ray lands (PC 0x925AEB): the drop to it, plus the deepest gap within the search radius around it.
    // Returns false (the result and the retry flag unchanged) when gaps are found but none is the deepest.
    float result = maxDepth;
    bool retry = false;
    auto searchRadius = [&](float drop) {
        float t = drop / 3.0f;
        if (t <= 0.0f) t = 0.0f;
        if (t >= 1.0f) t = 1.0f;
        return (maxGapRadius - gapRadius) * t + gapRadius;
    };
    auto land = [&](const port::World::ContactPoint& c) {
        const float drop = edge.z - c.position.z;
        const float radius = searchRadius(drop);
        std::vector<Gap> gaps;
        float depth = drop;
        if (radius > 0.01f && GetGapList(gaps, c.position, radius, 0.45f, false)) {
            Gap deepest;
            if (!GetDeepestGapInList(deepest, gaps)) return;
            depth = deepest.m_Depth + drop;
        }
        retry = false;
        result = depth;
    };

    // 2. The first ray down (PC 0x925584).
    Vec3 axis;  // the turn axis of the rays down
    Vec3 origin, ray, end;
    float rayLength;
    if (!nullNormal) {
        // From `frontOffset` past the edge, down turned 45 degrees out over the drop, to 0.5 m (at most maxDepth)
        // below the edge.
        axis = cross(k, edgeNormal);
        ray = RotateVector(down, axis, -0.78539819f);
        origin = edge + dir * frontOffset;
        Vec3 plane = edge;
        plane.z -= maxDepth <= 0.5f ? maxDepth : 0.5f;
        end = ProjectPointOnPlane(origin, plane, k, ray);
        rayLength = length(origin - end);
    } else {
        // Straight down from 0.5 m above `position` to 0.5 m below it.
        origin = position + k * 0.5f;
        ray = down;
        end = position + down * 0.5f;
        rayLength = length(origin - end);
        if (const auto c = world.rayCast(origin, ray, rayLength, kLayerOcclusionHeightTest)) {
            Vec3 flat;
            Vec3 slopeAxis;
            if (SlopeAngle(c->normal, 0.0005f, flat, slopeAxis) <= 0.78539819f) {
                const float drop = edge.z - c->position.z;
                const float radius = searchRadius(drop);
                std::vector<Gap> gaps;
                if (radius <= 0.01f || !GetGapList(gaps, c->position, radius, 0.45f, false)) return drop;
                Gap deepest;
                if (GetDeepestGapInList(deepest, gaps)) return deepest.m_Depth + drop;
                // No deepest gap: on to the loop, which casts the same ray again.
            } else {
                // Too steep: slide off it, 0.01 out from the contact, down turned 45 degrees away from the slope, to
                // 0.5 m below the contact.
                axis = slopeAxis;
                origin = c->position + flat * 0.01f;
                ray = RotateVector(down, axis, -0.78539819f);
                Vec3 plane = c->position;
                plane.z -= 0.5f;
                end = ProjectPointOnPlane(origin, plane, k, ray);
                rayLength = length(origin - end);
            }
        }
    }

    // 3. The loop (PC 0x925A93): cast the ray; when it is clear, cast on from its end, turned 10 degrees out (straight
    //    down for the first one under a null normal), to maxDepth below the edge. A contact on a slope steeper than 45
    //    degrees slides off it as above and casts again; any other contact lands. Clear all the way: maxDepth.
    bool straightDown = nullNormal;
    for (;;) {
        if (const auto c = world.rayCast(origin, ray, rayLength, kLayerOcclusionHeightTest)) {
            land(*c);
            if (!retry) break;
            continue;
        }
        origin = end;
        ray = straightDown ? down : RotateVector(down, axis, -0.17453292f);
        Vec3 plane = edge;
        plane.z -= maxDepth;
        end = ProjectPointOnPlane(origin, plane, k, ray);
        rayLength = length(origin - end);
        straightDown = false;
        const auto c = world.rayCast(origin, ray, rayLength, kLayerOcclusionHeightTest);
        if (!c) break;
        Vec3 flat;
        Vec3 slopeAxis;
        if (SlopeAngle(c->normal, 0.001f, flat, slopeAxis) <= 0.78539819f) {
            land(*c);
            if (!retry) break;
        } else {
            axis = slopeAxis;
            origin = c->position + flat * 0.01f;
            ray = RotateVector(down, axis, -0.78539819f);
            Vec3 slidePlane = c->position;
            slidePlane.z -= 0.5f;
            end = ProjectPointOnPlane(origin, slidePlane, k, ray);
            rayLength = length(origin - end);
            retry = true;
        }
    }
    return result;
}

bool Human::GetDeepestGapInList(Gap& out, const std::vector<Gap>& gaps) const {
    float deepest = -3.4028235e38f;
    int index = -1;
    for (size_t i = 0; i < gaps.size(); ++i) {
        if (gaps[i].m_Depth > deepest) {
            deepest = gaps[i].m_Depth;
            index = static_cast<int>(i);
        }
    }
    if (index == -1) return false;
    out = gaps[static_cast<size_t>(index)];
    return true;
}

bool Human::GetGapList(std::vector<Gap>& gaps, const Vec3& position, float radius, float minDepth,
                       bool allowNoEdge) const {
    gaps.clear();
    if (!m_PortWorld) return false;
    std::vector<port::GuidanceLine> lines;
    m_PortWorld->CastGuidanceSphere(position, radius, lines);  // a GuidanceZoneSphere on `position`
    bool shallowSeen = false;
    for (const port::GuidanceLine& c : lines) {
        if (!IsEdgeLeveled(c.p0, c.p1)) continue;
        const Vec3 n = normalized(Vec3{c.p0.x - c.p1.x, c.p0.y - c.p1.y, 0});  // the edge's flat direction
        Vec3 hit;
        if (!IntersectSegmentWithInfinitePlane(hit, c.p0, c.p1, position, n)) continue;  // the foot of `position`
        if (!(std::fabs(position.z - hit.z) < 0.3f)) continue;
        const Vec3 fn = GetLineGlobalFlatNormal(c.p0, c.p1, c.faceNormal0, c.faceNormal1);
        if (NullWithEpsilon(fn, 0.0005f)) continue;
        const float depth = GetDepthInFrontOfEdge(position, hit, fn, 7.0f, 0.02f, 0.0f);
        if (minDepth > depth) {
            shallowSeen = true;
            continue;
        }
        Gap g;
        g.m_EdgePosition = hit;
        g.m_EdgeNormal = fn;
        g.m_Depth = depth;
        g.m_SquareDistance = dot(hit - position, hit - position);
        if (dot(fn, position - hit) > 0.0f) g.m_SquareDistance = -g.m_SquareDistance;  // already past the edge
        g.m_NoEdge = false;
        g.m_EdgeSubType = c.subType & 0x3F;
        gaps.push_back(g);
    }
    if (!gaps.empty()) return true;
    if (!allowNoEdge || shallowSeen) return false;
    // No edge: the drop under `position` itself.
    const float depth = GetDepthInFrontOfEdge(position, position, Vec3{}, 7.0f, 0.02f, 0.0f);
    if (minDepth > depth) return false;
    Gap g;
    g.m_EdgePosition = position;
    g.m_EdgeNormal = normalized(Vec3{forwardFromYaw(m_Yaw).x, forwardFromYaw(m_Yaw).y, 0});  // the flat sight
    g.m_Depth = depth;
    g.m_SquareDistance = 0;
    g.m_NoEdge = true;
    g.m_EdgeSubType = 0;
    gaps.push_back(g);
    return true;
}

bool Human::GetBestGapInDirectionInList(Gap& out, const std::vector<Gap>& gaps, const Vec3& position, const Vec3& dir,
                                        float maxDistance, float maxAngle) const {
    float best = 3.4028235e38f;
    int index = -1;
    const float maxD2 = maxDistance * maxDistance;
    for (size_t i = 0; i < gaps.size(); ++i) {
        Vec3 d = position - gaps[i].m_EdgePosition;
        d.z = 0;
        if (!(maxD2 > dot(d, d))) continue;
        const float a = Gear::GetAngleAbsolute(dir, gaps[i].m_EdgeNormal);
        if (maxAngle >= a && best > a) {
            index = static_cast<int>(i);
            best = a;
        }
    }
    if (index == -1) return false;
    out = gaps[static_cast<size_t>(index)];
    return true;
}

bool Human::GetClosestGapInList(Gap& out, const std::vector<Gap>& gaps, const Vec3& position,
                                float maxDistance) const {
    int index = -1;
    float best = maxDistance * maxDistance;
    for (size_t i = 0; i < gaps.size(); ++i) {
        Vec3 d = position - gaps[i].m_EdgePosition;
        d.z = 0;
        const float d2 = dot(d, d);
        if (best > d2) {
            index = static_cast<int>(i);
            best = d2;
        }
    }
    if (index == -1) return false;
    out = gaps[static_cast<size_t>(index)];
    return true;
}

bool Human::GetModifiedSpeed(Vec3& displacement) {
    const float len = length(displacement);
    if (len < 0.0005f) {
        displacement = {};
        return true;
    }
    std::vector<Segment> segments;  // InplaceArray<Segment, 32>
    const Vec3 dirN = normalized(displacement);
    const Gap& gap = GetGap();
    if (!GetEdgeSegmentNearCharacterPos(*m_PortWorld, m_Position, forwardFromYaw(m_Yaw), gap.m_EdgePosition,
                                        gap.m_EdgeNormal, dirN, len + 0.8f, m_EdgeClampingSettings, segments, 32))
        return false;
    Vec3 out;
    if (!DisplacementCullingBySegment(out, m_Position, displacement, m_EdgeClampingSettings.m_EdgeDistance, segments))
        return false;
    displacement = out;
    return true;
}

void Human::EdgeClampingComputation() {
    m_IsEdgeClampingZeroSpeed = false;
    if (m_PortWorld && m_IsEdgeClampingActive && (m_IsEdgeClampingEnable || m_IsFightEdgeClampingActive)) {
        if (!m_GapCalculated && m_ForcedGapUpdate) {
            std::vector<Gap> gaps;
            GetGapList(gaps, m_Position, 0.75f, 0.45f, true);
            Gap gap;
            const bool valid =
                GetBestGapInDirectionInList(gap, gaps, m_Position, forwardFromYaw(m_Yaw), 0.75f, 3.1415927f);
            SetIsGapValid(valid);
            if (valid) SetGap(gap);
        }
        if (IsGapValid()) {
            // The current action's displacement over this frame, to world space. PORT: the root motion stand-in is
            // already in world space.
            const Vec3 local = m_PortRootMotion;
            Vec3 world = m_PortRootMotion;
            if (!IsNullWithEpsilon(local) && GetModifiedSpeed(world)) {
                if (IsNullWithEpsilon(world)) m_IsEdgeClampingZeroSpeed = true;
                m_EdgeClampUsedLastFrame = true;
                // ActionComponent::SetDisplacementMode(2): the action no longer moves the character; the character
                // component is sent to position + the clamped displacement (when the action moves at all).
                m_PortRootMotion = dot(local, local) > 2.5e-7f ? world : Vec3{};
                return;
            }
        }
    }
    if (m_EdgeClampUsedLastFrame) {
        // ActionComponent::ResetDisplacement and displacement mode 0: the action's root motion applies again.
        m_EdgeClampUsedLastFrame = false;
    }
}

}  // namespace scimitar::ac2
