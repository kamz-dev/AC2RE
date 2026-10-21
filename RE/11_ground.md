# 11: HumanGround (on-foot context)

Status 2026-10-21: the class, its vftable, the FSM layout and the locomotion branch of the state tree are matched
Mac → PC; the MoveCycle speed model, the Wait / MoveCycle transitions and ground turning are decoded from both
builds. MoveStop, the turn states, landings, jumps and edges are next.

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
later than Mac. Check each field on PC.

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
| +0x464 [+0x460] | int | must be 1 for TurnOnSpot | `TestWaitToTurnOnSpotMain` 0x5F4F30 |
| +0x7A0 [+0x780] | bool | blocks TurnStart from Wait | `TestWaitToTurnStartMain` 0x5F4EC0 |
| +0x7A8 [+0x788] | int | gait level 1 / 2 (drives `UpdateOrientation` when non-zero) | `UpdateMoveCycleMain`, `UpdateMoveMain` 0x604990 |
| +0x7AC [+0x78C] | int | **movement division**: 0 = no movement intent, 2 = moving (see 6) | `UpdateMovementIntention` 0x5E03E0 |
| +0x7B0 [+0x790] | int | 1 when the speed ratio is above 0.4 (running gaits) | `UpdateMoveCycleMain` |
| +0x7B4 [+0x794] | int | high-profile intention, = `HumanGroundData`+0x181 != 0 | `UpdateMovementIntention` 0x5E03E0 |
| +0x7BC [+0x79C] | float | **speed ratio** 0..1 | `UpdateMoveCycleMain` |
| +0x7C0 [+0x7A0] | float | AI-requested speed ratio | same |
| +0x7D8 [+0x7B8] | float | **turn angle**: signed, about +Z, from the move direction to the sight | `UpdateTurnAngle` 0x5FE6A0 |
| +0x7E0 | vec4 | the entity's sight, copied each `UpdateMoveMain` | `UpdateMoveMain` 0x604990 |
| +0x81C [+0x7FC] | `ResponseCurve` | slow-down curve (see 5) | `LocalInit` 0x61F920 |
| +0x8A8 [+0x888] | int | AI speed override on | `UpdateMoveCycleMain` |
| +0x8AC / +0x8B0 [+0x88C / +0x890] | float | extra blend weights (slot 12 / slot 19) | same |
| +0x8B8 / +0x8BC [+0x898 / ?] | float | `HeadingMinRotationSpeed` / `HeadingMaxRotationSpeed` (rad/s) | Mac setters 0xF9771C / 0xF9772E; `UpdateOrientation` 0x5FFC40 |
| +0x8C0 / +0x8C4 | float | `HeadingMinRotationAngle` / `HeadingMaxRotationAngle` (rad) | Mac setters 0xF97740 / 0xF97752 |
| +0x8D0 | int | `HeadingRotationDirection` (`IHumanGround::RotationDirection`; −1 / 0 / 1) | Mac setter 0xF97764; `UpdateOrientation` |
| +0x8C8 / +0x8CC | float | 15.0 / 12.5 at construction; use unknown | ctor 0x6317A0 |
| +0x990 [+0x970] | float | `TurnOnMovementMinAngle` (clamped ≤ π; π/2 at construction) | Mac setter 0xF9825E; ctor |
| +0x994 [+0x974] | int | speed restriction mode 0..7 (1 at construction) | `UpdateMoveCycleMain`; ctor |

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
some tuning (it is a reflected class). Offsets are the same in both builds; each was read on PC in the function
named:

| Offset | Type | Meaning | PC evidence |
|---|---|---|---|
| +0x10 | vec4 | a direction, zeroed after use | `UpdateMoveCycleMain` 0x61BB60 |
| +0x20 | vec4 | the **desired move direction** (the turn angle and `UpdateOrientation` turn towards it) | `UpdateTurnAngle` 0x5FE6A0, `UpdateOrientation` 0x5FFC40 |
| +0x80 | int | movement mode; 3 = oriented move (→ OrientedMove) | `_UpdateMove` 0x634D20 |
| +0x88 | float | stick magnitude **(hypothesis)**; input to the movement division and the target ratio | `UpdateMovementIntention` 0x5E03E0 |
| +0xB4 / +0xB8 | float | turn angles, smoothed with the sight (`ComputeVectorRotationWithSight` 0x5FE3D0) | `UpdateMoveCycleMain` |
| +0xED | bool | allows MoveStop | `TestMoveCycleToMoveStopMain` 0x5EAAF0 |
| +0xEE | bool | raises the speed band by 0.2 (with high profile: run → sprint) **(hypothesis: the "legs" button)** | `UpdateMoveCycleMain` |
| +0xF3 | bool | crouch request (→ CrouchMove) | `LocalInit` 0x61F920 (cleared); Mac `_UpdateMoveCycle` |
| +0x154 | u8 | 2 after `LocalInit`; use unknown | `LocalInit` |
| +0x155 / +0x156 | u8 | movement-division threshold counts (without / with high profile), 3 after `LocalInit` | `LocalInit`; Mac `GetMovementDivisionFromSpeedRatio` 0x1119F44 |
| +0x158 / +0x16C | float[5] | movement-division thresholds, {0, 0.9, 1} after `LocalInit` | same |
| +0x180 | bool | cleared by `LocalInit`; use unknown | `LocalInit` |
| +0x181 | bool | high-profile request (→ `this+0x7B4`) | `UpdateMovementIntention` 0x5E03E0 |
| +0x21C | float | rotation ratio (scales the heading rotation angles); `SetRotationRatio` 0x90E250 | `UpdateOrientation` |
| +0x220 | float | rotation ratio step per second; Mac `SetRotationRatioStep` 0x1119FAE; `ExitMove` sets ratio 1, step 0.5 | `UpdateOrientation`; Mac `ExitMove` 0xFAFB48 |
| +0x224 | u8 | 1 after `LocalInit`; use unknown | `LocalInit` |

## 5. MoveCycle: the speed model (`UpdateMoveCycleMain` PC 0x61BB60 [0xFCD20A])
Ground speed is **animation-driven**: the code computes a speed ratio and blends the MoveCycle action's items; the
character moves by the blended animations' root motion. Speeds in m/s come from the animation data, not from
code constants.

1. **Target ratio** *t*:
   - AI override on (+0x8A8): *t* = +0x7C0;
   - else *t* = stick × 0.2 + base, base = 0 (walk), 0.2 (+0xEE), 0.6 (high profile), 0.8 (high profile and
     +0xEE; the player also emits witness event 26 at most every 0.5 s);
   - clamped to ≤ 1.
2. **When the current action isn't the move cycle** (action id != 93469659 and not finishing): non-player humans
   are held to [0.2, 0.6]; the restriction mode (+0x994) limits *t*: 0 → 0.6; 1, 4 → [0.2, 0.6]; 2 → 0.2 below a
   ratio of 0.6, else 0.6; 3 → [0, 0.6]; 5, 6 → ≥ 0.2 and ≤ 0.6 (≤ 1 with +0xEE); 7 → ≤ 0.6 (≤ 1 with +0xEE);
   without high profile *t* ≤ 0.2. The ratio then moves toward *t* by `step` per frame.
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

   Each band's base slot has left / right turn variants at +1 / +2, weighted by the turn angles in
   `HumanGroundData` +0xB4 / +0xB8 (normalised by π/2, clamped to 1).
5. At ratio ≥ 0.8 the upper body's action is ended; below 0.6 the upper body may start a "cycle breaker"
   (random delay from `HumanData` +0x120..+0x12C).

## 6. Locomotion transitions (Move's children)
`UpdateMoveMain` (PC 0x604990 [0xFCA184]) runs first every frame: `PreprocessCharge` 0x5ED4E0,
`UpdateMovementIntention` 0x5E03E0, `EmitPresentationMovementType` 0x5F3A30, copies the sight to +0x7E0,
`UpdateTurnAngle` 0x5FE6A0, `UpdateOrientation` 0x5FFC40 when the gait level is non-zero, then the push handler.

**Movement division** (`UpdateMovementIntention`): with the AI override, 2 if the AI ratio > 0 else 0. Otherwise
`HumanGroundData::GetMovementDivisionFromSpeedRatio(highProfile, stick)` (0x90E190) returns the first index *i*
below the table's count whose threshold ≥ stick (0 if none); index 0 → division 0, 1 or 2 → division 2 (other
indices leave it unchanged). With the {0, 0.9, 1} tables this is **0 when the stick is exactly 0, else 2**.

**Can an action be cut by movement** (`IsActionInterruptibleByMovement` 0x5ECD30): never if the current action item's
flags (+0xC) have 0x20; else the flag bit chosen by (division, high profile): (0, off) 6, (0, on) 7, (1, off) 8,
(1, on) 10, (2, off) 9, (2, on) 11. Below, "free" = `ActionComponent::WillActionFinish` or that test.

| From → to | Test (PC [Mac]) | Condition |
|---|---|---|
| Wait → TurnStart | 0x5F4EC0 [0xF9EA82] | free, \|turn angle\| > `TurnOnMovementMinAngle`, byte +0x7A0 clear |
| Wait → MoveCycle | 0x5F4E70 [0xF9EAE6] | division 1 or 2, free |
| Wait → TurnOnSpot | 0x5F4F30 [0xF9E9F8] | +0x464 == 1, custom-action orientation check, free, division 0, \|turn angle\| ≥ 1° |
| Wait → CrouchWait | 0x5F4FC0 | not decoded |
| MoveCycle → MoveStop | 0x5EAAF0 [0xFB5CAE] | running, gait level ≠ 0, item flag 0x20 clear, `HumanGroundData`+0xED, and the action is MoveCycle (93469659), 96 or 97, or past 33 % |
| MoveCycle → Wait | 0x5EAB90 [0xFB5D7A] | division 0, and not running or the action isn't MoveCycle |
| MoveCycle → TurnStart | 0x5F5B00 [0xF9E8DA] | not running, \|turn angle\| > `TurnOnMovementMinAngle`, free |
| MoveCycle → CrouchMove | 0x5E1AC0 → 0x61F1D0 [inline in 0xFE05A2] | `HumanGroundData`+0xF3 (crouch) set and not running [Mac `this[484]`, the running flag] |

Each state's tests run in the table's order and the first that passes wins.

## 7. Turning (`UpdateOrientation` PC 0x5FFC40 [0xFB4E32])
Ground turning rotates the entity's **sight** (its facing) towards the desired move direction (`HumanGroundData`
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
values are open (live row 6).

## 8. Construction and `LocalInit` defaults
- ctor (0x6317A0): heading speeds / angles 0, `HeadingRotationDirection` 0, `TurnOnMovementMinAngle` π/2,
  speed restriction 1, +0x8C8 15.0, +0x8CC 12.5.
- `LocalInit` (0x61F920): `HumanGroundData` movement mode, move direction, +0x84, +0x88, +0x8C, +0x94, +0x98
  zeroed; the
  movement-division tables and the slow-down curve above; +0xED / +0xEE / +0xF3 / +0x180 / +0x181 cleared;
  turn angle 0. The character capsule: height 1.8, radius 0.4, and two offsets at height − 0.4 and one at
  height − 0.15 (`CharacterComponent` +0x64 / +0x68 / +0x6C from +0x78) **(hypothesis: capsule fields)**.

## 9. Open
- [ ] MoveStop, TurnStart / TurnOnSpot / RunTurn: their `Update*Main` and exits.
- [ ] Who calls `IHumanGround`'s heading setters for the player, and with what (live row 6; `21_input.md`).
- [ ] Wait → CrouchWait (0x5F4FC0) and the custom-action orientation check in TurnOnSpot.
- [ ] Landing / LandingHard / LandingBalanceFall / Roll / EdgeLanding: what InAir hands over and how each is
      chosen.
- [ ] JumpImpulsion*, ObstacleCollision, EdgeStop, WallingImpulsion: the free-run entry points.
- [ ] `HumanGroundData`'s full field list from its reflection properties.
- [ ] The MoveCycle action's animations and their root-motion speeds (needed for the port's speeds).
