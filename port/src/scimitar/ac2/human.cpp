#include "scimitar/ac2/human.h"

#include "port/input.h"
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
    m_RequestedContext = HumanContextId::None;
    m_TimeInContext = 0;
}

void Human::Update(const port::MoveInput& input, const port::World& world, float dt) {
    // PORT: the game's controller / action interpreter (not decoded) sets HumanGroundData's DestHeading and
    // DestSpeedRatio; the port copies the input's world direction and stick magnitude in. Live (RE/20 row 5,
    // session 2): legs (Space, the fast walk) sets Sprint (target 0.4); high profile (right mouse, the run) sets
    // +0x181 (target 0.8); both give the sprint (target 1). Each flag follows its own button.
    const bool sprint = input.held(port::kLegs);
    m_GroundData.m_DestHeading = input.moveDir;
    m_GroundData.m_DestSpeedRatio = input.moveMag;
    m_GroundData.m_HighProfileRequest = input.held(port::kHighProfile);
    IHumanGround& ground = m_ContextBundle.GetGround();
    ground.SetSprint(sprint);
    SetPortHeading(sprint ? kPortHeadingSprint : kPortHeadingNormal);  // live row 6: follows Sprint
    // PORT: who calls SetBreakHard for the player isn't decoded (LIVE row 7). Live it is set while DestSpeedRatio is
    // 0 and cleared ~0.1 s after a move starts; the port follows the stick without that lag.
    ground.SetBreakHard(input.moveMag <= 0.0f);

    m_RequestedContext = HumanContextId::None;
    if (ActorContext* ctx = m_ContextBundle.Get(m_CurrentContext)) ctx->Update(input, world, dt);
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

}  // namespace scimitar::ac2
