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

struct Human {
    AC2_PAD(0x48);                    // 0x000
    void* currentContext;             // 0x048  the context being updated (live: follows Ground / InAir / Ledge ...)
    AC2_PAD(0x440 - 0x4C);            // 0x04C
    HumanContextBundle contexts;      // 0x440  (Mac Human::AddContextToMainAITrack: this + 1088)
};

AC2_ASSERT_SIZE(HumanContextBundle, 23 * sizeof(void*));
AC2_ASSERT_OFFSET(HumanContextBundle, inAir, 0x08);
AC2_ASSERT_OFFSET(HumanContextBundle, narrowObject, 0x48);
AC2_ASSERT_OFFSET(HumanContextBundle, upperBody, 0x50);
AC2_ASSERT_OFFSET(Human, currentContext, 0x048);
AC2_ASSERT_OFFSET(Human, contexts, 0x440);

}  // namespace ac2sdk
