#pragma once
// scimitar::ac2::Human: the character. PC: Human::Update 0x933480 (Mac __ZN8scimitar3ac25Human6UpdateEv 0x12EDA06).

#include <array>
#include <iterator>
#include <memory>

#include "scimitar/ac2/human_context.h"
#include "scimitar/ac2/human_data.h"
#include "scimitar/math.h"

namespace scimitar::ac2 {

class HumanGround;
class HumanInAir;

// Mac: scimitar::ac2::Human::HumanContextBundle (ctor 0x12C374E, dtor 0x12C4360). Owns the contexts and creates
// them on demand (CreateContextOnDemandHelper<T> 0x161DC08).
class HumanContextBundle {
public:
    explicit HumanContextBundle(Human& human) : m_Human(human) {}
    ~HumanContextBundle();

    HumanGround& GetGround();  // Mac 0x12C4950
    HumanInAir& GetInAir();    // Mac 0x12C490C
    // Not implemented yet: GetLedge 0x12C48EA, GetClimb 0x12C48C8, GetWalling 0x12C48A6, GetLadder 0x12C492E,
    // GetNarrowObject 0x12C46A8, GetHayStack 0x12C4774, GetKiosk 0x12C47B8, GetSwim 0x12C4686.

    ActorContext* Get(HumanContextId id);  // PORT: by id, for the port's switching

private:
    template <class T>
    T& CreateContextOnDemand(HumanContextId id);

    Human& m_Human;
    std::array<std::unique_ptr<ActorContext>, static_cast<size_t>(HumanContextId::Count)> m_Contexts;
};

class Human {
public:
    Human();
    ~Human();

    // Mac Human::Update() takes no arguments: the game reads input and time from its own systems.
    // PORT: the port passes them in.
    void Update(const port::MoveInput& input, const port::World& world, float dt);

    HumanContextBundle& GetContextBundle() { return m_ContextBundle; }
    HumanContextId GetCurrentContextID() const { return m_CurrentContext; }
    HumanContextId GetPreviousContextID() const { return m_PreviousContext; }
    float GetTimeInContext() const { return m_TimeInContext; }

    HumanGroundData& GetGroundData() { return m_GroundData; }
    const HumanGroundData& GetGroundData() const { return m_GroundData; }
    const HumanInAirData& GetInAirData() const { return m_InAirData; }
    const PortGroundSpeeds& GetPortGroundSpeeds() const { return m_PortGroundSpeeds; }
    const PortActionLengths& GetPortActionLengths() const { return m_PortActionLengths; }

    // A context asks to hand over to another one; the switch happens at the end of the update.
    void RequestContext(HumanContextId id) { m_RequestedContext = id; }
    // PORT: puts the human in a context without running Exit / Enter (spawning, teleports, tests).
    void ForceContext(HumanContextId id);

    // PORT: stand-ins for the entity's matrix and the character component's velocity (AC1: entity +0x10 matrix,
    // CharacterComponent +0x90 velocity; AC2 offsets go in sdk/ once found).
    Vec3 m_Position;
    Vec3 m_Velocity;
    float m_Yaw = 0;

private:
    void SetPortHeading(const PortHeadingSettings& heading);  // PORT: the controller's heading setters

    HumanContextBundle m_ContextBundle{*this};
    HumanContextId m_CurrentContext = HumanContextId::Ground;
    HumanContextId m_PreviousContext = HumanContextId::Ground;
    HumanContextId m_RequestedContext = HumanContextId::Count;
    float m_TimeInContext = 0;
    HumanGroundData m_GroundData;
    HumanInAirData m_InAirData;
    PortGroundSpeeds m_PortGroundSpeeds;
    PortActionLengths m_PortActionLengths;
};

}  // namespace scimitar::ac2
