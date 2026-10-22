// The sdk/ headers compile in the port's build, and the address table stays inside the PC image.

#include "ac2sdk/addresses.h"
#include "ac2sdk/human.h"
#include "ac2sdk/human_ground.h"
#include "ac2sdk/layout.h"
#include "check.h"

namespace {
// PC IDB: .text 0x1000..0x18A5000, image size 0x2170000 (RE/01_triage.md).
constexpr std::uint32_t kTextBegin = 0x1000, kTextEnd = 0x18A5000;
}  // namespace

TEST(sdk_function_addresses_are_in_text) {
    using namespace ac2sdk::addr;
    for (std::uint32_t a : {Human_Update, Human_ApplyDeath, HumanInAirData_FastLoad, Engine_BeginFrame, World_GetPlayer,
                            HumanGround_ctor, HumanGround_Update, HumanGround_LinkFSM, HumanGround_UpdateHumanGround,
                            HumanGround_UpdateMove, HumanGround_UpdateMoveCycle, HumanGround_UpdateMoveCycleMain,
                            HumanGround_ExecTurnStartToWaitMain, HumanGround_ExecTurnStartToMoveCycleMain,
                            HumanGround_TransitFromMoveStopToWaitMain, EngineContext_GetCurrentGameWorld,
                            Human_ctor, Human_EdgeClampingComputation, Human_GetModifiedSpeed, Human_GetGapList,
                            Human_GetBestGapInDirectionInList, Human_GetDepthInFrontOfEdge,
                            GetEdgeSegmentNearCharacterPos, DisplacementCullingBySegment, Human_CreateAI,
                            Human_GetContextByID, Human_AddContextToMainAITrack, Human_InitMainTrack,
                            Human_UpdateAfterPhysics, AIActor_Update, AIActor_UpdateTracks, AIActor_UpdateAfterPhysics,
                            AIActor_TransitToContext_Functor0, AIActor_TransitToContext_Functor1, AIActor_Enter})
        CHECK(a >= kTextBegin && a < kTextEnd);
    static_assert(ac2sdk::addr::HumanGround_vftable >= kTextEnd);  // in .rdata
}

TEST(sdk_layout_macros_compile) {
    struct Probe {
        std::uint32_t vtable;
        AC2_PAD(4);
        float speed;
    };
    AC2_ASSERT_OFFSET(Probe, speed, 8);
    AC2_ASSERT_SIZE(Probe, 12);
    static_assert(sizeof(Probe) == 12);
}
