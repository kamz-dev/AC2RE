#pragma once
// The Human contexts. Mac: scimitar::ac2::Human::HumanContextBundle has one getter per context
// (GetGround 0x12C4950 ... GetUpperBody 0x12C815E) and creates some on demand
// (CreateContextOnDemandHelper<HumanSwim> 0x161DC08).

#include <cstdint>
#include <string_view>

namespace port {
struct MoveInput;
class World;
}  // namespace port

namespace scimitar::ac2 {

class Human;

// One per HumanContextBundle getter. PORT: the order is the port's; the game's context ids are not decoded yet.
enum class HumanContextId : std::uint8_t {
    Ground,          // GetGround        0x12C4950  movement
    InAir,           // GetInAir         0x12C490C  movement
    Ledge,           // GetLedge         0x12C48EA  movement
    Climb,           // GetClimb         0x12C48C8  movement
    Walling,         // GetWalling       0x12C48A6  movement
    Ladder,          // GetLadder        0x12C492E  movement
    NarrowObject,    // GetNarrowObject  0x12C46A8  movement (beams, poles)
    HayStack,        // GetHayStack      0x12C4774  movement
    Kiosk,           // GetKiosk         0x12C47B8  movement (market stalls / hiding spots)
    Swim,            // GetSwim          0x12C4686  movement
    Riding,          // GetRiding        0x12C4884  out of scope (horse)
    Boat,            // GetBoat          0x12C4862  out of scope
    GliderPilot,     // GetGliderPilot   0x12C4840  out of scope
    Cart,            // GetCart          0x12C481E  out of scope
    Wagon,           // GetWagon         0x12C47FC  out of scope
    CannonOperator,  // GetCannonOperator 0x12C47DA out of scope
    CarriedBody,     // GetCarriedBody   0x12C46EC  out of scope
    Dead,            // GetDead          0x12C4796  out of scope
    RagdollGround,   // GetRagdollGround 0x12C470E  out of scope
    LookAt,          // GetLookAt        0x12C4752  presentation
    Vocalization,    // GetVocalization  0x12C46CA  presentation
    UpperBody,       // GetUpperBody     0x12C815E  presentation
    DebugContext,    // GetDebugContext  0x12C4730  debug
    Count,
};

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
