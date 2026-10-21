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

Human::Human() = default;
Human::~Human() = default;

void Human::ForceContext(HumanContextId id) {
    m_CurrentContext = m_PreviousContext = id;
    m_RequestedContext = HumanContextId::Count;
    m_TimeInContext = 0;
}

void Human::Update(const port::MoveInput& input, const port::World& world, float dt) {
    // PORT: the game fills HumanGroundData from its controller / action interpreter (not decoded); the port copies
    // the input in. LIVE rows 3 and 5: which buttons set +0xEE and +0x181.
    m_GroundData.m_MoveDirection = input.moveDir;
    m_GroundData.m_StickMagnitude = input.moveMag;
    m_GroundData.m_HighProfileRequest = input.held(port::kHighProfile);
    m_GroundData.m_SpeedBandUp = input.held(port::kLegs);

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
