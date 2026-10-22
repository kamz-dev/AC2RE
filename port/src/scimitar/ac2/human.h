#pragma once
// scimitar::ac2::Human: the character. PC: Human::Update 0x933480 (Mac __ZN8scimitar3ac25Human6UpdateEv 0x12EDA06).

#include <array>
#include <iterator>
#include <memory>

#include <vector>

#include "scimitar/ac2/edge_clamping.h"
#include "scimitar/ac2/human_context.h"
#include "scimitar/ac2/human_data.h"
#include "scimitar/math.h"

namespace scimitar::ac2 {

class HumanGround;
class HumanInAir;

// scimitar::ac2::IHuman: the interface others use to reach the Human. In the game it is a secondary base at
// Human +0x100 (PC vftable 0x19E69B0, Mac 0x2842CA8); its slots match on both (byte offsets in brackets). The port has
// the slots it uses so far.
class IHuman {
public:
    virtual ~IHuman() = default;
    virtual void SetEdgeClamping(bool enable) = 0;      // [+0x6C] Mac 0x161C68A: m_IsEdgeClampingEnable
    virtual bool IsEdgeClampingZeroSpeed() const = 0;   // [+0x70] Mac 0x161C6AA
    virtual void SetGap(const Gap& gap) = 0;            // [+0x80] PC 0x913A70, Mac 0x12C4EBE
    virtual const Gap& GetGap() const = 0;              // [+0x84] Mac 0x12C40BE
    virtual void SetIsGapValid(bool valid) = 0;         // [+0x88] PC 0x913AF0, Mac 0x12C40CC
    virtual bool IsGapValid() const = 0;                // [+0x8C] Mac 0x12C40E4
    virtual void SetNearestGap(const Gap& gap) = 0;     // [+0x90] Mac 0x161EF76
    virtual void SetIsNearestGapValid(bool valid) = 0;  // [+0x94] Mac 0x161C664
    // [+0x98] PC 0x9282B0, Mac 0x12D4BD6. The guidance edges within `radius` of `position` with a drop of at least
    // `minDepth` in front; with `allowNoEdge`, the drop under `position` itself when there are none.
    virtual bool GetGapList(std::vector<Gap>& gaps, const Vec3& position, float radius, float minDepth,
                            bool allowNoEdge) const = 0;
    // [+0x9C] PC 0x91F1F0, Mac 0x12CE79C. Within `maxDistance` (flat), the gap whose edge normal is closest to `dir`,
    // and no further than `maxAngle` from it.
    virtual bool GetBestGapInDirectionInList(Gap& out, const std::vector<Gap>& gaps, const Vec3& position,
                                             const Vec3& dir, float maxDistance, float maxAngle) const = 0;
    // [+0xA0] Mac 0x12C6EFC. The gap whose edge is closest (flat), within `maxDistance`.
    virtual bool GetClosestGapInList(Gap& out, const std::vector<Gap>& gaps, const Vec3& position,
                                     float maxDistance) const = 0;
    virtual float GetWaterLevel() const = 0;                         // [+0xA8]
    virtual EdgeClampingSettings& GetEdgeClampingSettings() = 0;     // [+0x170] Mac 0x161C8CA: Human +0x9F0
};

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
    std::array<std::unique_ptr<ActorContext>, kHumanContextBundleSlotCount> m_Contexts;  // game slot order
};

class Human : public IHuman {
public:
    Human();
    ~Human();

    // Mac Human::Update() takes no arguments: the game reads input and time from its own systems.
    // PORT: the port passes them in.
    void Update(const port::MoveInput& input, const port::World& world, float dt);

    // PORT: stands in for AIActor::GetPhysicWorkspace (the world the queries go to). Human::Update sets it; the Sim
    // sets it before the controller's first frame.
    void SetPortWorld(const port::World* world) { m_PortWorld = world; }
    const port::World* GetPortWorld() const { return m_PortWorld; }

    // IHuman
    void SetEdgeClamping(bool enable) override { m_IsEdgeClampingEnable = enable; }
    bool IsEdgeClampingZeroSpeed() const override { return m_IsEdgeClampingZeroSpeed; }
    void SetGap(const Gap& gap) override { m_Gap = gap; }
    const Gap& GetGap() const override { return m_Gap; }
    void SetIsGapValid(bool valid) override { m_IsGapValid = valid; }
    bool IsGapValid() const override { return m_IsGapValid; }
    void SetNearestGap(const Gap& gap) override { m_NearestGap = gap; }
    void SetIsNearestGapValid(bool valid) override { m_IsNearestGapValid = valid; }
    bool GetGapList(std::vector<Gap>& gaps, const Vec3& position, float radius, float minDepth,
                    bool allowNoEdge) const override;
    bool GetBestGapInDirectionInList(Gap& out, const std::vector<Gap>& gaps, const Vec3& position, const Vec3& dir,
                                     float maxDistance, float maxAngle) const override;
    bool GetClosestGapInList(Gap& out, const std::vector<Gap>& gaps, const Vec3& position,
                             float maxDistance) const override;
    // PORT: the port has no water; the level is the lowest float, so every edge is above it.
    float GetWaterLevel() const override { return -3.4028235e38f; }
    EdgeClampingSettings& GetEdgeClampingSettings() override { return m_EdgeClampingSettings; }

    // Mac 0x12D0AD2, PC 0x9251D0 (outlined only). PORT: a stand-in: the ground just past the edge (`frontOffset`
    // + 0.05 along the normal), or straight down from `edge` with a null normal; `maxDepth` when there is none. The
    // game casts rays (layers 12 and 35) along the normal turned down 50 degrees and adds the deepest gap found where
    // they land.
    float GetDepthInFrontOfEdge(const Vec3& position, const Vec3& edge, const Vec3& edgeNormal, float maxDepth,
                                float frontOffset) const;

    // The edge-clamping flags (Revelations PDB names; PC offsets). The ground states that move freely set
    // m_IsEdgeClampingActive on Enter and clear it on Exit (Wait, MoveCycle, MoveStop, CrouchMove, EdgeStop,
    // ObstacleCollision, Poisoned, CarryObject, Dodge; RE/11_ground.md 16); the controller sets
    // m_IsEdgeClampingEnable through SetEdgeClamping.
    bool m_IsEdgeClampingActive = false;       // +0x991
    bool m_IsFightEdgeClampingActive = false;  // +0x992 set by a fight state (PC 0xF55950); not ported
    bool m_IsEdgeClampingEnable = false;       // +0x993
    bool m_IsEdgeClampingZeroSpeed = false;    // +0x994 the clamp left no displacement this frame
    bool m_EdgeClampUsedLastFrame = false;     // +0x995
    bool m_ForcedGapUpdate = false;            // +0x996 EdgeClampingComputation searches the gap itself (fight)
    bool m_GapUpdateBlocked = false;           // +0x921 (unnamed) blocks that search; not ported

    // PORT: the frame's root motion, in world space: what the current action would move the character by this frame
    // (the game's ActionComponent displacement, Action::GetDisplacementWithTime). The context sets it in its update;
    // Human::Update applies it after EdgeClampingComputation.
    Vec3 m_PortRootMotion;

    HumanContextBundle& GetContextBundle() { return m_ContextBundle; }
    HumanContextId GetCurrentContextID() const { return m_CurrentContext; }
    HumanContextId GetPreviousContextID() const { return m_PreviousContext; }
    float GetTimeInContext() const { return m_TimeInContext; }

    HumanGroundData& GetGroundData() { return m_GroundData; }
    const HumanGroundData& GetGroundData() const { return m_GroundData; }
    const HumanInAirData& GetInAirData() const { return m_InAirData; }
    HumanInAirData& GetInAirData() { return m_InAirData; }
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
    // PORT: the action component's current action (channel 0). Only the landing hand-over sets it so far.
    std::uint32_t m_PortCurrentActionId = 0;

private:
    void SetPortHeading(const PortHeadingSettings& heading);  // PORT: the controller's heading setters
    // PC 0x931330, Mac 0x12ED4B6: replaces the frame's root motion by its clamped version near a gap.
    void EdgeClampingComputation();
    // PC 0x930540, Mac 0x12ED290: clamps `displacement` against the edges near the gap; false when nothing applies.
    bool GetModifiedSpeed(Vec3& displacement);

    const port::World* m_PortWorld = nullptr;
    EdgeClampingSettings m_EdgeClampingSettings;  // +0x9F0
    Gap m_Gap;
    Gap m_NearestGap;
    bool m_IsGapValid = false;
    bool m_IsNearestGapValid = false;

    HumanContextBundle m_ContextBundle{*this};
    HumanContextId m_CurrentContext = HumanContextId::Ground;
    HumanContextId m_PreviousContext = HumanContextId::Ground;
    HumanContextId m_RequestedContext = HumanContextId::None;
    float m_TimeInContext = 0;
    HumanGroundData m_GroundData;
    HumanInAirData m_InAirData;
    PortGroundSpeeds m_PortGroundSpeeds;
    PortActionLengths m_PortActionLengths;
};

}  // namespace scimitar::ac2
