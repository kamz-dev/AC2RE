#pragma once
// The Human contexts. Mac: scimitar::ac2::Human::HumanContextBundle has one getter per context
// (GetGround 0x12C4950 ... GetUpperBody 0x12C815E) and creates some on demand
// (CreateContextOnDemandHelper<HumanSwim> 0x161DC08).

#include <cstdint>
#include <iterator>
#include <string_view>

namespace port {
struct MoveInput;
class World;
}  // namespace port

namespace scimitar::ac2 {

class Human;

// The game's context ids. Each context's ActorContextData holds its id at +4: AIActor::GetContextByID (Mac 0x2AF660)
// compares it, and Human::AddContextToMainAITrack (Mac 0x12CD0AC) switches on it. The ids of Ground, Vocalization,
// LookAt and UpperBody aren't in that switch; they were read live (RE/20_live_checks.md row 9). 0-2, 18 and 21 are
// no Human context's.
enum class HumanContextId : std::uint8_t {
    DebugContext = 3,     // debug
    Ground = 4,           // movement (live)
    Ladder = 5,           // movement
    InAir = 6,            // movement
    Ledge = 7,            // movement
    Climb = 8,            // movement
    Walling = 9,          // movement
    NarrowObject = 10,    // movement (beams, poles)
    Swim = 11,            // movement
    Riding = 12,          // out of scope (horse)
    Boat = 13,            // out of scope
    Cart = 14,            // out of scope
    GliderPilot = 15,     // out of scope
    Wagon = 16,           // out of scope
    CannonOperator = 17,  // out of scope
    RagdollGround = 19,   // out of scope
    Vocalization = 20,    // presentation (live)
    CarriedBody = 22,     // out of scope
    LookAt = 23,          // presentation (live)
    Kiosk = 24,           // movement (market stalls / hiding spots)
    Dead = 25,            // out of scope
    HayStack = 26,        // movement
    UpperBody = 27,       // presentation (live)
    None = 0xFF,          // PORT: no context (no request pending)
};

// Human::HumanContextBundle's slots, in the game's order (Mac getters GetGround 0x12C4950 ... GetUpperBody 0x12C815E:
// slot = their offset / 4; the PC layout is the same, verified through the live contexts' LocalInits, RE/20 row 9).
// The bundle sits at Human +0x440 on both builds.
inline constexpr HumanContextId kHumanContextBundleSlots[] = {
    HumanContextId::Ground,         HumanContextId::Ladder,        HumanContextId::InAir,
    HumanContextId::Ledge,          HumanContextId::Climb,         HumanContextId::Walling,
    HumanContextId::Riding,         HumanContextId::Boat,          HumanContextId::GliderPilot,
    HumanContextId::Cart,           HumanContextId::Wagon,         HumanContextId::CannonOperator,
    HumanContextId::Kiosk,          HumanContextId::Dead,          HumanContextId::HayStack,
    HumanContextId::DebugContext,   HumanContextId::Vocalization,  HumanContextId::LookAt,
    HumanContextId::NarrowObject,   HumanContextId::Swim,          HumanContextId::UpperBody,
    HumanContextId::RagdollGround,  HumanContextId::CarriedBody,
};
inline constexpr int kHumanContextBundleSlotCount = static_cast<int>(std::size(kHumanContextBundleSlots));

// The bundle slot of a context, or -1.
constexpr int HumanContextBundleSlot(HumanContextId id) {
    for (int i = 0; i < kHumanContextBundleSlotCount; ++i)
        if (kHumanContextBundleSlots[i] == id) return i;
    return -1;
}

std::string_view HumanContextName(HumanContextId id);

}  // namespace scimitar::ac2

namespace scimitar {

// The contexts' engine base class (Mac scimitar::ActorContext; HumanGround's primary base, RE/11_ground.md 1). Its
// vftable in the game: dtor, CreateAI, LocalInit, GlobalInit(World*), TriggerEnter, Shutdown(World*),
// UpdateAfterPhysics, ReleaseResources, QueryInterface, GetActorContextData (x2), SetAIActorData, Enter, Exit,
// Update(long), LinkFSM, GetCurrentStateName. The game's contexts also derive from
// DataContext<Human, HumanData, *Data>.
// PORT: only Enter / Exit / Update so far. The game's Update(long) takes the update phase (0 = main, 1 = after
// physics; HumanGround::_UpdateHumanGround PC 0x6381D0) and reads input and time from its own systems; the port
// passes them in. GetID and the Human reference stand in for the DataContext's typed accessors.
class ActorContext {
public:
    explicit ActorContext(ac2::Human& human) : m_Human(human) {}
    virtual ~ActorContext() = default;
    virtual ac2::HumanContextId GetID() const = 0;
    virtual void Enter() {}
    virtual void Exit() {}
    virtual void Update(const port::MoveInput& input, const port::World& world, float dt) = 0;

protected:
    ac2::Human& m_Human;
};

}  // namespace scimitar
