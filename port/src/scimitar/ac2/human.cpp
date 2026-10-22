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
    m_CurrentContext = m_PreviousContext = id;
    m_RequestedContext = HumanContextId::None;
    m_TimeInContext = 0;
}

void Human::Update(const port::MoveInput& input, const port::World& world, float dt) {
    // The player's controller (AssassinTask, port/src/scimitar/ac2/assassin_task.h) or an AI drives the contexts
    // through their interfaces before this runs.
    m_PortWorld = &world;
    m_RequestedContext = HumanContextId::None;
    m_PortRootMotion = {};
    ActorContext* ctx = m_ContextBundle.Get(m_CurrentContext);
    if (ctx) ctx->Update(input, world, dt);
    // PC 0x933480: after the actor's update (the contexts), EdgeClampingComputation, then UpdateUnbalanceIgnoreList
    // (PC 0x92E420; not ported). PORT: the physics step follows as the context's after-physics phase.
    EdgeClampingComputation();
    if (ctx) ctx->UpdateAfterPhysics(world, dt);
    m_TimeInContext += dt;

    // PORT: the game's switching goes through its FSMs and interface commands; not decoded yet.
    HumanContextId next = m_RequestedContext;
    if (next == HumanContextId::None || next == m_CurrentContext) return;
    ActorContext* to = m_ContextBundle.Get(next);
    if (!to) return;  // a context the port doesn't have yet: stay
    if (ActorContext* from = m_ContextBundle.Get(m_CurrentContext)) from->Exit();
    m_PreviousContext = m_CurrentContext;
    m_CurrentContext = next;
    m_TimeInContext = 0;
    to->Enter();
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

float Human::GetDepthInFrontOfEdge(const Vec3& position, const Vec3& edge, const Vec3& edgeNormal, float maxDepth,
                                   float frontOffset) const {
    (void)position;
    if (!m_PortWorld) return maxDepth;
    const Vec3 n = normalized(Vec3{edgeNormal.x, edgeNormal.y, 0});
    const Vec3 probe = edge + n * (frontOffset + 0.05f);
    const std::optional<float> ground = m_PortWorld->groundBelow(probe.x, probe.y, edge.z);
    if (!ground) return maxDepth;
    const float depth = edge.z - *ground;
    return depth < maxDepth ? depth : maxDepth;
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
        const float depth = GetDepthInFrontOfEdge(position, hit, fn, 7.0f, 0.02f);
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
    const float depth = GetDepthInFrontOfEdge(position, position, Vec3{}, 7.0f, 0.02f);
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
