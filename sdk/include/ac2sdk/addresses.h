#pragma once
// Function and data addresses in the pinned PC build (AssassinsCreedIIGame.exe, sha256 f4983add...; see
// RE/01_triage.md). Numbers are as the PC IDB shows them: the IDB is rebased to image base 0, so each is an RVA and
// the run-time address is the module base + the number.
//
// One line per address: the name as in the PC IDB (Class__Method or scimitar::...), the Mac symbol it was matched
// from when there is one, and its status: "named" (named in the IDB, not checked further), "static" (checked in
// the decompiler), or "live <date>" (confirmed in the running game).

#include <cstdint>

namespace ac2sdk::addr {

// scimitar::ac2::Human
inline constexpr std::uint32_t Human_Update = 0x933480;      // Mac __ZN8scimitar3ac25Human6UpdateEv 0x12EDA06; named
inline constexpr std::uint32_t Human_ApplyDeath = 0x929650;  // named
inline constexpr std::uint32_t Human_ctor = 0x932DC0;                     // static (writes both vftables)
inline constexpr std::uint32_t Human_vftable = 0x19E6B70;                 // static
inline constexpr std::uint32_t Human_IHuman_vftable = 0x19E69B0;          // at +0x100; slots match Mac 0x2842CA8
inline constexpr std::uint32_t Human_EdgeClampingComputation = 0x931330;  // Mac 0x12ED4B6; static (RE/11_ground.md 16)
inline constexpr std::uint32_t Human_GetModifiedSpeed = 0x930540;         // Mac 0x12ED290; static
inline constexpr std::uint32_t Human_GetGapList = 0x9282B0;               // IHuman +0x98, Mac 0x12D4BD6; static
inline constexpr std::uint32_t Human_GetBestGapInDirectionInList = 0x91F1F0;  // IHuman +0x9C, Mac 0x12CE79C; static
inline constexpr std::uint32_t Human_GetDepthInFrontOfEdge = 0x9251D0;    // Mac 0x12D0AD2; static
inline constexpr std::uint32_t GetEdgeSegmentNearCharacterPos = 0x1007D20;  // Mac 0xD37540; static
inline constexpr std::uint32_t DisplacementCullingBySegment = 0x1009420;    // Mac 0xD3675C; static

// scimitar::ac2::HumanGround (RE/11_ground.md)
inline constexpr std::uint32_t HumanGround_vftable = 0x19BFCD8;                // static (only xref of LinkFSM)
inline constexpr std::uint32_t HumanGround_ctor = 0x6317A0;                    // static (writes the vftable)
inline constexpr std::uint32_t HumanGround_Update = 0x63B2C0;                  // slot 14, Mac 0x1544B74; static
inline constexpr std::uint32_t HumanGround_LinkFSM = 0x602270;                 // slot 15, Mac 0xFA4130; static
inline constexpr std::uint32_t HumanGround_UpdateHumanGround = 0x6381D0;       // Mac 0xFED468; static
inline constexpr std::uint32_t HumanGround_UpdateMove = 0x634D20;              // Mac 0xFE069C; static
inline constexpr std::uint32_t HumanGround_UpdateMoveCycle = 0x621B90;         // Mac 0xFE05A2; static
inline constexpr std::uint32_t HumanGround_UpdateMoveCycleMain = 0x61BB60;     // Mac 0xFCD20A; static
inline constexpr std::uint32_t HumanGround_LocalInit = 0x61F920;               // slot 2, Mac 0xFA71B0; static
inline constexpr std::uint32_t HumanGround_UpdateMoveMain = 0x604990;          // Mac 0xFCA184; static
inline constexpr std::uint32_t HumanGround_UpdateMovementIntention = 0x5E03E0; // Mac 0xF98C54; static
inline constexpr std::uint32_t HumanGround_UpdateTurnAngle = 0x5FE6A0;         // Mac 0xFB34C4; static
inline constexpr std::uint32_t HumanGround_UpdateOrientation = 0x5FFC40;       // Mac 0xFB4E32; static
inline constexpr std::uint32_t HumanGround_TestWaitToMoveCycleMain = 0x5F4E70; // Mac 0xF9EAE6; static
inline constexpr std::uint32_t HumanGround_TestMoveCycleToWaitMain = 0x5EAB90; // Mac 0xFB5D7A; static
inline constexpr std::uint32_t HumanGround_IHumanGround_vftable = 0x19BF390;   // at +0x14 (ctor); static
inline constexpr std::uint32_t RotateVectorToVectorWithAngularSpeed = 0x5FF930;  // Mac 0xFB4A3C; static
inline constexpr std::uint32_t HumanGroundData_GetMovementDivisionFromSpeedRatio = 0x90E190;  // Mac 0x1119F44
inline constexpr std::uint32_t HumanGround_InitializeGlobalActions = 0x633E90;  // Mac 0xFA755A; static
inline constexpr std::uint32_t HumanGround_EnterTurnStart = 0x614BB0;          // Mac 0xFBE3D6; static
inline constexpr std::uint32_t HumanGround_EnterMoveStop = 0x614370;           // Mac 0xFB7804; static
inline constexpr std::uint32_t HumanGround_UpdateMoveStop = 0x621C80;          // Mac 0xFD1CA4; static
inline constexpr std::uint32_t HumanGround_UpdateTurnStart = 0x621EE0;         // Mac 0xFD18F6; static
inline constexpr std::uint32_t HumanGround_UpdateRunTurn = 0x621E60;           // Mac 0xFD197C; static
inline constexpr std::uint32_t HumanGround_StartTurnOnSpotAnim = 0x606B70;     // Mac 0xFB37D0; static
inline constexpr std::uint32_t HumanGround_GetTurnAnimation = 0x5FDF00;        // Mac 0xFB3550; static
inline constexpr std::uint32_t HumanGround_ExecTurnStartToWaitMain = 0x614F40;       // Mac 0xFB664C; static
inline constexpr std::uint32_t HumanGround_ExecTurnStartToMoveCycleMain = 0x615120;  // Mac 0xFB7474; static
inline constexpr std::uint32_t HumanGround_TransitFromMoveStopToWaitMain = 0x61F200; // Mac 0xFD1C64; named
inline constexpr std::uint32_t EngineContext_GetCurrentGameWorld = 0x2C4B0;  // named; ms_Instance +0x4C
inline constexpr std::uint32_t EngineContextImp_ms_Instance = 0x1E10AC0;     // .data; named (live path: RE/20)
inline constexpr std::uint32_t ActorContext_LocalInit = 0x16EEF30;          // Mac 0x509920; static
inline constexpr std::uint32_t HumanInAir_vftable = 0x19C05B0;              // LocalInit 0x6911A0 = Mac 0x122B552; live 2026-10-21
inline constexpr std::uint32_t HumanLedge_vftable = 0x19C0010;              // by bundle slot 3; live 2026-10-21
inline constexpr std::uint32_t HumanWalling_vftable = 0x1A2EC08;            // LocalInit 0xFA4760 = Mac 0x1178ED8; live 2026-10-21
inline constexpr std::uint32_t HumanNarrowObject_vftable = 0x1A30588;       // LocalInit 0xFF1110 = Mac 0x1131AAE; live 2026-10-21
inline constexpr std::uint32_t HumanLookAt_vftable = 0x1A2FB98;             // LocalInit 0xFE0E00 = Mac 0xEB7B58; live 2026-10-21
inline constexpr std::uint32_t HumanUpperBody_vftable = 0x19E6968;          // LocalInit 0x63B6B0 = Mac 0x1374282; live 2026-10-21
inline constexpr std::uint32_t Vocalization_vftable = 0x1A30160;            // by bundle slot 16; live 2026-10-21
inline constexpr std::uint32_t HumanGround_SetBreakHard = 0x632E20;            // IHumanGround slot 30, Mac 0x153B958
inline constexpr std::uint32_t HumanGround_SetSprint = 0x632E80;               // IHumanGround, Mac 0x153B9D0
inline constexpr std::uint32_t HumanGround_EnterMove = 0x60C610;              // Mac 0xFA3D06; static
inline constexpr std::uint32_t HumanGround_EnterMoveDispatch = 0x621720;      // _EnterMove, Mac 0xFD43E0; static
inline constexpr std::uint32_t HumanGround_UpdateLanding = 0x630800;          // Mac 0xFD1B0E; static
inline constexpr std::uint32_t HumanGround_UpdateLandingHard = 0x630890;      // Mac 0xFD1A78; static
inline constexpr std::uint32_t HumanGround_UpdateJumpImpulsion = 0x624060;    // Mac 0xFD5078; static
inline constexpr std::uint32_t HumanGround_UpdateJumpImpulsionStraight = 0x6240A0;  // Mac 0xFD4FF4; static
inline constexpr std::uint32_t HumanGround_UpdateEdgeStop = 0x624150;         // Mac 0xFD4F0C; static
inline constexpr std::uint32_t HumanGround_UpdateCrouchWait = 0x623FA0;       // Mac 0xFDEBF6; static
inline constexpr std::uint32_t HumanGround_UpdateCrouchMove = 0x61E620;       // Mac 0xFDEB50; static
inline constexpr std::uint32_t HumanGround_UpdateCrouchTurn = 0x5FA510;       // Mac 0xFC3300; static
inline constexpr std::uint32_t HumanGround_ActorCanPhysicallyStandUp = 0x5F72C0;  // Mac 0xFDEA42; static
inline constexpr std::uint32_t ActionComponent_WillActionFinish = 0x165C90;    // named by call shape; static
inline constexpr std::uint32_t ActionComponent_HasActionFinished = 0x165C20;   // named by call shape; static

// scimitar::ac2::AssassinTask, the player's controller (RE/21_input.md)
inline constexpr std::uint32_t AssassinTask_UpdateGround = 0xE049A0;           // Mac 0x1082F62; static
inline constexpr std::uint32_t AssassinTask_UpdateGroundMovement = 0xDF7FE0;   // Mac 0x107F0EC; static (constants)
inline constexpr std::uint32_t AssassinTask_UpdateInAir = 0xDEC620;            // Mac 0x105C244; static (constants)
inline constexpr std::uint32_t AssassinTask_vftable = 0x1A23360;               // live 2026-10-21 (player's task)
inline constexpr std::uint32_t PadInput_ComputeRelativeInput = 0x1E84A0;       // Mac 0x81F570; static
inline constexpr std::uint32_t PadInput_Update = 0x1E8740;                     // Mac 0x81FC30; static
inline constexpr std::uint32_t AssassinTask_ReorientDirectionFromWallCollision = 0xDF3590;  // static
inline constexpr std::uint32_t AssassinTask_GetCrowdObstructionFactor = 0xDE1720;           // static
inline constexpr std::uint32_t AssassinTask_OnSprintBegin = 0xDEB5E0;          // Mac 0x104EA26; static
inline constexpr std::uint32_t AssassinAbilities_GetMaxSpeed = 0x6CCC40;       // static
inline constexpr std::uint32_t AssassinAbilities_GetAbility = 0x6CCCB0;        // static

// Action-id maps (filled by HumanGround::InitializeGlobalActions)
inline constexpr std::uint32_t g_GroundWaitActionMap = 0x1E13600;
inline constexpr std::uint32_t g_MoveStopActionMap = 0x1E13694;
inline constexpr std::uint32_t g_RunTurnActionMap = 0x1E136A4;
inline constexpr std::uint32_t g_TurnStartActionMap = 0x1E13740;

// Context data
inline constexpr std::uint32_t HumanInAirData_FastLoad = 0x90C4D0;  // named

// Engine and world
inline constexpr std::uint32_t Engine_BeginFrame = 0x227F0;  // scimitar::Engine::BeginFrame; named
inline constexpr std::uint32_t World_GetPlayer = 0x3A300;    // scimitar::World::GetPlayer; named

}  // namespace ac2sdk::addr
