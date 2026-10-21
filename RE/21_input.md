# 21: Player input → movement (the player controller)

Status 2026-10-21: **the ground controller is decoded and ported** (`port/src/scimitar/ac2/assassin_task.*`), up to
PC 0xDF8C2D; the rest of `UpdateGroundMovement` (ladders, interactions) and the pad's own processing (stick bands,
damping) are open. PC addresses first, Mac in brackets.

## Who drives the player
- `BhvAssassin::UpdatePadInput` [0x1012A5A] updates the pad (`PadInput::Update` [0x81FC30], at BhvAssassin +240),
  sends it to the entity as an `InputReferenceEvent`, then calls the player's navigation service.
- **`AssassinTask`** is the player controller: an FSM with one state per Human context (Init, Main, Debug, Ground,
  Ladder, InAir, Ledge, Climb, Walling, NarrowObject, Swim, …; `LinkFSM` [0x104FFE6]). Each frame `PreUpdate`
  [0x106C922] runs `UpdateGlobal` [0x106BDE2], then the current state's update. On the ground: `UpdateGround`
  0xE049A0 [0x1082F62] → **`UpdateGroundMovement(IHumanGround*, ControlTypes&)`** 0xDF7FE0 (11 KB)
  [0x107F0EC, 16 KB]. The PC function was found by its heading constants (2.6 and 0.975, only used there). It
  matches the Mac one branch for branch with the same AssassinTask offsets (the timer is the one exception, below).
  PC does part of the arithmetic in double precision (`cvtps2pd`); the port follows it.
- AI humans are driven by the navigation services instead (`CSrvNavigation`, `GroundNavigationTask`
  [`UpdateSpeedAndDir` 0xE320FA, `UpdateBrakeHard` 0xE32FB8]), through the same interface.

## The interface (IHumanGround, Mac secondary vftable 0x281AC70; byte offsets)
| Slot | Method | What it does in HumanGround |
|---|---|---|
| +0x00 | `SetDirection(dir)` [0xF98EE6] | `HumanGroundData::m_DestHeading` := dir |
| +0x04 | `SetSpeedRatio(r)` [0xF9B710] | `m_SpeedControlType` := MvtTypeAndMvtState; `m_DestSpeedRatio` and `m_IntentionSpeedRatio` := r; `m_IntentionSpeed` := `GetMovementSpeedFromRatio(r)` (0 when \|r\| ≤ 0.0005) |
| +0x0C | `SetMovementType(type)` [0xFBA59A] | `HumanGroundData::m_DestMvtType` := type; a `ProfileChangeEvent` on change |
| +0x40 | `SetCrouch` [0x153B98A thunk] | `HumanGroundData::m_Crouch` |
| +0x68 / +0x70 | `SetMovementSpeed` [0xF9B770] / `GetMovementDivisionSpeed` [0x153D890 thunk] | the direct-speed path (debug) |
| +0x78 | `SetBreakHard` | `HumanGroundData::m_BreakHard` |
| +0x80 / +0x84 | `SetSprint` / `GetSprint` | `HumanGroundData::m_Sprint` |
| +0x420 | `IsInObstacleCollision` | |
| +0x458 | `DetectBeamInDirection` | |
| +0x524 / +0x528 | `SetHeadingMin/MaxRotationSpeed` | |
| +0x534 | `SetHeadingRotationDirection` | |
| +0x538 | `IsOrientable` | |
| +0x53C / +0x540 | `SetForceTurnAround` / `GetForceTurnAround` | `m_ForceTurnAround` (+0x8D4 bit 0) |
| +0x580 / +0x584 / +0x588 | `GetGentlePushIntention` / `IsPushing` / `IsInCrowdShove` | |
| +0x61C | `IsPickPocketing` | |
| +0x674 | `IsPraying` | |
| +0x7C0 | `IsCarryingObject` | |
| +0x7E4 / +0x7E8 | `SetSpeedFactor` / `GetSpeedFactor` | (debug path) |

## AssassinTask's fields used here
The stick block lines up with Revelations' `AssassinTask` (same order and spacing), so it takes those names:

| AC2 | Revelations | Name | What |
|---|---|---|---|
| +0xEC0 | +0x2398 | `m_LastInputLength` | `ControlScheme::GetStickLength` |
| +0xEC4 | +0x239C | `m_LastInputAngle` | `Gear::GetAngle(stick, sight, K)` while the length is above 0.35, else kept |
| +0xED0 | +0x23A0 | `m_LastRelativeDirection` | `GetRelativeStick`, normalised |
| +0xEE0 | +0x23B0 | `m_InPreciseTargetingMode` | written by UpdateGroundMovement: button 0 without high profile |
| +0xEE4 | +0x23B4 | `m_LastDampedInputAngle` | the same angle for the damped stick |
| +0xEF0 | +0x23C0 | `m_LastDampedRelativeDirection` | `GetDampedRelativeStick` |

Others (names not matched): +0x8B0 the stick's speed ratio, +0x8B8 the turn slowdown factor, +0x8C0 a rotation
direction, +0x94C high profile (Revelations calls its flag `m_IsHighProfile`; the neighbouring bool block suggests
the same place, hypothesis), +0x94E the forced turn-around, +0x968 [+0x964] its `scimitar::Timer`.

## PadInput: the length, the bands, the relative and "damped" sticks
The ControlScheme (AssassinTask +0x70) holds a `scimitar::PadInput` at +0x10 (AssassinTask +0x80, live). Each frame
`PadInput::Update` 0x1E8740 [0x81FC30] runs, per stick, `ComputeRelativeInput` 0x1E84A0 [0x81F570], stores the length
(capped at 1), and sets the band: the number of thresholds below the length (thresholds 0.35 / 0.9 / 1.0, live).

`ComputeRelativeInput` keeps a 10-frame history of three directions, each made camera-relative by
`CameraProxy::RemapPadInput` 0x52740 [0x860EF0]:
- **raw** (`AddRawRelativeStickValue` 0x1E80C0): the normalised stick;
- **relative** (`AddRelativeStickValue` 0x1E8120; `ControlScheme::GetRelativeStick`): the stick, but exactly forward
  when dot(stick, forward) > PadInput +0xA8[stick] (0.9961947 = cos 5° for stick 0, 1.0 for stick 1). On PC this is
  unconditional; Mac gates it on a flag;
- **"damped"** (`AddDampedRelativeStickValue` 0x1E8180; `GetDampedRelativeStick`): not a time filter. The (snapped)
  stick's angle θ from forward goes through the `ResponseCurve` at PadInput +0x600 + 0x8C·stick (`GetOutput`,
  vftable 0x18BA584 slot 1) as |θ| / π, and forward is turned by the result on the stick's side. The constructor
  (0x1E8B00 [0x820150]) gives stick 0 the keys (0, 0), (1/36, 1°), (1/18, 5°), (1/9, 10°), (1/6, 20°), (2/9, 40°),
  (1, 180°): directions near forward are pulled towards it, by at most 10° (at 30°), and from 40° on nothing changes.
  Stick 1 (the camera) has the identity.

Live (2026-10-21, gamepad): the damped and relative directions differed by at most 10.0° over 6663 samples, and were
equal in most of them (stick directions 40° or more from the camera's forward); with the stick held still while the
camera turned, the difference stayed constant. With the keyboard they were always equal: the key directions are 0°,
45° or 90° from forward, where the curve changes nothing.

## UpdateGlobal [0x106BDE2] (every frame, before the context's update)
- **High profile** (+0x94C): with the HighProfile ability, and while all actions are allowed, `ButtonPressed(13)`
  (12 with the alternative control setting, GameSetting +172), flipped when the inverse-profile option is on.
  Without the ability it is cleared.
- The camera-relative stick (+0xED0, normalised), the damped stick (+0xEF0), the length (+0xEC0); above 0.35 the
  angles from the sight (+0xEC4, +0xEE4). The last stick direction is buffered (+0x8E0) and cleared 0.3 s after the
  stick goes under 0.35.

## UpdateGroundMovement 0xDF7FE0 [0x107F0EC]
`UpdateGround` 0xE049A0 [0x1082F62] gets here only when none of its handlers takes the frame. In order (PC names
from the Mac symbols): `UpdateCape` 0xDD82B0, `UpdateCarryNPCDetection` 0xDEEA20, a ground-state check
(IHumanGround +0x7E0 ≠ 7), `CheckForMissionInteraction` 0xDE37C0, `UpdateWeapon` 0xDE7D60,
`CheckForWeaponSwitchRequest` 0xDEAEC0, `UpdateRest` 0xE04490, `UpdateGroundLoot` 0xDF3CB0,
`UpdateFormationControl` 0xE04040, `UpdateDrawWeapon` 0xDEA970, `UpdateFight` 0xDFD7C0, `UpdateGrabbed` 0xDED850,
`UpdatePickUp` 0xDEAB40, `UpdateCarry` 0xDD9910, `UpdateControls` 0xDF7630, `UpdateMissionInteractionControls`
0xDD9290, `CheckForFightInteraction` 0xE02C00, `UpdateAnimusDB` 0xDD9A80, `CheckDoGroupManip` 0xDEC550, then
`UpdateGroundMovement`, `UpdateMainCharacterActionIDs` 0xDEC210, `UpdateObjectCollision` 0xDD80F0,
`UpdateGroundCharge` 0xDE5430. The `Check*` / `Update*` ones whose result is tested end the frame when they return
true.

In order (`sight` is `BaseEntity::GetSight`; `dt` the entity's world dt, PC entity +0x6C → +0x5D8):
1. `ReorientCaracterInFirstPerson` 0xDDD960 (first person only).
2. Without the GetAllActions ability: `SetSpeedRatio(0)`, `SetDirection(normalised sight)`, return.
3. **Break hard** (0xDF8139): `true` while the stick is in band 0 (`JoyThresholdPressed(stick 0, 0)`; PC inlines it
   as `unk_IsStickHijackedByOther` 0xD11B0 and a compare of the length, AssassinTask +0x630 (ControlScheme +0x5C0),
   against the thresholds at `*(AssassinTask +0x7A8)` (ControlScheme +0x738): **0.35, 0.9, 1.0** live; band 0 is
   length ≤ 0.35, band 1 up to 0.9, band 2 up to 1.0). Otherwise, if +0x8C0 ≠ 0 and the stick isn't in band 1: `false`.
   Otherwise `true` when dot(damped stick, sight) < −0.70710677 (**more than 135° from the facing**) and
   `IsPlayerNavigatingInAGroupFormation` 0xDD9520 is false; else `false`. (+0x8C0 is always 0 here, see 9.)
4. **Forced turn-around** (0xDF81F7): when the stick just entered band 1 or 2 (current band AssassinTask +0x7D4,
   last frame's +0x7D6), not already forced, and |+0xEE4| > 2.0943952 (120°): set +0x94E and start the timer for
   0.3 s. Otherwise it is cleared once the timer has run out or HumanGround's `GetForceTurnAround` is false. Then
   `SetForceTurnAround(+0x94E)`. The elapsed test is `scimitar::Timer::sub_6BCC0` (now ≥ end, the clock at
   *(timer +0x1C)). Live (2026-10-21): five forced turn-arounds lasted 0.30 s each.
5. **The speed ratio** (+0x8B0, 0xDF834B): (length − 0.35) / 0.65, at most 1; 0 at or below 0.35. Then by
   `AssassinAbilities::GetMaxSpeed` 0x6CCC40 (`AssassinAbilitySet::MaxSpeed`: 0 NoMovementAllowed, 1 Walk, 2 Jog,
   3 Run, 4 Sprint, 5–7 slow walks): NoMovementAllowed → 0; Jog in high profile → at most 0.3.
6. **The turn slowdown** (+0x8B8, 0xDF83F3): t = clamp((|+0xEE4| − π/4) / (π/4), 0, 1). If the factor is above
   1 − t it falls by 10·dt (not below 0.1), else it rises by 5·dt (not above 1). Outside high profile it is 1.
   Live (2026-10-21, high profile, the stick swung 81° off the facing): it fell about 0.04 per 4 ms frame and rose
   about 0.02 per frame once the facing caught up, as the rule gives.
7. **Walls, crowds, beams** (0xDF8518): above 0.35 and not in obstacle collision,
   `ReorientDirectionFromWallCollision(dir, 0, π/4, target, dir)` 0xDF3590 may bend the direction (target: 0.2 ×
   the damped stick while carrying a body, else zero); a result of 2 brakes hard and stops. In low profile with the
   stick within 45° of the sight, the crowd factor is 1 − f × (0.1 pushing / 0.2 gentle push / 0.5), or 0 when
   `GetCrowdObstructionFactor` 0xDE1720 is 0.95 or more (f = 0 while pickpocketing). A crowd shove gives 0, praying
   0.75. While carrying, a beam ahead (`DetectBeamInDirection`) stops. **`SetSpeedRatio(+0x8B0 × min(+0x8B8,
   crowd, shove, praying))`**.
8. **Heading rotation speeds** (0xDF8818): carrying in high profile 0.75 / 2.0; with pad button 0 0.975 / 2.6 (0.375
   / 3.0 praying); else 1.5 / 4.0. These are the values measured live (`20_live_checks.md` row 6).
9. **Rotation direction** (0xDF88DD): with `IsOrientable` and no wall reorientation the code picks −1 / 1 from the
   sign of +0xEE4 beyond 30° (0.5235988) into +0x8C0, but every path then stores 0 and calls
   `SetHeadingRotationDirection(0)`: the shipped game always turns the short way.
10. **Direction** (0xDF8B01): `SetDirection(dir)` while the stick length is above 0, else the normalised flat
    sight.
11. +0xEE0 := high profile ? 0 : button 0; `SetCrouch(false)`.
12. **Sprint** (0xDF8BA9): with MaxSpeed Sprint, `SetSprint(button 0)` (`OnSprintBegin` 0xDEB5E0 on a new press);
    otherwise `SetSprint(false)`.
13. **Movement type** (0xDF8C08): `SetMovementType(high profile and MaxSpeed > Walk ? 1 : 0)`.
14. From 0xDF8C2D [0x107FEF1]: ladders (the static `noLadder`) and more, not decoded.

A static `speedMethod` (PC dword_1E2C2B0) switches 5–8 to a debug path: `SetMovementSpeed` from the division speeds
and a speed factor stepped by button 3.

**Pad button 0** is the legs button (A; Space on PC): it gives `Sprint` (live row 5). The button numbering is the
`Pad::PadButton` enum.

PC naming note: the baseline's `scimitar::ControlScheme::ButtonPressed` at 0xD1170 is not the press test. It returns
true when **another** control scheme holds the button; the press test is "not held by another" and then
`Pad::ButtonPressed_0` 0x10B1710. It is renamed `unk_IsButtonHijackedByOther`.

## Live (2026-10-21)
The player's AssassinTask was found by scanning for the player entity at +0x0C; its vftable is PC 0x1A23360. With
the keyboard the stick length is 0 or 1 (no ramp), so the stick goes straight from band 0 to band 2 and the previous
band (+0x7D6) follows one frame later; +0xEC0 and +0x630 always held the same length. The gamepad gives every length
and all three bands, consistent with the thresholds. The damped stick: see PadInput above.

## Port
`scimitar::ac2::AssassinTask` (`port/src/scimitar/ac2/assassin_task.h`) runs steps 2–13 as above; `port::Sim` runs it
before `Human::Update`, as the game's task runs before the contexts. `scimitar::PadInput`
(`port/src/scimitar/pad_input.h`) has the snap cone, the curve and the length cap; the stick thresholds are the live
values. PORT stand-ins: `CameraProxy::RemapPadInput` is a rotation by `MoveInput::cameraYaw`; the `UpdateGround`
handlers are skipped (out of scope); no crowds, carrying, praying, walls or group formations
(their queries answer false or 0); the abilities default to everything allowed; `OnSprintBegin` and the first-person
reorientation aren't ported.

## Open
- [ ] `UpdateGlobal`'s PC address.
- [ ] The rest of `UpdateGroundMovement` (from PC 0xDF8C2D).
- [x] The stick thresholds (0.35 / 0.9 / 1.0, live 2026-10-21, row 11).
- [x] The "damped" stick: PadInput's angle curve (2026-10-21, static + live with a gamepad).
- [ ] Where the thresholds come from (`ControlScheme::SetupStickThresholds`); `CameraProxy::RemapPadInput`; the Mac
      flag on the snap; `Pad::ReadStick`'s own dead zone.
- [ ] `ReorientDirectionFromWallCollision`, `GetCrowdObstructionFactor`, `OnSprintBegin`.
