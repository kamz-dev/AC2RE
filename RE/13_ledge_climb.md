# 13: HumanLedge (hanging from a ledge) and HumanClimb

Status 2026-10-23: started. The class, its vftable, the FSM (all 29 states named and mapped Mac → PC), the
reflected data, how the context picks its entry state, and the pull-down's three phases are decoded. Next: HangEntry,
Wait, the release to InAir and the pull-up to Ground; then the hand placement / movement along the ledge (anchor
points, `19_guidance.md`). HumanClimb is not started.

Addresses: **PC** first (IDB rebased to 0), Mac (`IDB/scimitar_sf.i64`) in brackets. The Mac build has full symbols
for this class (828 functions: every state's Enter / Exit / Update, every transition's Test / Exec / Transit).

## 1. The class
- `scimitar::ac2::HumanLedge`: `DataContext<Human, HumanData, HumanLedgeData>` (an `ActorContext`), with
  `IHumanLedge` and the damage / teleport interfaces (Revelations PDB: `IHumanLedge` +0x14, `IHumanDamage` +0x18,
  `IHumanTeleport` +0x1C, `IHumanMindBurn` +0x20; AC2 not checked).
- PC vftable **0x19C0010**, 17 slots (the recorder's Ledge context id, `livelab/record_edges.py`):

| Slot | PC | Name [Mac] |
|---|---|---|
| 0 | 0x66AFA0 | scalar deleting dtor |
| 1, 4, 5, 6, 7 | 0x16EEF20, 0x16EEF50, 0x16EEF60, 0x16EEF40, 0x16EEF70 | `ActorContext` bases (shared) |
| 2 | 0x64D880 | `LocalInit` → `ActorContext::LocalInit` (shared thunk) [0x1318BB0] |
| 3 | 0x5E3280 | `GlobalInit` (shared) |
| 8 | 0x64D800 | `QueryInterface` [0x1318B76] |
| 9 / 10 | 0x64EB20 / 0x64EB10 | `DataContext<...>::GetActorContextData` (+ const) |
| 11 | 0x64F4D0 | `SetAIActorData` |
| 12 | 0x670A00 | `Enter` [0x1638396] |
| 13 | 0x663D60 | `Exit` [0x16381FC] |
| 14 | 0x6718E0 | `Update(long)` → `_UpdateHumanLedge` 0x6711A0 [0x16382CE → 0x1345F86] |
| 15 | 0x65D2E0 | `LinkFSM` [0x131E966] |
| 16 | 0x65DC90 | `GetCurrentStateName` [0x1319A48] |

## 2. The state machine
Same generated layout as HumanGround (`11_ground.md` 3), at the same offsets in both builds: the root at +0xDE0
(+0xDE1 initial child, +0xDE2 current), the `FSMSuccessfulInterfaceTransitionTracker` at +0xDE4 (reserved 29), and
3-byte records `{id, initialChild, currentChild}` from +0xE2C, one per id (`LinkFSM` PC 0x65D2E0). The root starts
at Movement; Movement's initial child is Wait; PullupToActorInterraction's is PullupToActor. The dispatcher
`_UpdateHumanLedge` (PC 0x6711A0 [0x1345F86]) clears the tracker; phase 0 runs `UpdateHumanLedgeMain` (PC 0x64F790)
and the root's transitions (`TestHumanLedgeToInAirMain` 0x65F320, `TestHumanLedgeToSwimMain` 0x6541E0); phase 1
runs `UpdateHumanLedgeAfterPhysics` (PC 0x64F860); then the current state's `_Update`.

| Id | Record | State | `_Update` PC [Mac] | Revelations member |
|---|---|---|---|---|
| 1 | +0xE2C | Movement (children 2, 3) | inlined [0x1340236] | `m_MovementState` |
| 2 | +0xE2F | Wait | 0x66F510 [0x133BC88] | `m_WaitState` |
| 3 | +0xE32 | HandPlacement | 0x662C00 [0x13401E4] | `m_HandPlacementState` |
| 4 | +0xE35 | ParrallelJumpPreparation | 0x662C70 [0x13443B4] | |
| 5 | +0xE38 | ParrallelJump | 0x6554F0 [0x1321568] | |
| 6 | +0xE3B | PullDown | 0x66A880 [0x132D2CA] | `m_PullDownState` |
| 7 | +0xE3E | HangEntry | 0x66F600 [0x133DE52] | `m_HangEntryState` |
| 8 | +0xE41 | SwingEntryCollision | 0x660240 [0x1321E44] | |
| 9 | +0xE44 | SwingEntryCollisionBeam | 0x66B460 [0x133BE68] | |
| 10 | +0xE47 | Pullup | 0x670A40 [0x133DAC4] | `m_PullupState` |
| 11 | +0xE4A | HandPassOver | 0x66B4B0 [0x133ED84] | |
| 12 | +0xE4D | ReboundTransition | 0x655530 [0x13210EA] | |
| 13 | +0xE50 | HangWallReception | 0x662D10 [0x1321DA0] | |
| 14 | +0xE53 | HangFreeReception | 0x6602A0 [0x1321D3E] | |
| 15 | +0xE56 | Swing | 0x6602E0 [0x1324E64] | |
| 16 | +0xE59 | SwingDownImpulsion | 0x662D70 [0x133F1FA] | |
| 17 | +0xE5C | SwingDown | 0x663950 [0x133F64A] | |
| 18 | +0xE5F | ReleaseToTarget | 0x662DB0 [0x131C5A8] | |
| 19 | +0xE62 | GraspEntry | 0x662DF0 [0x134012C] | |
| 20 | +0xE65 | SwimEntry | 0x660340 [0x1321CBE] | |
| 21 | +0xE68 | GraspHangFreeOneHandReception (children 22, 23) | 0x662EA0 [0x13402B6] | |
| 22 | +0xE6B | GraspHangFreeOneHandReceptionInertPart | [0x1340286] | |
| 23 | +0xE6E | GraspHangFreeLastHandPlacement | [0x1321C5A] | |
| 24 | +0xE71 | PullupToActorInterraction (children 25–27) | 0x66B530 [0x13456A6] | |
| 25 | +0xE74 | PullupToActor | [0x134563C] | |
| 26 | +0xE77 | GrabOnGround | [0x1321180] | |
| 27 | +0xE7A | GrabOnGroundToLedge | [0x1321BEE] | |
| 28 | +0xE7D | VerticalLeapImpulsion | 0x66B570 [0x1345F3C] | |
| 29 | +0xE80 | Hurt | 0x65D490 [0x1326C7E] | `m_HurtState` |

The PC names were given by position against the Mac dispatcher (both dispatchers test the records in the same
order). Revelations has 40 states (hookblade, vertical jump, swim, grab-on-ground variants); AC2's 29 are a subset.
Children of 21 and 24 are dispatched inside their parents' `_Update`, so their PC functions aren't named yet.

**The transitions** (Mac symbols; all `ExecXToYMain` / `…On<InterfaceCommand>`), for the states the first phases need:
- PullDown → InAir (sub-state 4 `ReleaseToInAir`), → Climb (HumanLedge +0xA74 & 0x10, Mac), → HangEntry
  (`TestPullDownToHangEntryMain` [0x1325458]).
- HangEntry → Movement, → Climb, → ReboundTransition (Rebound), → VerticalLeapImpulsion (VerticalLeap).
- Wait → HandPlacement, → Climb, → Ladder, → InAir, → ParrallelJumpPreparation, → SwingDownImpulsion.
- Movement → InAir (Release), → Pullup (Pullup, the assassinations), → HandPlacement (TurnAround), → HandPassOver
  (PassOver, ExitWaterWell, Hurt), → ReboundTransition, → ReleaseToTarget, → Swing, → VerticalLeapImpulsion,
  → PullupToActorInterraction.
- Pullup → Movement (Ground), → InAir (Release, JumpFromHangKnee, Hurt, Die), → NarrowObject.
- The root → InAir, → Swim, → Ground (TeleportToGround), → Hurt.

The interface commands (`HumanLedge::InterfaceCommand`, Revelations): 0 Pullup, 1 Rebound, 2 JumpFromHangKnee,
3 Release, 4 ReleaseToTarget, 5 StartSwing, 6 StopSwing, 7 JumpFromSwing, 8 JumpFromHandPassOver, 9 Hurt,
10 TurnAround, 11–18 the HumanLedge-wide ones (Hurt, Die, TeleportToGround, MindBurn, the assassinations,
PullupToActor, VerticalLeap), 19 PassOver, 20 ExitWaterWell, 21 VaultOnBeam (AC2's numbering not checked).

## 3. HumanLedgeData (reflected, PC; `py RE/tools/refl.py HumanLedgeData`)
0x1B0 bytes, parent `HumanLedgeDatav2`. Reflected members (the rest is unreflected; Revelations offsets differ by
+0x10 from +0x40 on: it adds `m_FastPullUpHookbladePos`):

| PC | Name | Type |
|---|---|---|
| +0x10 | `CurDirection` | vec4 |
| +0x20 | `CurRelativeDirection` | vec4 |
| +0x30 | `JumpDirection` | vec4 |
| +0x40 | `PullDownType` | enum PullDownType |
| +0x44 | `PullDownSide` | enum PullDownSide |
| +0x48 | `GraspType` | enum GraspType |
| +0x4C | `SubState` | enum LedgeSubState |
| +0x50 | `HandPassOverSubState` | enum |
| +0x54 | `PullDownSubState` | enum PullDownSubState |
| +0x58 | `DirectionType` | u32 flags |
| +0x5C / +0x60 | `CurSpeedRatio` / `DestSpeedRatio` | float |
| +0x64 | `NextHandToMove` | enum |
| +0x68 | `LedgeHangType` | enum LedgeHangType |
| +0x6C | `HangFreeReceptionType` | enum |
| +0x70 | `SwingStrength` | float |
| +0x74 | `WaterWell` | handle Entity |
| +0x78 / +0x80 | `CloseMovementGuidanceReport` / `ParallelJumpGuidanceReport` | GuidanceReport |
| +0x88 | `DestMvtType` | u8 |
| +0x89 / +0x8A / +0x8B | `ArmsApart` / `HangAfterPassover` / `IsWaterWellLedge` | bool |
| +0xF0 | (`m_PullDownGap`, Revelations +0x110) the gap the pull-down starts from | `IHuman::Gap` |
| +0x134 | (unnamed) a blend weight the PullDownType 4 actions use | float |

Enums (Revelations values; AC2's checked where noted):
- `PullDownType`: 0 Ground_Soft_EdgeStop, 1 Ground_Soft_Wait, 2 Ground_Hard, 3 Beam_Soft, 4 HandPassOver,
  5 WaterWellSoft, 6 Ground_Fast. HumanGround's Execs set 0 from EdgeStop and 1 from Move (`11_ground.md` 17).
- `PullDownSide`: 0 Front, 1 Back, 2 Left, 3 Right.
- `PullDownSubState`: 0 None, 1 Orientation, 2 Descent, 3 Reception, 4 ReleaseToInAir.
- `LedgeHangType`: 0 Wall, 1 Free, 2 WallFree.
- `GraspType`: 0 HangKnee, 1 Climb, 2 HangWaist, 3 HangWall, 4 HangFree_1Hand, 5 HangFree_2Hands.
- `LedgeSubState` (the entry request, below). AC2 matches Revelations through 13 and lacks its 14
  `SubState_HookBladeGrasp`, so AC2's 14–18 are Revelations' 15–19 **(hypothesis; every case of the entry switch
  fits)**: 0 Entry, 1 Movement, 2 TurnCorner, 3 HandPlacement, 4 Pullup, 5 ReboundTransition, 6 HangWallReception,
  7 HangFreeReception, 8 SwingReception, 9 TransitionInFromClimb, 10 TransitionInFromLadder, 11 PullDown,
  12 HandPassOver, 13 Grasp, 14 ParallelJump, 15 SwimEntry, 16 PullupToActorGrab, 17 PullupToActorAssassinate,
  18 JumpToActorAssassinate.

## 4. Entering: the requested sub-state picks the state
Whoever switches to Ledge first fills `HumanLedgeData` and sets `SubState` (+0x4C). HumanGround's pull-down Exec
(`ExecMoveToLedgeOnHumanGroundInterfaceCommand_LedgePullDown` [0xF98DA6]) copies the gap to +0xF0 and sets
`PullDownType` 1, `PullDownSide` 0 (Front), `SubState` 11.

`EnterHumanLedge` [0x13477BA] (PC: inside `Enter` 0x670A00, not split yet) then:
- sets the context flags (Mac +0xA74, PC not checked: `& 0xFFFF7880 | 0x8001`), turns on the water-well height when the
  ledge is a well, switches the character capsule to layer `GetLayer(20)`, turns sticking off, moves the capsule to
  `LEDGE_CAPSULE_OFFSET` over 0.2 s (except `SubState` 12), clears data +0x89 (`ArmsApart`);
- picks the root's initial state (+0xDE1) from `SubState`: 0 → HangEntry, 1 → Movement, 4 → Pullup,
  6 → HangWallReception, 7 → HangFreeReception, 8 → Swing, 9 → Movement (after `SetupTransitionFromClimb`),
  11 → **PullDown**, 12 → HandPassOver, 13 → GraspEntry, 14 → Movement with `SubState` 3, 15 → SwimEntry,
  16 / 17 → PullupToActorInterraction, 18 → Pullup (after `SetupJumpToActorAssassinate`);
- unless the current action is one of five hang actions, initialises the limbs' IK (`InitializeLimbIKData`), except
  for the entries that place their own limbs (0, 4, 6, 8, 11, 13, 15);
- emits presentation event 2, starts a 0.5 s timer (Mac +0xA00), and the accomplishment event 19.
`_EnterHumanLedge` [0x1347BE0] then enters the chosen state (PullDown: `EnterPullDown` [0x133DE9E]).

## 5. PullDown (id 6)
`PullDownSubState` (data +0x54) steps through three moves. Each is a `DisplacementManager` move set up with
`SetupInterpolatorForHumanLedgeMove` [0x10A3C66] (or `LegacySetup`) over the current action's length, updated every
frame (`LegacyUpdate`); a step ends when `GetProgressionRatio` reaches 1.

- **Enter** (`EnterPullDown` [0x133DE9E]): the action is `g_PullDownActionTable[PullDownType][PullDownSide]`
  [0x2D2BC20] (types 4 and 5 pick their own actions); the reference rotation delay is 0.2 s; for sides Back / Left /
  Right the banking is turned (Back: the sight reversed); the skeleton tweens over 0.2 s. A first anchor point is
  looked for under the gap's edge (`GetClosestPointInBoxWithOffset`, a 0.1 m box, layer mask 0x2032), else set from
  the gap's edge and normal. The body moves along that action to the edge. `PullDownSubState` = 1 (Orientation);
  presentation event 103; the climb IK is reset.
- **Orientation done** (`UpdatePullDownMain` [0x132BBA8], sub-state 1): cast the pull-down guidance report
  (`CastPullDownGuidanceReport` [0x132B7EA]); find two hand anchors with `GetNextVerticalLimbPos` [0xD206F8], one
  each side of the anchor (the offsets aren't read yet; the call's limits are −0.3, 0.3 and 0.7). Found: the descent action
  (`dword_2D2BC30[PullDownType][PullDownSide]`), the hands' IK targets (limbs 0 and 1), and a move to the anchor
  point 0.8 m lower; sub-state 2 (Descent). Not found: sub-state 4 (→ InAir).
- **Descent done** (sub-state 2): from the two hands' anchors (positions and normals made flat), look for the toes'
  anchors on the wall 1.2 m below (`Human::GetToesDestAnchors` [0x12DBE14], 0.65). Found: hang against the wall
  (action 0x6E8BD81; `Human::ComputeHangWallEntityPosition` [0x12D61CE]; a blend between two (or three) actions by
  the wall's tilt, up to 30° one way or 45° the other; the toes' IK targets; data +0x68 `LedgeHangType` = 0 Wall).
  Not found: hang free (action 0x82F9F3B; `ComputeHangFreeEntityPosition` [0x12CDE14]; `LedgeHangType` = 1 Free).
  A last move to that position; sub-state 3 (Reception).
- **→ HangEntry** (`TestPullDownToHangEntryMain` [0x1325458]): the current action is 0x82F9F3B or 0x6E8BD81 and the
  move is done.

So the hang's position is geometry (the hand anchors on the guidance line and the wall), while each step's duration
is its animation's length: the port needs the former decoded and the latter measured live.

**Live (row 15, 2026-10-23):** Orientation 0.53 s (PullDownType 1) / 0.40 s (type 0), Descent 0.13 s, Reception
0.13 s (wall) / 0.33 s (free). The root ends the Orientation 0.53 m out from the edge, the Descent 0.8 m lower, and
the hang at **0.539 out, 1.082 down** against a wall or **on the edge line, 2.400 down** hanging free.

## 6. HangEntry (id 7) and the hang
- `UpdateHangEntryMain` [0x133DB24]: waits for the reception action to finish (`HasActionFinished`), forces the move
  to its end (progression 1), initialises the limbs' IK (unless the entry was a pull-down, `SubState` 11), then plays
  the hang's idle by `LedgeHangType` (data +0x68): Wall → 0x106F2E8 (unless a parallel jump is found on the way in),
  Free → 0x12719F1 (blended by data +0x70 for grasp types 2 / 3), WallFree → 0x106F2E9; and sets `SubState` = 1.
- `_UpdateHangEntry` [0x133DE52]: → Climb when HumanLedge +0xA75 & 2 (Mac), else → Movement once `SubState` is 1
  (`TestHangEntryToMovementMain` [0x131907E]). Movement enters its initial child, Wait.
- Wait → InAir by itself only when hanging from a broken rope (`TestWaitToInAirMain` [0x1318CD4]); otherwise the
  hang ends through interface commands.

## 7. Interface commands while hanging
Each `CanX` asks `_TestInterfaceTransitionsHumanLedge(command)` [0x1342A74] and is true when the answer is 2 (the
same tracker as HumanGround's); `DoX` packs the parameters and runs `_ExecInterfaceTransitionHumanLedge(command)`.
- **Release** (command 3; `CanRelease` [0x1342EE4], `DoRelease(FootHold*, LandingIntention)` [0x134731A]): from
  Movement, `TestMovementToInAirOnInterfaceCommand_Release` [0x1324584] answers 2 at a water well or when the current
  action can be interrupted by a release (`IsActionInterruptibleByRelease` [0x13244B6]), else 1;
  `ExecMovementToInAirOnInterfaceCommand_Release` [0x13407A2] → `ReleaseToInAir` [0x13402EA].
- **Pullup** (command 0; `CanPullup` [0x134308C], `DoPullup` [0x134754A]): from Movement, 3 at a water well, else
  `EvaluatePullupFromMovement(1, 0, 1)` [0x132EDCA] (`TestMovementToPullupOnInterfaceCommand_Pullup` [0x132FB72]).
- Live (row 15): the pull-up takes 0.73 s from a wall hang and 1.54 s from a free hang, then goes through
  **NarrowObject** (`ExecPullupToNarrowObjectMain`) for 0.27 s before Ground, standing 0.05–0.08 m back from the edge.
- The player's controller sends them from `AssassinTask::UpdateLedge` [0x1076B30] (0x1E74 bytes, not read yet).

## Open
- PC addresses of the Enter / Exit / Test / Exec functions (only the `_Update`s are named).
- `ComputeHangWallEntityPosition`, `ComputeHangFreeEntityPosition`, `GetNextVerticalLimbPos`, the anchor points
  (`AnchorPoint`, `19_guidance.md`), `SetupInterpolatorForHumanLedgeMove` / `DisplacementManager`.
- `UpdateWaitMain` [0x133B696], `ReleaseToInAir`, `EvaluatePullupFromMovement`, Pullup → Ground, and
  `AssassinTask::UpdateLedge` [0x1076B30] (the AI's is `GroundNavigationTask::UpdateLedgeContext` [0xE30D5C]).
- Live: the pull-down's three step durations, the hang position against the edge (wall and free), and how a
  high-profile run hands Ground over to Ledge (`11_ground.md` 16 Open).
