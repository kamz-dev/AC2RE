#pragma once
// scimitar::ac2::Human as it sits in the PC game: only the fields confirmed so far (RE/20_live_checks.md rows 9 and
// session 1). The rest is padding; the class's full size isn't known, so it isn't asserted.
//
// Finding the player's Human (RE/20, session 1): EngineContextImp::ms_Instance (0x1E10AC0) +0x4C = World;
// World::GetPlayer (0x3A300) = the entity; entity +0x74 -> +0x54 -> +0x2A4 = the Human.

#include <cstdint>

#include "ac2sdk/layout.h"

namespace ac2sdk {

// The game's context ids: each context's ActorContextData holds its id at +4 (Mac AIActor::GetContextByID 0x2AF660,
// Human::AddContextToMainAITrack 0x12CD0AC; Ground 4, Vocalization 20, LookAt 23 and UpperBody 27 read live).
enum class HumanContextId : std::uint32_t {
    DebugContext = 3, Ground = 4, Ladder = 5, InAir = 6, Ledge = 7, Climb = 8, Walling = 9, NarrowObject = 10,
    Swim = 11, Riding = 12, Boat = 13, Cart = 14, GliderPilot = 15, Wagon = 16, CannonOperator = 17,
    RagdollGround = 19, Vocalization = 20, CarriedBody = 22, LookAt = 23, Kiosk = 24, Dead = 25, HayStack = 26,
    UpperBody = 27,
};

// Human::HumanContextBundle: one pointer per context, created on demand (null until then). Slot order from the Mac
// getters (GetGround 0x12C4950 ... GetUpperBody 0x12C815E); the same on PC (live, LocalInits matched, RE/20 row 9).
struct HumanContextBundle {
    void* ground;          // 0x00  vftable 0x19BFCD8 (HumanGround)
    void* ladder;          // 0x04
    void* inAir;           // 0x08  vftable 0x19C05B0
    void* ledge;           // 0x0C  vftable 0x19C0010
    void* climb;           // 0x10
    void* walling;         // 0x14  vftable 0x1A2EC08
    void* riding;          // 0x18
    void* boat;            // 0x1C
    void* gliderPilot;     // 0x20
    void* cart;            // 0x24
    void* wagon;           // 0x28
    void* cannonOperator;  // 0x2C
    void* kiosk;           // 0x30
    void* dead;            // 0x34
    void* hayStack;        // 0x38
    void* debugContext;    // 0x3C  vftable 0x1A2FC70
    void* vocalization;    // 0x40  vftable 0x1A30160
    void* lookAt;          // 0x44  vftable 0x1A2FB98
    void* narrowObject;    // 0x48  vftable 0x1A30588
    void* swim;            // 0x4C
    void* upperBody;       // 0x50  vftable 0x19E6968
    void* ragdollGround;   // 0x54
    void* carriedBody;     // 0x58
};

// scimitar::ac2::EdgeClampingSettings in AC2 (not Revelations' layout); names by use (hypothesis), RE/11_ground.md 16.
struct EdgeClampingSettings {
    float edgeDistance;                  // 0x00  0.2 (Human ctor PC 0x932DC0); read by GetModifiedSpeed PC 0x930695
    AC2_PAD(0x0C);                       // 0x04
    float directionForRejectAngle[4];    // 0x10  written by AssassinTask::UpdateGroundMovement (the stick)
    float rejectAngle;                   // 0x20  0 or pi/3
};

// scimitar::ActorTrack (Revelations PDB names), 0x30 bytes; AIActor holds five from +0x04 (RE/10_human_update.md 1;
// AIActor::UpdateTracks PC 0x7A8C0 walks them with stride 0x30).
struct ActorTrack {
    bool contextHasChanged;           // 0x00  set by TransitToContext; the next update of the track is skipped once
    AC2_PAD(0x03);                    // 0x01
    void* contextList;                // 0x04  Array<ActorContext*> (data; the size word at +0x0A)
    std::uint32_t contextListCount;   // 0x08
    std::uint32_t initialContextID;   // 0x0C
    std::uint32_t previousContextID;  // 0x10
    void* currentContext;             // 0x14
    AC2_PAD(0x10);                    // 0x18  the init Functor0
    void* owner;                      // 0x28  the AIActor
    std::uint32_t trackID;            // 0x2C
};

// The Human's tracks (Human::CreateAI PC 0x9333A0): 1 the locomotion contexts, 2 Vocalization, 3 LookAt, 4 UpperBody.
inline constexpr int kMainTrack = 1;

struct Human {
    void* vftable;                    // 0x000  0x19E6B70 (ctor 0x932DC0)
    ActorTrack tracks[5];             // 0x004  AIActor's tracks: tracks[1].currentContext is Human +0x48 (live: follows
                                      //        Ground / InAir / Ledge ...), tracks[1].previousContextID +0x44
    AC2_PAD(0x100 - 0xF4);            // 0x0F4
    void* iHumanVftable;              // 0x100  the IHuman base: 0x19E69B0 (ctor 0x932DC0)
    AC2_PAD(0x440 - 0x104);           // 0x104
    HumanContextBundle contexts;      // 0x440  (Mac Human::AddContextToMainAITrack: this + 1088)
    AC2_PAD(0x991 - 0x49C);           // 0x49C
    // The edge-clamping flags (Revelations PDB names; EdgeClampingComputation PC 0x931330, Mac 0x12ED4B6 at +0x971..).
    bool isEdgeClampingActive;        // 0x991  set by the free-moving ground states' Enter (PC _EnterWait 0x61DFF2)
    bool isFightEdgeClampingActive;   // 0x992
    bool isEdgeClampingEnable;        // 0x993  IHuman::SetEdgeClamping
    bool isEdgeClampingZeroSpeed;     // 0x994
    bool edgeClampUsedLastFrame;      // 0x995
    bool forcedGapUpdate;             // 0x996
    AC2_PAD(0x9F0 - 0x997);           // 0x997
    EdgeClampingSettings edgeClampingSettings;  // 0x9F0  IHuman::GetEdgeClampingSettings
};

AC2_ASSERT_SIZE(HumanContextBundle, 23 * sizeof(void*));
AC2_ASSERT_OFFSET(HumanContextBundle, inAir, 0x08);
AC2_ASSERT_OFFSET(HumanContextBundle, narrowObject, 0x48);
AC2_ASSERT_OFFSET(HumanContextBundle, upperBody, 0x50);
AC2_ASSERT_SIZE(ActorTrack, 0x30);
AC2_ASSERT_OFFSET(ActorTrack, previousContextID, 0x10);
AC2_ASSERT_OFFSET(ActorTrack, currentContext, 0x14);
AC2_ASSERT_OFFSET(ActorTrack, trackID, 0x2C);
AC2_ASSERT_OFFSET(Human, tracks, 0x004);
static_assert(!AC2_GAME_ABI || offsetof(Human, tracks) + kMainTrack * sizeof(ActorTrack) +
                                       offsetof(ActorTrack, currentContext) == 0x48,
              "the main track's current context is Human +0x48");
AC2_ASSERT_OFFSET(Human, iHumanVftable, 0x100);
AC2_ASSERT_OFFSET(Human, contexts, 0x440);
AC2_ASSERT_OFFSET(Human, isEdgeClampingActive, 0x991);
AC2_ASSERT_OFFSET(Human, forcedGapUpdate, 0x996);
AC2_ASSERT_OFFSET(Human, edgeClampingSettings, 0x9F0);
AC2_ASSERT_OFFSET(EdgeClampingSettings, directionForRejectAngle, 0x10);
AC2_ASSERT_OFFSET(EdgeClampingSettings, rejectAngle, 0x20);

}  // namespace ac2sdk
