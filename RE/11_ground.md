# 11: HumanGround (on-foot context)

Status 2026-10-21: the class, its vftable, the FSM layout and the locomotion branch of the state tree are matched
Mac → PC; the MoveCycle speed model, the Wait / MoveCycle transitions and ground turning are decoded from both
builds, and so are MoveStop, TurnStart, RunTurn and TurnOnSpot with the action maps they play, entering Move
(the hand-over from other contexts), the landings and Roll, the jump / edge / obstacle entry points, crouching,
TurnOnSpot and Idle. Left: the property names of `HumanGroundData` (reflection, `22_reflection.md`) and the
outside systems that drive HumanGround (controller, free-run / guidance).

Addresses: **PC** first (IDB rebased to 0), Mac (`IDB/scimitar_sf.i64`) in brackets.

## 1. The class
- `scimitar::ac2::HumanGround` derives from `scimitar::ActorContext` (primary) and
  `scimitar::DataContext<Human, HumanData, HumanGroundData>`, and implements `scimitar::ac2::IHumanGround`
  [Mac vtable `__ZTVN8scimitar3ac211HumanGroundE` 0x281A1E0].
- PC vftable **0x19BFCD8**, 24 slots (no RTTI; found from `LinkFSM`'s only data xref). Constructor 0x6317A0,
  destructor 0x633740 (both write the vftable).
- Slot map (PC slot *k* = Mac slot *k*+1, because Itanium has two destructor entries):

| Slot | PC | Name [Mac] |
|---|---|---|
| 0 | 0x6367C0 | scalar deleting dtor [D1 0xFADF06 / D0 0xFAB9BA] |
| 1 | 0x16EEF20 | `ActorContext::CreateAI` (base, shared) [0x509910] |
| 2 | 0x61F920 | `LocalInit` [0xFA71B0] |
| 3 | 0x5DF640 | `GlobalInit(World*)` [0xF9766E] |
| 4 | 0x16EEF50 | `ActorContext::TriggerEnter` (base) [0x509940] |
| 5 | 0x604540 | `Shutdown(World*)` [0xF9C05A] |
| 6 | 0x16EEF40 | `ActorContext::UpdateAfterPhysics` (base) [0x509930] |
| 7 | 0x604580 | `ReleaseResources` [0xFBA904] |
| 8 | 0x5DF660 | `QueryInterface(unsigned long)` [0xF976B8] |
| 9 / 10 | 0x5E32A0 / 0x5E3290 | `DataContext<...>::GetActorContextData` (+ const) [0x153BE28 / 0x153BE1C] |
| 11 | 0x5E76A0 | `SetAIActorData(AIActorData*)` [0xF97682] |
| 12 | 0x633730 | `Enter` [0x1544B6A] |
| 13 | 0x6367F0 | `Exit` [0x1543BD4] |
| 14 | 0x63B2C0 | `Update(long)` → `_UpdateHumanGround` [0x1544B74] |
| 15 | 0x602270 | `LinkFSM` [0xFA4130] |
| 16 | 0x5FB130 | `GetCurrentStateName` (returns "" in both builds) [0xF983CC] |
| 17 | 0x5E30C0 | `GetHumanGroundBlockTarget` [0x1543900] |
| 18–20 | 0x5E6BB0, 0x60BBA0, 0x5E8ED0 | `CanBreak`, `DoBreak`, `BreakOther` |
| 21–23 | 0x5E0B30, 0x5FEDF0, 0x5EC2D0 | `HandleAnimDeath`, `HandleWeaponHandSwitch`, `CanHurt` |

Slots with 4–17-byte bodies (1, 3, 4, 6, 9, 10, 12, 13, 17) are left unnamed in the IDB where they may be folded
with other classes' identical functions.

## 2. Fields (PC offsets; Mac in brackets where it differs)
The FSM records sit at identical offsets in both builds; from about 0x6E0 on, PC fields are **+0x10 or +0x20**
later than Mac. Check each field on PC. **Names** in `code` are the Revelations PDB's (`py RE/tools/pdbtypes.py
scimitar::ac2::HumanGround`): Revelations' HumanGround is 0xD50 bytes with more members, but the movement block
keeps its order, and neighbouring members pair up one for one with AC2's (2026-10-21). The enums' value names
are the PDB's too.

| PC | Type | Meaning | Evidence |
|---|---|---|---|
| +0x08 | `AIActor*` | the actor (Mac `this[2]`) | `_UpdateNavigation` 0x6371E0 |
| +0x0C | `HumanData*` **(hypothesis)** | the DataContext's actor data; holds cycle-breaker timings (+0x120..+0x12C) and a flag +0x11E | `UpdateMoveCycleMain` 0x61BB60 |
| +0x10 | `HumanGroundData*` | the context's runtime data (see 4) | everywhere |
| +0x41 / +0x42 | u8 | root FSM: initial / current state id | `LinkFSM`, `_UpdateHumanGround` |
| +0x44 | tracker | `FSMSuccessfulInterfaceTransitionTracker`, reserved 147 | `LinkFSM` |
| +0x8C… | FSM records | see 3 | `LinkFSM` |
| +0x6F9 [+0x6E9] | bool | walking on a horse (cached `IsWalkingOnHorse` 0x5FBCE0) → InAir | `_UpdateMove` 0x634D20 |
| +0x790 [+0x770] | `HumanUpperBody*` **(hypothesis from Mac calls)** | upper-body context | `UpdateMoveCycleMain` |
| +0x464 [+0x460] | `IHumanGround::TurnMode` | `m_TurnMode`: 0 MainCharacter, 1 NPC; TurnOnSpot needs NPC (`SetTurnMode`'s target) | `TestWaitToTurnOnSpotMain` 0x5F4F30 |
| +0x468 [+0x464] | `HumanGround::TurnIntention` | `m_TurnIntention`: 0 Left, 1 Right (when the turn angle was ≥ 0), 2 Back, 3 Unknown | `EnterTurnStart` 0x614BB0 |
| +0x480 | float | `m_IntendedTurnAngle`: the turn angle when TurnStart was entered | `EnterTurnStart` |
| +0x7A0 [+0x780] | bool | `m_OnlyUseTurnOnSpot`: blocks TurnStart from Wait | `TestWaitToTurnStartMain` 0x5F4EC0 |
| +0x7A8 [+0x788] | `IHumanGroundAccess::MovementState` | `m_CurrentMvtState`: 0 Wait, 1 Slow, 2 Fast (drives `UpdateOrientation` when not Wait) | `UpdateMoveCycleMain`, `UpdateMoveMain` 0x604990 |
| +0x7AC [+0x78C] | same | **`m_IntentionMvtState`** (the "movement division"): 0 Wait = no movement intent, 2 Fast = moving (see 6) | `UpdateMovementIntention` 0x5E03E0 |
| +0x7B0 [+0x790] | `IHumanGroundAccess::MovementType` | `m_CurrentMvtType`: 0 Low, 1 High (the speed ratio is above 0.4) | `UpdateMoveCycleMain` |
| +0x7B4 [+0x794] | same | `m_IntentionMvtType`, = `HumanGroundData::m_DestMvtType` (+0x181) != 0 | `UpdateMovementIntention` 0x5E03E0 |
| +0x7BC [+0x79C] | float | **`m_CurrentSpeedRatio`** 0..1 | `UpdateMoveCycleMain` |
| +0x7C0 [+0x7A0] | float | `m_IntentionSpeedRatio`, used with `SpeedControlType_DirectSpeed` | same |
| +0x7D8 [+0x7B8] | float | **`m_TurnAngle`**: signed, about +Z, from the move direction to the sight | `UpdateTurnAngle` 0x5FE6A0 |
| +0x7E0 | vec4 | `m_HeadingBeforeRotationUpdate`: the entity's sight, copied each `UpdateMoveMain` | `UpdateMoveMain` 0x604990 |
| +0x81C [+0x7FC] | `ResponseCurve` | `m_SlowDownCurve` (see 5) | `LocalInit` 0x61F920 |
| +0x8A8 [+0x888] | `IHumanGround::SpeedControlType` | `m_SpeedControlType`: 0 MvtTypeAndMvtState, 1 DirectSpeed (the target is `m_IntentionSpeedRatio`) | `UpdateMoveCycleMain` |
| +0x8AC / +0x8B0 [+0x88C / +0x890] | float | `m_AlternateJogBlendFactor` / `m_AlternateSprintBlendFactor` (blend slots 12 / 19) | same |
| +0x8B4 | bool | `m_OverrideMoveCyclebySubState`: `UpdateMoveCycleMain` zeroes the body angles and returns; cleared by `ExitMoveCycle` | same; `ExitMoveCycle` 0x60DB70 |
| +0x8B8 / +0x8BC [+0x898 / ?] | float | `HeadingMinRotationSpeed` / `HeadingMaxRotationSpeed` (rad/s) | Mac setters 0xF9771C / 0xF9772E; `UpdateOrientation` 0x5FFC40 |
| +0x8C0 / +0x8C4 | float | `HeadingMinRotationAngle` / `HeadingMaxRotationAngle` (rad) | Mac setters 0xF97740 / 0xF97752 |
| +0x8D0 | int | `HeadingRotationDirection` (`IHumanGround::RotationDirection`: 0 Shortest, −1 Left, 1 Right) | Mac setter 0xF97764; `UpdateOrientation` |
| +0x8D4 [+0x8B4] | bitfield | bit 0 `m_ForceTurnAround` (`Has180TurnIntention`); Revelations packs crouch / hide-stance flags in the same byte | `_UpdateMoveStop` 0x621C80 |
| +0x8C8 / +0x8CC | float | `m_SavedHeadSpeed` / `m_SavedBodySpeed`: 15.0 / 12.5 at construction | ctor 0x6317A0 |
| +0x990 [+0x970] | float | `TurnOnMovementMinAngle` (clamped ≤ π; π/2 at construction) | Mac setter 0xF9825E; ctor |
| +0x994 [+0x974] | `TransitionToMotionType` | **`m_TransitionToMotionTypeInternal`**: the speed restriction (BlendWalkToJog at construction) | `UpdateMoveCycleMain`; ctor |
| +0x998 | u8 | `m_DoubleTransitionToMotionSpeed`: bit 0 plays the entry action at 2 − ratio (section 5) | `UpdateMoveCycleMain`; `ExitMoveCycle` |

Interfaces: PC HumanGround has eight vftable pointers (+0x00 primary 0x19BFCD8, +0x14 `IHumanGround` 0x19BF390,
then +0x18…+0x2C, ctor 0x6317A0). The Mac `IHumanGround` thunks adjust `this` by 20, so `IHumanGround` is at +0x14
in both builds.

## 3. The state machine
Generated code. `LinkFSM` gives every state an id; a simple state is a 3-byte record
`{id, initialChild, currentChild}`, and a sub-machine is an 8-byte `FSMClassStateWrapper<State, HumanGround>`
(its id byte at +5 on PC). Each state *X* has `_UpdateX(long phase)` (phase 0 = main update, 1 = after
physics), which runs `UpdateXMain`, then tests its transitions in order (`TestAToBMain` → `_TransitFromAToBMain`),
then dispatches to the current child's `_Update`. 147 ids in all.

Locomotion branch (ids from `LinkFSM`; PC dispatcher / Mac dispatcher):

```
HumanGround (root, +0x41/+0x42)               _UpdateHumanGround 0x6381D0 [0xFED468]
└─ Vulnerable 1                               _UpdateVulnerable  0x637D40 [0xFED1B0]  → InAir, → HayStack
   ├─ Navigation 2                            _UpdateNavigation  0x6371E0 [0xFE35FA]  → InAir (boat collision)
   │  ├─ Move 3                               _UpdateMove        0x634D20 [0xFE069C]  → OrientedMove, Poisoned, InAir
   │  │  ├─ Wait 4                            _UpdateWait        0x621A80 [0xFC5FDC]  → TurnStart, MoveCycle, TurnOnSpot, CrouchWait
   │  │  │  └─ Idle 5, LookDown 8, HumanGroundSearch 10, Loot 11 (+ their children)
   │  │  ├─ MoveCycle 15                      _UpdateMoveCycle   0x621B90 [0xFE05A2]  → MoveStop, Wait, TurnStart, CrouchMove
   │  │  │  └─ (default 16), Charge 17, PushSoft 20, PushCrowdShove 21
   │  │  ├─ MoveStop 22 0x621C80 · Roll 23 0x621D10 · HandPassOverReception 24 0x621DB0
   │  │  ├─ Landing 25 0x630800 · LandingHard 26 0x630890 · LandingBalanceFall 27 0x630940
   │  │  ├─ RunTurn 28 0x621E60 · TurnStart 29 0x621EE0 · TurnOnSpot 30 0x623F20 · Shocked 32 0x623F40
   │  │  └─ CrouchWait 35 0x623FA0 · CrouchMove 36 0x61E620 · CrouchTurn 37 0x5FA510
   │  ├─ JumpImpulsion 38 0x624060 · JumpImpulsionStraight 39 0x6240A0 · JumpImpulsionForwardFromIdle 40 0x5FA570
   │  ├─ WallingImpulsion 41 0x603270 · ObstacleCollision 42 0x6240F0 · EdgeStop 43 0x624150
   │  ├─ OrientedMove 44 (sub-machine `OrientedMoveState`) 0x6032B0 · CarryObject 45 · CustomAction 46 · Acting 47
   │  └─ Frozen 48 · HumanGroundDodge 49 0x624200 · Rest 50 · StealedSurprise 61
   ├─ EdgeLanding 62 0x626410 · GetUp 63 · Fight 64 · ... (combat, steal, push, grab, poison: out of scope)
├─ HumanGroundHurt 139 · HurtFight 140 · Agonizing 141 · (142) · Assassinate 143 · GetAssassinated 144
└─ Tackle 145 · GetTackled 146
```

Ids not listed belong to children of states not yet walked (e.g. Idle's IdleStill / IdleBreaker,
MoveCycleDefault).

## 4. `HumanGroundData` is runtime state
`HumanGroundData` (the DataContext's context data, at `this+0x10`) holds the per-frame inputs and outputs and
some tuning. It is a reflected class (section 15): the names of its members up to +0xF3 are the game's own, from
the reflection table; the members after that aren't reflected and are named here after what they do. Offsets are
the same in both builds; each was read on PC in the function named.

| Offset | Type | Meaning | PC evidence |
|---|---|---|---|
| +0x10 | vec4 | `DestGlobalBodyOrientation`; zeroed after use in `UpdateMoveCycleMain` | `UpdateMoveCycleMain` 0x61BB60 |
| +0x20 | vec4 | **`DestHeading`**: the direction the controller asks to move in (the turn angle and `UpdateOrientation` turn towards it) | `UpdateTurnAngle` 0x5FE6A0, `UpdateOrientation` 0x5FFC40 |
| +0x80 | enum | **`SubState`** (`HumanGroundSubState`: 0 Movement, 1 Fight, 2 FreeRun, 3 OrientedMove → OrientedMove, 4 Hurt, 5 ObstacleCollision) | `_UpdateMove` 0x634D20 |
| +0x84 | float | `CurSpeedRatio` (zeroed by `LocalInit` / `EnterMove`) | `EnterMove` 0x60C610 |
| +0x88 | float | **`DestSpeedRatio`**: the speed ratio the controller asks for (the player's: the stick); input to the movement division and the target ratio | `UpdateMovementIntention` 0x5E03E0 |
| +0x8C / +0x90 | float | `HeadingLinearRotationSpeed` / `HeadingEasedOutRotationSpeedRatio` | `LocalInit` |
| +0x94 / +0x98 | u32 | `ParamFlags` / `InternalFlags` (`EnterMove` sets bit 1 in both) | `EnterMove` |
| +0xB4 / +0xB8 | float | `CurrentBodyAngle` / `CurrentBodyBankingAngle`, smoothed with the sight (`ComputeVectorRotationWithSight` 0x5FE3D0); they weight the move cycle's turn variants | `UpdateMoveCycleMain` |
| +0xED | bool | **`BreakHard`**: while running, sends MoveCycle to MoveStop. Set by `IHumanGround::SetBreakHard` (PC 0x632E20 [0x153B958]) | `TestMoveCycleToMoveStopMain` 0x5EAAF0 |
| +0xEE | bool | **`Sprint`**: +0.2 to the target speed ratio (high profile + sprint = 0.8..1). Set by `IHumanGround::SetSprint` (PC 0x632E80 [0x153B9D0]); for the player, on while legs (Space, the fast walk) is held (live row 5) | `UpdateMoveCycleMain` |
| +0xF3 | bool | **`Crouch`** (→ CrouchMove / CrouchWait). Set by `IHumanGround::SetCrouch` [0x153B994] | `LocalInit` 0x61F920 (cleared); Mac `_UpdateMoveCycle` |
| (+0x30…+0xF2) | | the other reflected members (collision, look-at sights, combat style, block cylinder, acting, custom-action channels): section 15 | |
| +0x150 | `IHumanGround::HumanGroundStage` | `m_HumanGroundStage`; `EnterMove` sets 1 (Navigation) | `EnterMove` 0x60C610 |
| +0x154 | u8 | `m_NbMvtTypes`: 2 after `LocalInit` (Low, High) | `LocalInit` |
| +0x155 / +0x156 | u8 | `m_NbMvtDivisions[2]`: threshold counts (movement type Low / High), 3 after `LocalInit` | `LocalInit`; Mac `GetMovementDivisionFromSpeedRatio` 0x1119F44 |
| +0x158 / +0x16C | float[5] | `m_MvtDivThreshold[2][5]`, {0, 0.9, 1} after `LocalInit` | same |
| +0x180 | u8 | `m_CurMvtType`, cleared by `LocalInit` | `LocalInit` |
| +0x181 | u8 | **`m_DestMvtType`** (→ `m_IntentionMvtType` +0x7B4); for the player, 1 (High) while high profile (right mouse, the run) is held (live row 5) | `UpdateMovementIntention` 0x5E03E0 |
| +0x1A0 | `MovementState` | **`m_DestMovementState`**: how `EnterMove` starts (section 11) | `EnterMove` 0x60C610 |
| +0x1A8 / +0x204 | `ActionTransition` / bool | `m_DestTransition` / `m_IsCustomTransitionSet` | Mac `SetupTransitionData` 0x111AAC6 |
| +0x20C / +0x210 | u32 / `ACTFeetPosition` | `m_DestTransitionFlags` (passed to `SetTargetAction`) / `m_DestFeetPos`. `SetupTransitionData` copies the setup's +0x0C / +0x10 here, and the AC2 ctor (Mac 0x150EF4C) stores its feet-position argument at +0x10, so the PDB order holds | Mac `SetupTransitionData` 0x111AAC6; `EnterMoveCycle` 0xFBE706 |
| +0x22C | bool | `m_TeleportToGroundSetup`: `EnterMove` resets the character component when set | `SetupTransitionData`; `EnterMove` 0x60C610 |
| +0x250 | `EntryState` | `m_EntryState`; `SetupTransitionData` resets it to 0 (`EntryState_Move`) | Mac `SetupTransitionData` 0x111AAC6 |
| +0x214 | float array | `m_DestBlendFactors` for the first action | same |
| +0x21C | float | `m_RotationRatio` (scales the heading rotation angles); `SetRotationRatio` 0x90E250 | `UpdateOrientation` |
| +0x220 | float | `m_RotationRatioStep` per second; Mac `SetRotationRatioStep` 0x1119FAE; `ExitMove` sets ratio 1, step 0.5 | `UpdateOrientation`; Mac `ExitMove` 0xFAFB48 |
| +0x224 | bool | `m_SkipActionOnStateEntry` (1 after `LocalInit` and for `MovementState_MAX`) | `LocalInit`, `EnterMove` |
| +0x228 | `TransitionToMotionType` | **`m_TransitionToMotionType`**: `EnterMoveCycle` copies it into `m_TransitionToMotionTypeInternal` (+0x994); reset to BlendWalkToJog | Mac `EnterMoveCycle`, `ResetTransitionData` 0x1119FE2 |
| +0x22C | u8 | a transition flag; when set, `EnterMove` zeroes the character controller's local offset | `EnterMove` |

## 5. MoveCycle: the speed model (`UpdateMoveCycleMain` PC 0x61BB60 [0xFCD20A])
Ground speed is **animation-driven**: the code computes a speed ratio and blends the MoveCycle action's items; the
character moves by the blended animations' root motion. Speeds in m/s come from the animation data, not from
code constants.

1. **Target ratio** *t*:
   - AI override on (+0x8A8): *t* = +0x7C0;
   - else *t* = `DestSpeedRatio` × 0.2 + base, base = 0 (walk), 0.2 (Sprint), 0.6 (high profile), 0.8 (high profile and
     Sprint; the player also emits witness event 26 at most every 0.5 s);
   - clamped to ≤ 1.
2. **When the current action isn't the move cycle** (action id != 93469659 and not finishing): non-player humans
   are held to [0.2, 0.6]; the restriction mode (+0x994) limits *t*: 0 → 0.6; 1, 4 → [0.2, 0.6]; 2 → 0.2 below a
   ratio of 0.6, else 0.6; 3 → [0, 0.6]; 5, 6 → ≥ 0.2 and ≤ 0.6 (≤ 1 with +0xEE); 7 → ≤ 0.6 (≤ 1 with +0xEE);
   without high profile *t* ≤ 0.2. The ratio then moves toward *t* by `step` per frame (no curve), and the function
   returns before the bands: gait and running keep their values. With +0x998 bit 0 (set by
   `ExecTurnStartToMoveCycleMain`'s high-profile branch, cleared by `ExitMoveCycle` PC 0x60DB70 [0xF9FD94]) the current
   action plays at 2 − its ratio. **Live** (row 8): this is the 0.68 s / 0.50 s hold after TurnStart → MoveCycle: the
   restriction mode is 3 (`TransitionToMotionType` 3 from the Exec), so the ratio stays at 0.2 (no high profile) or
   0.6 (high profile) until the move cycle action becomes current.
3. **In the move cycle:** rising, ratio += `step` (at most *t*); falling, the player's ratio drops by
   `step × curve(ratio)`, not below *t*; the AI's drops by `step`. The curve is the `ResponseCurve` at PC +0x81C,
   piecewise linear (`ResponseCurve::GetOutput` Mac 0x507D10) through the keys `LocalInit` (0x61F920) adds:
   (0, 1), (0.333, 1), (0.4, 0.3), (0.666, 0.2), (1, 1). So slowing from a run is slow between ratios 0.4 and 0.67
   and quick elsewhere.
   `step` = the float at entity → +0x6C → +0x5D8 on PC [+0x60 → +0x8A8 on Mac], most likely the frame dt
   **(hypothesis; LIVE row 4)**.
4. **Bands** (ratio → blend slots of the MoveCycle action; *f* = position inside the band):

| Ratio | Slots | Gait level +0x7A8 | Running +0x7B0 |
|---|---|---|---|
| 0 – 0.2 | 0 → 3 | 1 below 0.1, else 2 | 0 |
| 0.2 – 0.4 | 3 → 6 | 2 | 0 |
| 0.4 – 0.6 | 6 → 9 | 1 | 1 |
| 0.6 – 0.8 | 9 → 13 (+ slot 12 weighted by +0x8AC) | 2 | 1 |
| 0.8 – 1.0 | 13 → 16 (+ slot 19 weighted by +0x8B0, fading out at full sprint) | 2 | 1 |

   Each band's base slot has left / right turn variants at +1 / +2, weighted by `CurrentBodyAngle` /
   `CurrentBodyBankingAngle` (`HumanGroundData` +0xB4 / +0xB8, normalised by π/2, clamped to 1).
5. At ratio ≥ 0.8 the upper body's action is ended; below 0.6 the upper body may start a "cycle breaker"
   (random delay from `HumanData` +0x120..+0x12C).

## 6. Locomotion transitions (Move's children)
`UpdateMoveMain` (PC 0x604990 [0xFCA184]) runs first every frame: `PreprocessCharge` 0x5ED4E0,
`UpdateMovementIntention` 0x5E03E0, `EmitPresentationMovementType` 0x5F3A30, copies the sight to +0x7E0,
`UpdateTurnAngle` 0x5FE6A0, `UpdateOrientation` 0x5FFC40 when the gait level is non-zero, then the push handler.

**Movement division** (`UpdateMovementIntention`): with the AI override, 2 if the AI ratio > 0 else 0. Otherwise
`HumanGroundData::GetMovementDivisionFromSpeedRatio(highProfile, DestSpeedRatio)` (0x90E190) returns the first index
*i* below the table's count whose threshold ≥ DestSpeedRatio (0 if none); index 0 → division 0, 1 or 2 → division 2
(other indices leave it unchanged). With the {0, 0.9, 1} tables this is **0 when DestSpeedRatio is exactly 0, else
2**. The indices are `HumanGroundData::MvtDivisions` (0 WaitLow, 1 WaitHigh, 2 WalkVerySlow, 3 Walk, 4 Jog, 5 Run,
6 Crouch).

**Can an action be cut by movement** (`IsActionInterruptibleByMovement` 0x5ECD30): never if the current action item's
flags (+0xC) have 0x20; else the flag bit chosen by (division, high profile): (0, off) 6, (0, on) 7, (1, off) 8,
(1, on) 10, (2, off) 9, (2, on) 11. Below, "free" = `ActionComponent::WillActionFinish` or that test.

| From → to | Test (PC [Mac]) | Condition |
|---|---|---|
| Wait → TurnStart | 0x5F4EC0 [0xF9EA82] | free, \|turn angle\| > `TurnOnMovementMinAngle`, byte +0x7A0 clear |
| Wait → MoveCycle | 0x5F4E70 [0xF9EAE6] | division 1 or 2, free |
| Wait → TurnOnSpot | 0x5F4F30 [0xF9E9F8] | +0x464 == 1, custom-action orientation check, free, division 0, \|turn angle\| ≥ 1° |
| Wait → CrouchWait | 0x5F4FC0 | not decoded |
| MoveStop → RunTurn | inline in 0x621C80 [0xF9DA6C] | `Has180TurnIntention` (force turn-around +0x8D4, or \|turn angle\| > 2π/3) and `WillActionFinish` |
| MoveStop → Wait | 0x5F5B70 [0xF9E88E] | (`HasActionFinished` or interruptible) and division 0 |
| MoveStop → MoveCycle | 0x5F5BB0 [0xF9E840] | (`HasActionFinished` or interruptible) and division 1 or 2 |
| TurnStart → Wait | inline in 0x621EE0 [0xF9DDD6] | division 0 and `WillActionFinish` |
| TurnStart → MoveCycle | inline in 0x621EE0 [0xF9DE14] | division ≠ 0 and `WillActionFinish` |
| RunTurn → Wait | inline in 0x621E60 [0xF9DD4A] | division 0 and `HasActionFinished` |
| RunTurn → MoveCycle | 0x5EB690 [0xF9DD88] | division ≠ 0 and (`HasActionFinished` or no 180° intention any more) |
| TurnOnSpotStep → TurnOnSpotStep | 0x5FE0A0 [0xFB3684] | the turn-on-spot cycle flag (+0x8BD Mac) set, action finished, and time left for the next turn animation |
| TurnOnSpotStep → Wait | [0xF9D8FE] | cycle flag clear and `HasActionFinished` |
| MoveCycle → MoveStop | 0x5EAAF0 [0xFB5CAE] | running, gait level ≠ 0, item flag 0x20 clear, `HumanGroundData`+0xED, and the action is MoveCycle (93469659), 96 or 97, or past 33 % |
| MoveCycle → Wait | 0x5EAB90 [0xFB5D7A] | division 0, and not running or the action isn't MoveCycle |
| MoveCycle → TurnStart | 0x5F5B00 [0xF9E8DA] | not running, \|turn angle\| > `TurnOnMovementMinAngle`, free |
| MoveCycle → CrouchMove | 0x5E1AC0 → 0x61F1D0 [inline in 0xFE05A2] | `HumanGroundData`+0xF3 (crouch) set and not running [Mac `this[484]`, the running flag] |

Each state's tests run in the table's order and the first that passes wins.

## 7. Turning (`UpdateOrientation` PC 0x5FFC40 [0xFB4E32])
Ground turning rotates the entity's **sight** (its facing) towards `DestHeading` (`HumanGroundData`
+0x20), every frame the gait level is non-zero:

1. rotation ratio *r* = min(1, `HumanGroundData`+0x21C + dt × +0x220), stored back (`SetRotationRatio`);
2. unless the current action item's flags have 0x10, and neither vector is null (ε 0.0005):
   sight = `RotateVectorToVectorWithAngularSpeed`(sight, moveDir, `HeadingMinRotationSpeed`,
   `HeadingMaxRotationSpeed`, `HeadingMinRotationAngle` × *r*, `HeadingMaxRotationAngle` × *r*,
   `HeadingRotationDirection`, dt);
3. the sight is written back (`BaseEntity::SetSightGeneral`, up = +Z) if it moved by more than 0.001.

`RotateVectorToVectorWithAngularSpeed` (PC 0x5FF930 [0xFB4A3C]): with *a* the angle still to turn,
speed = minSpeed if *a* < minAngle; a linear blend to maxSpeed between minAngle and maxAngle; maxSpeed from
maxAngle on. It rotates by speed × dt, and returns the target itself when that would reach or pass it. The
direction is the shortest way, unless `HeadingRotationDirection` ≠ 0 forces the side (then *a* is measured
that way round, 0..2π).

**The heading values come from outside.** The constructor sets all four to 0; they are only written through
`IHumanGround`'s setters (Mac `SetHeadingMin/MaxRotationSpeed`, `SetHeadingMin/MaxRotationAngle`,
`SetHeadingRotationDirection`, `SetTurnOnMovementMinAngle`; PC `IHumanGround` vftable 0x19BF390), i.e. by the
player's or the AI's controller. One state sets its own (PC 0x617E40: min 0.2, max 0.4 rad/s). The player's
speeds come from `AssassinTask::UpdateGroundMovement` (`21_input.md` 8); the angles' setter is open (live row 6).

## 8. Construction and `LocalInit` defaults
- ctor (0x6317A0): heading speeds / angles 0, `HeadingRotationDirection` 0, `TurnOnMovementMinAngle` π/2,
  speed restriction 1, +0x8C8 15.0, +0x8CC 12.5.
- `LocalInit` (0x61F920): `HumanGroundData` `SubState`, `DestHeading`, `CurSpeedRatio`, `DestSpeedRatio`,
  `HeadingLinearRotationSpeed`, `ParamFlags`, `InternalFlags` zeroed; the
  movement-division tables and the slow-down curve above; +0xED / +0xEE / +0xF3 / +0x180 / +0x181 cleared;
  turn angle 0. The character capsule: height 1.8, radius 0.4, and two offsets at height − 0.4 and one at
  height − 0.15 (`CharacterComponent` +0x64 / +0x68 / +0x6C from +0x78) **(hypothesis: capsule fields)**.

## 9. Open
- [x] Who calls `IHumanGround`'s heading setters, `SetBreakHard` and `SetSprint` for the player:
      `AssassinTask::UpdateGroundMovement` PC 0xDF7FE0 (`21_input.md`, 2026-10-21). Still open: who sets the heading
      rotation *angles*.
- [x] The hold of the speed ratio after MoveCycle starts: the restricted branch while the move cycle action isn't current (section 5, live row 8, 2026-10-21). Still open: which action is current during it.
- [x] TurnOnSpot: the custom-action check, `m_TurnMode`, the cycle flag and the target (section 13, 2026-10-21).
      Still open: who calls `DoTurnOnSpot` / `SetUseTurnOnSpotCycle` (the look-at and custom-action systems).
- [x] CrouchWait's transition check and `ActorCanPhysicallyStandUp` (section 13, 2026-10-21).
- [x] InAir's side of the landing hand-over (`HumanInAir::ExecMainToGroundMain`, section 11, 2026-10-21). Still open:
      EdgeLanding; `ComputeFallDamage`; the spring jump. (InAir's `m_LandingSpeedRatio`: `AssassinTask::UpdateInAir`,
      `21_input.md`.)
- [x] Edges outside free-run: edge clamping (section 16, 2026-10-22), decoded and ported. Still open: the
      high-profile case live (row 12), `GetDepthInFrontOfEdge` in full, the edge-jump decision.
- [x] Who sends the edge commands (LookDown, UnbalanceOnEdge, FallOffEdge, LedgePullDown) and the jump to target:
      the player's controller (section 17, `21_input.md`, 2026-10-22). Not ported yet.
- [ ] Who sends the other jump / obstacle interface commands (the free-run / guidance system); `CanWallingVertical`,
      `TestForPotentialObstacleCollision`, `IsNarrowEdge`.
- [x] `HumanGroundData`'s property names (section 15, 2026-10-21).
- [ ] The actions' animations: root-motion speeds and turns, lengths (needed for the port's speeds, turns and the
      animation-driven states).

## 10. Animation-driven states: MoveStop, TurnStart, RunTurn, TurnOnSpot
These states play one action and leave when it ends (`ActionComponent::WillActionFinish` = ends this frame,
`HasActionFinished` = has ended; PC 0x165C90 / 0x165C20) or when movement may cut it; the movement division picks
where they go (section 6's table). The turn or stop itself is the animation's root motion.

- **TurnStart** (id 29; entered from Wait or MoveCycle when \|turn angle\| > `TurnOnMovementMinAngle`):
  `EnterTurnStart` (PC 0x614BB0 [0xFBE3D6]) plays `g_TurnStartActionMap[6 × side + 3 × running + foot]` (foot only
  for the player, `GetCurrentFoot` PC 0x5ECDE0) blended 1−f / f with f = (\|a\| − π/2) / (π/2), i.e. between a
  90° and a 180° turn; a smaller angle uses a 3-item blend by \|a\| / (π/2). It stores the side (+0x468) and the
  angle (+0x480), sets high profile := running then running := high profile, and clears the gait level (so
  `UpdateOrientation` doesn't run during the turn). `UpdateTurnStartMain` (PC 0x5EB6F0 [0xFB65B6]) plays it at
  twice the speed in high profile, except the four running turns.
- **MoveStop** (id 22; from MoveCycle when running and BreakHard): `EnterMoveStop` (PC 0x614370 [0xFB7804]) plays
  `g_MoveStopActionMap[foot]`, a 3-item blend set from the speed ratio it stops from (from the move cycle:
  (0, 1 − f, f) with f = (ratio − 0.8) / 0.2 above 0.8; otherwise by how far the previous action got past 33 %).
- **RunTurn** (id 28; from MoveStop with a 180° intention): `EnterRunTurn` [0xFB76DE] plays
  `g_RunTurnActionMap[foot]` and sets the entity's banking.
- **TurnOnSpot** (id 30, child TurnOnSpotStep 31; from Wait): `StartTurnOnSpotAnim` (PC 0x606B70 [0xFB37D0]) picks a
  turn with `GetTurnAnimation` (PC 0x5FDF00 [0xFB3550]): the signed angle from the sight to the target about +Z,
  \|a\| < 25° → 0 / 1, < 70° → 2 / 3, < 135° → 4 / 5, else 6 / 7 (even for a positive angle); then
  `PlayTurnAnimation` (PC 0x5EA850) with weight \|a\| / (15°, 45°, 90° or 180° for the four sizes) and a 0.2 s
  blend. Action ids 0–7: 788325963, 788325965, 660761273, 660761275, 660761277, 660761279, 660761269, 660761271.

**Where the speed ratio is set.** `ExitMoveCycle` [0xF9FD94], `EnterWait` [0xFBEA66] and `EnterMoveCycle` leave it
alone, so MoveCycle → Wait keeps it (live: 0.196 in Wait after a walk). The transitions' Exec functions set it:
- `ExecMoveStopToWaitMain` (Mac 0xF9748A, inlined in PC `_TransitFromMoveStopToWaitMain` 0x61F200): +0x7B4 := 1,
  ratio := 0;
- `ExecTurnStartToWaitMain` (PC 0x614F40 [0xFB664C]): +0x7B4 := running (+0x7B0); the wait action
  `g_GroundWaitActionMap[12·side + 6·running + 3·foot]` (another map for six current-action ids), blended by
  (\|turn angle\| − π/2) / (π/2); ratio := 0;
- `ExecTurnStartToMoveCycleMain` (PC 0x615120 [0xFB7474]): action 0x5923BDB; if running, ratio 0.6 and
  `TransitionToMotionType` 2; otherwise ratio 0.6 with +0x7B4 (and +0x998 |= 1), 0 for the player at division 1,
  else 0.2, and `TransitionToMotionType` 3;
- `EnterMove` whenever Move is entered (section 11).

Live (2026-10-21, `20_live_checks.md`): ratio 0 in Wait after MoveStop and after TurnStart; 0.2 / 0.6 and
restriction 3 at the start of a walk / high-profile move cycle; +0x7B4 at 1 for one frame after MoveStop → Wait.

**Action maps** (filled once by `HumanGround::InitializeGlobalActions`, PC 0x633E90 [0xFA755A]; ids are either
small aliases or hashes):

| Map (PC address) | Index | Ids |
|---|---|---|
| `g_GroundWaitActionMap` (0x1E13600) | [3][2][2][3] | 48, 49, 50, 51, 52, 53, 0xF0000049, 0xF000008C, 15963158, 15963159 (the rest −1) |
| `g_MoveStopActionMap` (0x1E13694) | [foot] | 86, 87 |
| `g_RunTurnActionMap` (0x1E136A4) | [foot] | 94, 95 |
| `g_TurnStartActionMap` (0x1E13740) | [side][running][foot] | side 0: 137346172, 137346183 / running 161521137, 161522199; side 1: 137346184, 137346185 / running 161521138, 161522200 |
| `g_JumpStraightAnticipationActionMap` (0x1E136B4) | [foot] | 233845637, 250429907 |
| `g_JumpStraightAnticipationCancelActionMap` (0x1E136C4) | [foot] | 88, 17407127 |
| `g_JumpImpulsionStraightActionMap` (0x1E136D4) | [foot] | 89, 17407126 |
| `g_JumpImpulsionRunActionMap` (0x1E136E4) | [foot] | 9, 9 |
| `g_HandPassOverReceptionActionMap` (0x1E136F4) | [foot] | 172796614, 172796613 |
| `g_CrouchWaitActionMap` (0x1E13704) | [3][2] | 17228607, … |
| `g_CrouchTurnActionMap` (0x1E13730) | [2] | 314169075, 314169075 |
| the move cycle | | 93469659 (0x5923BDB) |

## 11. Entering Move; landings and Roll
**The hand-over into HumanGround.** The context handing over (InAir after a fall, the climb contexts, …) builds a
`TransitionSetupDataToMovement` (ctor Mac 0x150EF4C: `MovementState`, `ActionTransition*`, feet position, a
flags word, blend factors, rotation ratio, rotation-ratio step, a bool, `TransitionToMotionType`, a bool) and
`SetupTransitionData` (Mac 0x111AAC6) copies it into `HumanGroundData` (+0x1A0, +0x1A8 / +0x204, +0x20C, +0x210,
+0x214, +0x21C, +0x220, +0x224, +0x228, +0x22C). `ResetTransitionData` (Mac 0x1119FE2) puts MovementState back to 14
and `TransitionToMotionType` to 1; `EnterMoveCycle` calls it after copying the motion type into the speed
restriction mode.

`EnterMove` (PC 0x60C610 [0xFA3D06]) runs when Move is entered: `PushState(4)`, `HumanGroundData`+0x150 := 1,
`SubState` := Movement, `DestHeading` := the entity's sight, `CurSpeedRatio` / `DestSpeedRatio` := 0,
`ParamFlags` / `InternalFlags` |= 1, then a switch on **MovementState**
(+0x1A0) picks Move's first child and seeds the gait fields; afterwards high profile := running and division :=
gait level. `_EnterMove` (PC 0x621720 [0xFD43E0]) then enters that child (PC inlines most children's Enter).

| `MovementState` (PDB names) | First child | Running | Gait level | Speed ratio |
|---|---|---|---|---|
| 0 WaitLow | Wait | 0 | 0 | 0 |
| 1 WaitHigh | Wait | 1 | 0 | 0 |
| 2 WalkVerySlow | MoveCycle | 0 | 1 | 0 |
| 3 Walk | MoveCycle | 0 | 2 | 0.2 |
| 4, 5 Jog, Run | MoveCycle | 1 | 2 | 0.6 |
| 6 Sprint | MoveCycle (motion type 1 → 5) | 1 | 2 | 1.0 |
| 7 Crouch | CrouchWait | 0 | 0 | 0 |
| 10 Landing | Landing | 0 | 0 | 0 |
| 11 LandingHard | LandingHard | 0 | 0 | 0 |
| 12 LandingBalanceFall | LandingBalanceFall | 1 | 0 | 0 |
| 13 HandPassOverReception | HandPassOverReception | 1 | 2 | 0.6 |
| 14 MAX (none) | Wait (`m_SkipActionOnStateEntry` := 1) | 0 | 0 | 0 |

`MovementState`'s names are the Revelations PDB's (`HumanGroundData::MovementState`). AC2 numbers 0–7 like
Revelations; its 10–13 are Revelations' 11–14 and 14 is `MovementState_MAX`, so AC2 has two of Revelations'
Collide / CarryObject / CustomAction at 8 and 9. **8 is Collide**: `HumanNarrowObjectBeam::
ExecMoveToGroundOnHumanNarrowObjectInterfaceCommand_Collide` (Mac 0xF211E8) hands over with 8. Whether 9 is CarryObject
or CustomAction isn't decoded (no caller seen with 9). `EnterMove` has no case for either, so they keep Move's
previous child. A separate reflected enum `LandingType` (Safe,
SmallDamage, HeavyDamage, Fatal) is InAir's, and most likely what picks 10 / 11 / 12.

**The landing from InAir** (`HumanInAir::ExecMainToGroundMain` PC 0x69B730 [0x1236156]) picks the landing action
and the `MovementState`:
1. With InAir's `m_DoAirAssassination` / `m_DoAirTackle` (InAir +0x181 bits 0x20 / 0x40) it only sets
   `HumanGroundData::m_EntryState` to 2 / 10 (`EntryState_AirAssassination` / `_AirTackle`) and stops.
2. The drop is `HumanInAirData::JumpOrigin`'s height (the matrix at +0x10, translation at +0x40) minus the landing
   height. In a spring jump (current action 0xDA3ACAAF on PC; `IsInSpringJump` PC 0x6925D0 on Mac) or **above 3.0 m**:
   **LandingHard** (11). The action is 0x10DD707 when InAir's `m_LandingSpeedRatio` (+0x21C) is ≤ 0.2 or
   `ComputeFallDamage` (PC 0x6927A0) returns 2, else 0x10DD70B (0x256801CC in a spring jump). Then fall damage
   (`ConvertLandingToDamageAndSendEvent` PC 0x69AF20), a witness event, a sound and a dust FX scaled by
   (drop − 3) / 7.
3. Otherwise **Landing** (10). A steep fall (no horizontal speed, or more than 75° between it and the velocity with z
   clamped ≤ 0) gives 1855747415 (no landing speed) or 1855746090. Otherwise the landing speed band
   (`m_LandingSpeedRatio` < 0.2 / 0.5 / 0.9 / else) picks a standing landing 1855747402 / 1855747404 (by
   `Human::GetFrontFoot` PC 0x9183D0) or a moving one 1830814048 + foot, blended by min(drop / 2.5, (horizontal
   distance − 5) / 7.5, 1).
4. Both hand over through `TransitionSetupDataToMovement` (`SetupTransitionData` PC 0x90EFA0) with rotation ratio 0,
   step 0.5 and `TransitionToMotionType_BlendWalkToJog`.

So Ground's Landing → Wait (the three standing ids, below) happens exactly when InAir had no landing speed.

**The landing states** play the action the handing-over context set up and leave when it ends:

| State | Exit | Condition |
|---|---|---|
| Landing (PC `_UpdateLanding` 0x630800) | → Poisoned | action finished and the human is poisoned |
| | → Wait (0x5EB330 [0xFB5FA6]) | action finished and it is one of the standing landings 1855747402 / 1855747404 / 1855747415 |
| | → MoveCycle (0x5EB3A0 [0xFB602A]) | action finished, any other landing |
| LandingHard (0x630890) | → Poisoned / Wait / MoveCycle (0x5EB4C0 [0xF9DC96]) | will finish; Wait with division 0, MoveCycle otherwise |
| LandingBalanceFall (0x630940) | → Wait [0xF9DD20] | will finish |
| Roll (0x621D10) | → Wait / MoveCycle [0xF9DAB2 / 0xF9DAF6] | will finish; by division |

Into MoveCycle, the Exec functions seed the cycle and set rotation ratio 0 with step 0.5 (turning ramps back in
over two seconds):
- `ExecLandingToMoveCycleMain` [0xFA920A]: high profile and Sprint → ratio 1.0, MovementState 6; high profile → 0.6,
  state 4; otherwise 0.2, state 3; division 2; then `HumanGroundData`+0x228 := 6 (the speed restriction mode for the
  cycle).
- `ExecLandingHardToMoveCycleMain` [0xFA911E]: high profile → 0.6 (state 4), otherwise 0.2 (state 3).
- `ExecRollToMoveCycleMain` [0xFAE818]: 0.6 (state 4), with a transition action (alias 0x82FB27C).

**Roll** is entered by interface command only, accepted from MoveCycle when running at gait level 2
(`TestMoveCycleToRollInterface` [0xF96DB6]).

## 12. Jumps and edges (entry points)
These states start from **interface commands** sent to HumanGround (queued as member-function pointers at
`this+0x84`; each `_ExecInterfaceTransitionX` checks whether its command is pending). The senders (the free-run /
guidance code) are outside this doc.

| Command | From → to | Accepted when |
|---|---|---|
| `JumpImpulsion` | Move → JumpImpulsion [0xFB5B76] | not blocked (a vfunc and the item flag 0x20 clear) and either standing (gait 0) or the current action is not the move cycle and is under 33 % (stored at +0x45C) |
| `JumpToTarget` | Move / JumpImpulsion → JumpImpulsionForwardFromIdle [0xFB5A9E / 0xF98A14] | not decoded |
| `StraightJump`, `OverHeadJumpToTarget` | JumpImpulsion → JumpImpulsionStraight [0xF98ADA / 0xF98A98] | not decoded |
| `EnterWallingVertical` | JumpImpulsion / ObstacleCollision → WallingImpulsion [0xFD2AAA / 0xFD2D30] | not decoded |
| `UnbalanceOnEdge` | Move → EdgeStop [0xFCBF64] | no hurt fall, `IsNarrowEdge` = 1, the gap's drop (+0x30) > 2 m, `Human::TestPullDownToLedgeClearance` passes |

- **JumpImpulsion** (`EnterJumpImpulsion` [0xFC2CE2]): plays `g_JumpStraightAnticipationActionMap[foot]`, starts a
  0.2 s timer (+0x2C0 PC, × the factor at +0x45C), emits witness event 14. Each frame it updates the movement
  intention; back to Move (`TestJumpImpulsionToMoveMain` PC 0x5EB850) on crouch without high profile, or when the
  timer runs out with division 2; otherwise a command above picks the jump.
- **JumpImpulsionStraight** (`EnterJumpImpulsionStraight` [0xFC2EA8]: `g_JumpImpulsionStraightActionMap[foot]`):
  → Move if the foothold (+0x51C PC) has been unloaded; → **InAir** (`TestJumpImpulsionStraightToInAirMain`
  PC 0x5FE750) when the target action has finished.
- **EdgeStop**: plays the unbalance action 115916159, then loops 115916157 (`UpdateEdgeStopMain` PC 0x5E8FC0); →
  Move when the target action has finished (PC `_UpdateEdgeStop` 0x624150).

## 13. Crouching, TurnOnSpot, Idle
**Crouch** (`HumanGroundData`+0xF3, `IHumanGround::SetCrouch`):

| From → to | Test (PC [Mac]) | Condition |
|---|---|---|
| Wait → CrouchWait | 0x5F4FC0 [0xF9E9C2] | crouch, no high profile, the current action interruptible by movement |
| MoveCycle → CrouchMove | inline [0xFE05A2] | crouch and not running |
| CrouchWait → Wait | inline in PC 0x623FA0 [0xFDEBB8] | crouch released, interruptible, `ActorCanPhysicallyStandUp` (PC 0x5F72C0: `Human::CheckClearanceCapsuleGlobal` PC 0x934800 [0x12CA306] with a 0.38 m capsule from the feet + 1.42 m down to + 0.42 m) |
| CrouchWait → CrouchMove | 0x5EBDD0 [0xF9DF6A] | `m_IntentionMvtState` ≠ Wait and, while `m_IsCrouchTransitionFromExternal` (+0x8D5 [+0x8B5]) is set, the stand → crouch transition more than 75 % done: the transition action's ratio if one plays, else the current blend's time over its length (done when there is no blend) |
| CrouchWait → JumpImpulsion | 0x622580 [0xFC2E48] | high profile pressed |
| CrouchWait → CrouchTurn | 0x603690 | `Has180TurnIntention` (PC 0x5E1FA0) |
| CrouchMove → CrouchWait | inline | division 0 |
| CrouchMove → MoveCycle | inline [0xFDEB1A] | (crouch released or high profile) and can stand up |
| CrouchMove → CrouchTurn | 0x6036B0 | `Has180TurnIntention` |
| CrouchTurn → CrouchWait | inline in PC 0x5FA510 | the character controller's displacement is done (`DisplacementManager::GetProgressionRatio` PC 0x487340 ≥ 1) |

`ExecWaitToCrouchWaitMain` [0xFB9206] plays `g_CrouchWaitActionMap[2·foot + GetUseHideStance()]` with a 0.4 s
transition (through a hide-stance transition action when the hide stance is on) and sets
`m_IsCrouchTransitionFromExternal`; `ExecCrouchWaitToCrouchMoveMain` [0xFB6D70] plays the move through
`g_CrouchWaitToMoveActionMap[foot]` and clears it. The crouch command (interface command 135, `ExecCrouchWaitInterfaceCommand`
[0xFB6E6C]) replays the crouch wait action without setting it. `SetCrouched` [0xF97E58] (`m_IsCrouched`, presentation
event 31) is set by the crouch states' Enter and cleared by their Exit.

The crouch bytes at PC +0x8D4..+0x8D9 [+0x8B4..+0x8B9] follow Revelations' bitfield order; three are anchored by
code: `m_IsCrouched` (`SetCrouched`), `m_UseHideStance` (`GetUseHideStance` [0x153B800]) and `m_CrouchWasHidden`
(`EnterCrouchWait` stores `GetUseHideStance()` there), so +0x8D5 `m_IsCrouchTransitionFromExternal` and +0x8D6
`m_EnteredJumpImpulsionFromCrouch` take the PDB names by position.

`EnterCrouchWait` [0xFC316C] / `EnterCrouchMove` [0xFC3338] set the **capsule height to 1.0** (standing: 1.8,
`LocalInit`), running 0, gait 0 / 1, and restart a timer (0.5 s; `UpdateCrouchWaitMain` PC 0x5E1EC0 calls a
virtual every 0.5 s, `UpdateCrouchMoveMain` restarts it at 1 s). CrouchTurn plays `g_CrouchTurnActionMap`.

**TurnOnSpot** turns on the spot to `m_TurnOnSpotDir` (+0x8E0 [+0x8C0]). Its only child is TurnOnSpotStep (one turn
animation per step). Two ways in, both from Wait:
- **On its own** (`TestWaitToTurnOnSpotMain` PC 0x5F4F30 [0xF9E9F8]): `m_TurnMode` is NPC (1); not in a custom action
  that orients the character (`IsInCustomAction` [0xF9BC40]: the CustomAction state is active or the upper body,
  +0x790, plays a custom action; `GetCurrentCustomActionOrientation` PC 0x5F2320 [0xF9BDE0] must then be 2,
  `CustomActionOrientation_None`, not Aggressive / Progressive); the current action finishing or interruptible by
  movement; `m_IntentionMvtState` Wait; \|`m_TurnAngle`\| ≥ 1°. The target is `DestHeading`
  (`ExecWaitToTurnOnSpotMain` [0xF9939E], inlined in `_TransitFromWaitToTurnOnSpotMain` PC 0x61F0D0). The player's
  turn mode is MainCharacter, so this serves NPCs.
- **On request**: interface command 155 through `IHumanGround::CanTurnOnSpot(dir, minAngle)` [0xFDB938] and
  `DoTurnOnSpot(dir)` [0xFEBEDE]. Its test (Wait only, [0xFB3B88]) passes when the angle about +Z from the sight to
  `dir` is ≥ `minAngle` (`TestTurnOnSpotDeadZone` [0xFB3A6C]); its Exec [0xF98D70] takes `dir` as the target.

`StartTurnOnSpotAnim` (PC 0x606B70 [0xFB37D0], section 10) plays the turn animation for the angle to the target
(`GetTurnOnSpotAnimation` PC 0x5E1920 [0xF97B1C]). `_UpdateTurnOnSpotStep` (PC 0x621F90 [0xFD185A]):
- **→ TurnOnSpotStep** (`TestTurnOnSpotStepToTurnOnSpotStepMain` PC 0x5FE0A0 [0xFB3684]): the cycle flag
  `m_TurnOnSpotCycle` (+0x8DD [+0x8BD]) is set, channel 0's action has finished, channel 1 is busy and its action has
  more time left than the next turn animation lasts; then the target becomes the upper body's custom-action
  direction (`HumanUpperBody::GetCustomActionTargetDirection` PC 0x6408E0) and the next animation starts. When the
  time left is too short the flag is cleared instead; when channel 1 is idle nothing happens (the step holds).
- **→ Wait** ([0xF9D8FE]): the cycle flag is clear and channel 0's action has finished. `ExitTurnOnSpot` (PC
  0x60D7A0 [0xFA0944]: `DestHeading` := sight, flag cleared), then `ExecTurnOnSpotStepToWaitMain` (PC 0x5EA930
  [0xF98EFE]: `DestHeading` and `m_HeadingBeforeRotationUpdate` := sight, `m_CurrentMvtState` Wait, ratio 0,
  `m_IntentionMvtType` := `m_CurrentMvtType`, `m_IntentionMvtState` Wait).

`SetUseTurnOnSpotCycle` [0xF979FE] only takes while `IsInTurnOnSpotTurn` [0xF9BC7A] (TurnOnSpot active). So a caller
(the look-at / custom-action code) keeps the character turning in steps while an upper-body action plays.

**Idle** (Wait's child) alternates IdleStill (id 6) and IdleBreaker (id 7): a breaker plays when `HumanData`+0x133
[Mac] allows it, a timer (Mac +0x31C) has run out and `g_IdleBreakerActions` has a block for the actor
(`TestIdleStillToIdleBreakerMain` [0xFB341E]). Presentation only; nothing moves.

## 14. ObstacleCollision, WallingImpulsion, JumpImpulsionForwardFromIdle
- **ObstacleCollision** (Navigation's child) is entered by interface transition from Move when
  `Human::TestForPotentialObstacleCollision` [0x12EACEC] reports one ([0xF99344]). It leaves to Move when the
  `SubState` (`HumanGroundData`+0x80) is `SubState_Movement` [0xF984D0], to the NarrowObject context when a byte (Mac +0x6EA) is set
  [0xF96AA8], to InAir by interface unless the action item flag 0x20 is set [0xF9D314], to Push or Riding by
  interface, or to WallingImpulsion on `EnterWallingVertical`. `EnterObstacleCollision` [0xFCA224] is large (the
  bump reaction); not decoded.
- **WallingImpulsion** (from JumpImpulsion or ObstacleCollision on `EnterWallingVertical`; from JumpImpulsion only
  when standing, after the 0.2 s window, and `CanWallingVertical` [0xFD295A]): → the **Walling** context when its
  action has finished [0xF9DF10].
- **JumpImpulsionForwardFromIdle** (`JumpToTarget` from Move: item flag 0x20 clear and either standing, factor 1, or
  an action other than the move cycle / 228159995 / 228159996 under 75 %, factor = ratio / 0.75, stored at +0x45C
  [0xFB5A9E]; or from JumpImpulsion): → **InAir** when its action has finished [0xF9DEB6].

## 15. The reflection table
Recovered with `RE/tools/refl.py` (format and tool: `22_reflection.md`); names come from the hash list (standard
zlib CRC-32 of the name).

Enums: `HumanGroundSubState` (0 SubState_Movement, 1 SubState_Fight, 2 SubState_FreeRun, 3 SubState_OrientedMove,
4 SubState_Hurt, 5 SubState_ObstacleCollision); `MvtDivisions` (0 WaitLow, 1 WaitHigh, 2 WalkVerySlow, 3 Walk, 4 Jog,
5 Run, 6 Crouch, 7 MAX, 8 Invalid); `CombatStyle` (0 Normal, 1 Assassin, 2 MAX); `ObstacleLeanType` (0 Hands, 1 Feet).

| Offset | Name | Type |
|---|---|---|
| +0x010 | `DestGlobalBodyOrientation` | vec4 |
| +0x020 | `DestHeading` | vec4 |
| +0x030 | `CollidePosition` | vec4 |
| +0x040 | `CollideNormal` | vec4 |
| +0x050 | `LCOActorSight` | vec4 |
| +0x060 | `LCOPelvisSight` | vec4 |
| +0x070 | `LCOFootSight` | vec4 |
| +0x080 | `SubState` | enum HumanGroundData::HumanGroundSubStatev2 |
| +0x084 | `CurSpeedRatio` | float |
| +0x088 | `DestSpeedRatio` | float |
| +0x08c | `HeadingLinearRotationSpeed` | float |
| +0x090 | `HeadingEasedOutRotationSpeedRatio` | float |
| +0x094 | `ParamFlags` | u32 flags |
| +0x098 | `InternalFlags` | u32 flags |
| +0x09c | `EntityToAvoid` | handle Entity |
| +0x0a0 | `CombatStyle` | enum HumanGroundData::CombatStyle |
| +0x0a4 | `CarryObjectData` | struct HumanGroundCarryObjectData |
| +0x0b4 | `CurrentBodyAngle` | float |
| +0x0b8 | `CurrentBodyBankingAngle` | float |
| +0x0bc | `ObstacleLeanType` | enum HumanGroundData::ObstacleLeanTypev2 |
| +0x0c0 | `CollideHeight` | float |
| +0x0c4 | `CollideEntity` | handle Entity |
| +0x0c8 | `BlockCylinderRadius` | float |
| +0x0cc | `BlockCylinderLength` | float |
| +0x0d0 | `BlockCylinderOffset` | vec4 |
| +0x0e0 | `CustomActionControlChannel` | u32 |
| +0x0e4 | `CustomActionSecondaryChannel` | u32 |
| +0x0e8 | `IdleBreakerActionBlock` | handle ActionBlock |
| +0x0ec | `IsSet` | bool |
| +0x0ed | `BreakHard` | bool |
| +0x0ee | `Sprint` | bool |
| +0x0ef | `ForceFallFromGrabThrow` | bool |
| +0x0f0 | `ForceRemainStandingOnAnyWallImpact` | bool |
| +0x0f1 | `ActingPlaying` | bool |
| +0x0f2 | `ActingInterrupted` | bool |
| +0x0f3 | `Crouch` | bool |

## 16. Edge clamping: why a walk stops at an edge (2026-10-22)
Outside free-run the game doesn't let a walk carry Ezio off a deep edge. HumanGround has no state for this. The
**Human** cuts each frame's root motion against the guidance edges near a *gap*. The player's controller decides when
(`21_input.md`, "Gaps and edge clamping"). Decoded on Mac (symbols), matched on PC; ported
(`port/src/scimitar/ac2/edge_clamping.*`, `Human::EdgeClampingComputation`).

**The flags** (Human; Revelations PDB names, the AC2 block sits at Revelations −0x56):

| PC | Mac | Name | Set by |
|---|---|---|---|
| +0x991 | +0x971 | `m_IsEdgeClampingActive` | the ground states below, on Enter (1) / Exit (0) |
| +0x992 | +0x972 | `m_IsFightEdgeClampingActive` | a fight state's Enter (PC 0xF55950, with +0x996) |
| +0x993 | +0x973 | `m_IsEdgeClampingEnable` | `IHuman::SetEdgeClamping` (+0x6C, Mac 0x161C68A), the controller, every ground frame |
| +0x994 | +0x974 | `m_IsEdgeClampingZeroSpeed` | `EdgeClampingComputation`; read through IHuman +0x70 (no reader found yet) |
| +0x995 | +0x975 | `m_EdgeClampUsedLastFrame` | `EdgeClampingComputation` |
| +0x996 | +0x976 | `m_ForcedGapUpdate` | the fight state; makes `EdgeClampingComputation` search the gap itself |
| +0x921 | +0x901 | `m_GapCalculated` | set by `SetIsGapValid` (PC 0x913AF0 writes IHuman +0x821); blocks that search |
| +0x8E0 / +0x920 | | `m_Gap` / `m_IsGapValid` | `SetGap` (PC 0x913A70, IHuman +0x7E0) / `SetIsGapValid` (IHuman +0x820); both run on the IHuman base at Human +0x100 |
| +0x9F0 | +0x9C0 | `EdgeClampingSettings` | +0x00 edge distance **0.2** (Human ctor PC 0x932DC0), +0x10 reject direction, +0x20 reject angle (AC2 layout, not Revelations'; names by use, hypothesis) |

`Human::LocalInit` (Mac 0x12C82CE) and PC 0x9220B0 clear the flags. **`m_IsEdgeClampingActive`** is set by the
Enter of Wait (PC `_EnterWait` 0x61DFF2), MoveCycle [0xFBE706], MoveStop (PC 0x614577), CrouchMove [0xFC3338],
EdgeStop, ObstacleCollision, Poisoned, `HumanGroundCarryObject` and `HumanGroundDodgeState`; their Exits clear it
(Mac bytes checked). TurnStart, RunTurn, TurnOnSpot, the landings and Roll don't clamp.

**`Human::EdgeClampingComputation`** (PC 0x931330, Mac 0x12ED4B6), called by `Human::Update` (PC 0x933480) after the
actor's update and before `UpdateUnbalanceIgnoreList` (PC 0x92E420):
```
m_IsEdgeClampingZeroSpeed = 0
if (m_IsEdgeClampingActive && (m_IsEdgeClampingEnable || m_IsFightEdgeClampingActive)) {
  if (!+0x921 && m_ForcedGapUpdate) {                                    // PC 0x93139D
    GetGapList(gaps, pos, 0.75, 0.45, true)                             // IHuman +0x98
    v = GetBestGapInDirectionInList(gap, gaps, pos, sight, 0.75, pi)    // IHuman +0x9C
    SetIsGapValid(v); if (v) SetGap(gap)
  }
  if (IsGapValid()) {                                                    // PC 0x9314C8
    local = the current action's displacement over dt (sub_4F6E50, Action::GetDisplacementWithTime)
    world = entity matrix * local
    if (!IsNullWithEpsilon(local) && GetModifiedSpeed(world)) {          // epsilon 0.001, every lane (PC 0x2B3130)
      if (IsNullWithEpsilon(world)) m_IsEdgeClampingZeroSpeed = 1
      m_EdgeClampUsedLastFrame = 1
      ActionComponent: SetDisplacementMode(2), persistence 2             // the action stops moving the character
      if (|local|^2 > 2.5e-7) CharacterComponent target = pos + world    // PC 0x931640
      return
    }
  }
}
if (m_EdgeClampUsedLastFrame) { clear it; ResetDisplacement; SetDisplacementMode(0) }
```
For the player the gap comes from the controller (`SetGap(gap B)`, `SetIsGapValid`); `m_ForcedGapUpdate` is a
fight path.

**`Human::GetModifiedSpeed`** (PC 0x930540, Mac 0x12ED290): `|d| < 0.0005` → d = 0, true. Otherwise
`GetEdgeSegmentNearCharacterPos(workspace, entity, gap.m_EdgePosition, gap.m_EdgeNormal, normalize(d), |d| + 0.8,
settings, segments, 32)` (PC 0x1007D20, Mac 0xD37540), then `DisplacementCullingBySegment(out, pos, d,
settings.edgeDistance, segments)` (PC 0x1009420, Mac 0xD3675C); d = out when both succeed.

**`GetEdgeSegmentNearCharacterPos`**:
1. A `GuidanceZoneBox` on the entity (X = normalize(sight × K), Y = sight, Z = K), local bounds (−r, −r, −0.65) to
   (r, r, 0.3), cast with `GuidanceZone::CastWorld` (PC 0x3FEA00). It reads **guidance data, not the navmesh**.
2. Each line with `IsEdgeLeveled` (PC 0x5F9EE0: longer than 0.0005, |slope| ≤ 40°) becomes a flat segment with
   `GetLineGlobalFlatNormal` (PC 0x414140: the normal of the lower face, i.e. out over the drop). At most 32.
3. Crossing segments are split (`IntersectSegmentWithSegment` PC 0x10FC800 → `IntersectLineWithLine` PC 0x10FA790;
   parallel lines never cross; the axis is picked on the cross product's components truncated to int). The first
   part keeps p0; the tail is appended.
4. Each segment is oriented against the travel direction (`OrientSegmentInDirection` PC 0xEC3FF0).
5. The seed: the segment whose normal equals the gap's (0.001 per lane) closest to the gap's position (start 10000;
   taken at once under 0.0005). None → no clamp.
6. The chain from the seed, forward from p1 then backward from p0, through segments touching end to end
   (`GetBestSegmentAngleTouchingPoint` PC 0x1007580: the least turn by `Gear::GetAngle`; a beaten candidate is
   deleted). The list is cut to the chain. (A box-clipping pass follows behind a static flag that is 0.)
7. `FilterSegmentListWithNormalAngle` (PC 0x10074B0): with a reject angle above 0.0005, segments whose normal is
   within the angle of the reject direction are removed.

**`DisplacementCullingBySegment`**, in the XY plane: each edge is pulled back by the edge distance against its
normal. `DisplacementCollideWithPlane` (PC 0x1007780) finds the first pulled-back plane the move crosses outwards
(type 1). Its hit must project onto the real edge within the edge distance + 0.0005. A start already past a plane
but not past the edge is type 2. The move slides along the plane (`ProjectPointOnPlane` PC 0x10EA7E0), and the
edge is dropped from the list. This repeats while there is movement left and edges remain. A slide that would turn
back stops. Result: the clamped flat displacement, or false when nothing was hit.

**`Gear::GetAngle(a, b, axis)`** (PC 0x53590, Mac 0x9D08C0): positive when b is counter-clockwise from a about the
axis. So `m_TurnAngle` (`UpdateTurnAngle`, `GetAngle(DestHeading, sight, K)`) is **positive when the move direction is
to the right** of the sight. The port had the opposite sign until 2026-10-22, which mirrored `EnterTurnStart`'s side.

**Gaps** (`Human::GetGapList` PC 0x9282B0, Mac 0x12D4BD6):
- **Query:** a `GuidanceZoneSphere` of the given radius on `pos`.
- **Kept lines:** `IsEdgeLeveled`, with the foot of `pos` on the line inside the segment
  (`IntersectSegmentWithInfinitePlane` PC 0x10FA690), within 0.3 m of `pos` in height, and with a non-null flat
  normal.
- **Depth:** `GetDepthInFrontOfEdge(pos, foot, normal, 7.0, 0.02, 0)` (PC 0x9251D0; outlined only: rays down at 50°
  past the edge, layers 12 and 35, plus the deepest gap where they land). Under `minDepth` the line is no gap.
- **Fields filled:**
  - `m_EdgePosition` = the foot;
  - `m_EdgeNormal`;
  - `m_Depth`;
  - `m_SquareDistance` = |foot − pos|², negative when `pos` is on the normal's side;
  - `m_EdgeSubType`;
  - `m_HolePosition` is never written.
- **No-edge fallback:** with none found, `allowNoEdge` set and no shallow edge seen, the drop under `pos` itself becomes
  the gap (`m_NoEdge` = 1, the normal the flat sight).
- **Layout difference:** AC2 swaps Revelations' last two fields (+0x38 `m_NoEdge` byte, +0x3C `m_EdgeSubType`).
- **`GetBestGapInDirectionInList`** (PC 0x91F1F0, Mac 0x12CE79C): within the distance (flat, strict), the smallest
  `GetAngleAbsolute(dir, m_EdgeNormal)` not above the max angle.
- **`GetClosestGapInList`** (Mac 0x12C6EFC): the smallest flat distance, strict.

**Open:**
- `GetDepthInFrontOfEdge` in full. The port measures the ground 0.07 m past the edge.
- `CastEntityInternal` (Mac 0x62CC70): which lines a zone catches. The port tests the line against the sphere / box.
- Who reads `IsEdgeClampingZeroSpeed`.
- Live (row 12, 2026-10-22): the rules hold on roofs of 1.2, 2.3 and 7+ m. A high-profile run over a deep edge goes
  from Ground **straight to Ledge** (a hang); that hand-over isn't decoded (the port falls into InAir instead). After
  a low-profile stop at an edge the ground leaves the clamping states (+0x991 = 0); which state isn't known.
- Who clears `m_GapCalculated` each frame; `m_IsGapValid` flickers between frames live while standing at an edge
  (another writer, hypothesis).

## 17. The edge commands: LookDown, EdgeStop, FallOffEdge, LedgePullDown (2026-10-22)
The controller's edge reactions (`21_input.md`, "Edge reactions") send these interface commands with a Gap
parameter. Mac symbols; the PC Can / Do entries are named (PC addresses in `21_input.md`).
- **EnterLookDown (136), Idle → LookDown** [`TestIdleToLookDown…` 0xF9832A]:
  - The test passes while `*(HumanGround +0x790 PC / +0x770 Mac) + 0x24 == 1`. That object comes from LocalInit's
    `Human->vfunc+0x24(4, 0x1B)`, which may be the UpperBody context (id 27) (hypothesis).
  - The Exec copies the gap to +0x9B0 (Mac).
  - `EnterLookDown` [0xFD1248]: pushes state 9, sets +0x954 = 1, plays 0x2669E0F7 / 0x2669E0F8 by the front foot
    blended by the sight-to-edge angle, and emits a `StandOnLegdeEvent`.
  - LookDown → Idle when that field isn't 1 (`TestLookDownToIdleMain` PC 0x5E18A0); ExitLookDown (137) always
    passes.
  - `DoEnter` / `DoExitLookDown` first call `Human->vfunc+0x4C(50)->vfunc+0x30(0 / 1)` (unidentified).
- **UnbalanceOnEdge (87), Move → EdgeStop** [`TestMoveToEdgeStop…` 0xFCBF64], when all of these hold:
  - no hurt fall;
  - `IsNarrowEdge(gap, 0.0225, b) == 1` [0xFCBC84];
  - depth > 2;
  - `Human::TestPullDownToLedgeClearance(…, true)`.

  `IsNarrowEdge` returns:
  - 0 without an edge, at a depth ≤ 1.3, or with the signed square distance above 0.0225 (0.15 m);
  - otherwise, by the flat angle from the sight to the normal: 1 within 60° (120° with b), 3 for (−120°, −60°],
    4 for [60°, 120°), else 2.

  3 / 4 lead to NarrowObject instead [0xFCC052]. EdgeStop plays the unbalance action and goes back to Move when it
  ends (section 12).
- **LedgePullDown (88), Move or EdgeStop → the Ledge context**:
  - From Move [0xFCBB42], all of these hold:
    - the gap has an edge;
    - the current action item's flag 0x20 is clear;
    - dot3(normal, sight) > 0;
    - depth > 2;
    - `TestPullDownToLedgeClearance(pos, edge, normal, false)`.
  - From EdgeStop [0xFCB974]: once the unbalance action (115916159) has finished, the same checks.
  - The Exec functions copy the gap into the Ledge data (+0xF0) and set +0x40 to 1 from Move or 0 from EdgeStop,
    +0x44 = 0, +0x4C = 11.
  - `TestPullDownToLedgeClearance` [0x12DB8AA] (outline):
    - fails unless the edge is at least 0.5 above the water;
    - clears two boxes: one in front of the edge at pos.z + 1.075, half-extents 0.3 / 0.75 / 0.725 (on layer 14 when
      the bool is set), and one rotated about the edge, half-extents 0.3 / 0.3 / 0.8.
- **FallOffEdge (86), Ground → InAir** [`CanFallOffEdge` 0xFDA29A; test 0xFCB406]. A timer must have elapsed, then
  either a force flag, or all of these:
  - +0x455 bit 0 clear;
  - not in a bumping no-fall zone;
  - the ragdoll isn't driving;
  - depth ≥ 0.53;
  - `TestIsEdgeValidForFall` [0xFCAB92]: no hurt fall, **square distance < −0.01** (already past the edge), and
    `TestFallOffEdgeClearance`.

**Live (session 5, 2026-10-22, `livelab/record_edges.py`, which logs the FSM's current children):**
- A low-profile walk at a 7 m edge goes MoveCycle → Wait/Idle 0.40 m short (square distance 0.155–0.157), then
  Idle → **LookDown** 0.25 s later (five times).
- A high-profile run goes MoveCycle → **EdgeStop** at square distance **0.0224** (just inside 0.0225), then **Ledge**
  0.27 s later with the stick held (three times).
- On a 2.3 m drop the run goes MoveCycle → **InAir** at square distance −0.0115 (just past −0.01): FallOffEdge.
- Not seen yet: EdgeStop → Move on releasing the stick; the grasp pull-down.

**Port (2026-10-22):**
- `HumanGround` has EdgeStop (kept with Move's children) and Wait's child LookDown, and the commands' tests as above.
- `AssassinTask` sends LookDown, Unbalance, FallOffEdge and the EdgeStop pull-down.
- FallOffEdge and LedgePullDown are queued and run at the start of the ground's update, as the game's interface
  commands are; the others run at once (as TurnOnSpot already did).
- PORT stand-ins:
  - the two clearance tests pass;
  - no upper body (LookDown's +0x24 taken as 1);
  - EdgeStop's unbalance lasts the measured 0.27 s and goes back to Wait;
  - the pull-down requests the Ledge context, which the port doesn't have yet, so a run at a deep edge wobbles in
    EdgeStop instead of hanging;
  - the jumps and the grasp button aren't ported;
  - the ground holds while any point within 0.38 m of the centre has ground (the stand-up capsule's radius), so
    FallOffEdge (0.1 m past) comes first, as in the game.
- Tests run at 120 Hz: at 30 Hz a run steps over the 0.15 m Unbalance window.
