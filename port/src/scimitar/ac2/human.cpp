#include "scimitar/ac2/human.h"

#include "port/input.h"
#include "scimitar/ac2/human_ground.h"
#include "scimitar/ac2/human_in_air.h"

namespace scimitar::ac2 {

std::string_view HumanContextName(HumanContextId id) {
    static constexpr std::string_view kNames[] = {
        "Ground", "InAir", "Ledge", "Climb", "Walling", "Ladder", "NarrowObject", "HayStack",
        "Kiosk", "Swim", "Riding", "Boat", "GliderPilot", "Cart", "Wagon", "CannonOperator",
        "CarriedBody", "Dead", "RagdollGround", "LookAt", "Vocalization", "UpperBody", "DebugContext",
    };
    static_assert(std::size(kNames) == static_cast<size_t>(HumanContextId::Count));
    auto i = static_cast<size_t>(id);
    return i < std::size(kNames) ? kNames[i] : "?";
}

HumanContextBundle::~HumanContextBundle() = default;

template <class T>
T& HumanContextBundle::CreateContextOnDemand(HumanContextId id) {
    auto& slot = m_Contexts[static_cast<size_t>(id)];
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
    // PORT: stands in for the player's controller, which sets the heading rotation through IHumanGround.
    IHumanGround& ground = m_ContextBundle.GetGround();
    ground.SetHeadingMinRotationSpeed(heading.minRotationSpeed);
    ground.SetHeadingMaxRotationSpeed(heading.maxRotationSpeed);
    ground.SetHeadingMinRotationAngle(heading.minRotationAngle);
    ground.SetHeadingMaxRotationAngle(heading.maxRotationAngle);
}
Human::~Human() = default;

void Human::ForceContext(HumanContextId id) {
    m_CurrentContext = m_PreviousContext = id;
    m_RequestedContext = HumanContextId::Count;
    m_TimeInContext = 0;
}

void Human::Update(const port::MoveInput& input, const port::World& world, float dt) {
    // PORT: the game's controller / action interpreter (not decoded) sets HumanGroundData's DestHeading and
    // DestSpeedRatio; the port copies the input's world direction and stick magnitude in. Live (RE/20 row 5,
    // 2026-10-21): high profile alone sets Sprint (target 0.4, a jog); high profile + legs also sets +0x181 (target
    // 1, running), which was never seen without Sprint. Whether legs alone sets +0x181 is still open (row 5).
    const bool highProfile = input.held(port::kHighProfile);
    m_GroundData.m_DestHeading = input.moveDir;
    m_GroundData.m_DestSpeedRatio = input.moveMag;
    m_GroundData.m_HighProfileRequest = highProfile && input.held(port::kLegs);
    IHumanGround& ground = m_ContextBundle.GetGround();
    ground.SetSprint(highProfile);
    SetPortHeading(highProfile ? kPortHeadingSprint : kPortHeadingNormal);  // live row 6: follows Sprint
    // PORT: who calls SetBreakHard for the player isn't decoded (LIVE row 7). Live it is set while DestSpeedRatio is
    // 0 and cleared ~0.1 s after a move starts; the port follows the stick without that lag.
    ground.SetBreakHard(input.moveMag <= 0.0f);

    m_RequestedContext = HumanContextId::Count;
    if (ActorContext* ctx = m_ContextBundle.Get(m_CurrentContext)) ctx->Update(input, world, dt);
    m_TimeInContext += dt;

    // PORT: the game's switching goes through its FSMs and interface commands; not decoded yet.
    HumanContextId next = m_RequestedContext;
    if (next == HumanContextId::Count || next == m_CurrentContext) return;
    ActorContext* to = m_ContextBundle.Get(next);
    if (!to) return;  // a context the port doesn't have yet: stay
    if (ActorContext* from = m_ContextBundle.Get(m_CurrentContext)) from->Exit();
    m_PreviousContext = m_CurrentContext;
    m_CurrentContext = next;
    m_TimeInContext = 0;
    to->Enter();
}

}  // namespace scimitar::ac2
