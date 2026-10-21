# 21: Player input → movement (the player controller)

Status 2026-10-21: **started**. The player's controller and its interface to HumanGround are found; the ground
movement function is partly decoded (break hard, high profile, the stick). PC addresses of the controller are not
matched yet (all addresses below are Mac unless marked PC).

## Who drives the player
- `BhvAssassin::UpdatePadInput` [0x1012A5A] updates the pad (`PadInput::Update` [0x81FC30], at BhvAssassin +240),
  sends it to the entity as an `InputReferenceEvent`, then calls the player's navigation service.
- **`AssassinTask`** is the player controller: an FSM with one state per Human context (Init, Main, Debug, Ground,
  Ladder, InAir, Ledge, Climb, Walling, NarrowObject, Swim, …; `LinkFSM` [0x104FFE6]). Each frame `PreUpdate`
  [0x106C922] runs `UpdateGlobal` [0x106BDE2], then the current state's update; on the ground `UpdateGround`
  [0x1082F62] → **`UpdateGroundMovement(IHumanGround*, ControlTypes&)`** [0x107F0EC, 16 KB].
- AI humans are driven by the navigation services instead (`CSrvNavigation`, `GroundNavigationTask`
  [`UpdateSpeedAndDir` 0xE320FA, `UpdateBrakeHard` 0xE32FB8]), through the same interface.

## The interface (IHumanGround, Mac secondary vftable 0x281AC70)
| Slot (byte offset) | Method | What it does in HumanGround |
|---|---|---|
| +0x00 | `SetDirection(dir)` [0xF98EE6] | `HumanGroundData::m_DestHeading` := dir |
| +0x04 | `SetSpeedRatio(r)` [0xF9B710] | `m_SpeedControlType` := MvtTypeAndMvtState; `m_DestSpeedRatio` and `m_IntentionSpeedRatio` := r; `m_IntentionSpeed` := `GetMovementSpeedFromRatio(r)` (0 when \|r\| ≤ 0.0005) |
| +0x0C | `SetMovementType(type)` [0xFBA59A] | `HumanGroundData::m_DestMvtType` := type; a `ProfileChangeEvent` on change |
| +0x40 | `SetCrouch` | `HumanGroundData::m_Crouch` |
| +0x68 | `SetMovementSpeed(speed)` [0xF9B770] | `m_SpeedControlType` := DirectSpeed; `m_IntentionSpeed` := speed; the ratio from the speed table |
| +0x78 | `SetBreakHard` | `HumanGroundData::m_BreakHard` |
| +0x80 / +0x84 | `SetSprint` / `GetSprint` | `HumanGroundData::m_Sprint` |

## UpdateGlobal (every frame, before the context's update)
- **High profile** (AssassinTask +0x94C, PDB `m_IsHighProfile`): `ButtonPressed(13)` (12 with the alternative
  control setting, GameSetting +172), inverted when the inverse-profile option is on; only with the HighProfile
  ability.
- The camera-relative stick (`ControlScheme::GetRelativeStick`, +0xED0, normalised) and its **damped** version
  (`GetDampedRelativeStick`, +0xEF0); the stick length (+0xEC0). Above a length of 0.35 the angles from the sight to
  both are kept (+0xEC4, +0xEE4), and the last direction (+0x8E0) is held; below it the direction is cleared after
  0.3 s.

## UpdateGroundMovement (partly decoded)
- **Break hard** (answers live row 7's "when"): `SetBreakHard(true)` while the stick is in its rest band
  (`JoyThresholdPressed(stick 0, band 0)`). Otherwise `false`, except that when the stick is in band 1 (or a flag at
  +0x8C0 is clear) the controller brakes hard if the damped stick and the new direction are more than 135° apart
  (dot < −0.70710677) and the player isn't in a group formation: a quick reversal of the stick.
- **Speed ratio**: `SetSpeedRatio(v × (+0x8B0))` for the normal case; v is not fully decoded yet.
- **Sprint / movement type / crouch**: `SetCrouch` (+0x40), `GetSprint` → `OnSprintBegin` [0x104EA26] → `SetSprint`
  (+0x80), `SetMovementType` (+0x0C) are called around 0x107FE33–0x107FEEE; not decoded yet.
- `GetPadInputSpeedRatio` [0x104E44C] (used by `EnterGround` and the ladder): stick in band 1 → 0.5, band 2 → 1.0,
  else 0.

## Open
- [ ] The PC addresses of `AssassinTask`, `UpdateGroundMovement`, `UpdateGlobal` (match by strings / constants:
      −0.70710677, 0.35, 2.0943952).
- [ ] The rest of `UpdateGroundMovement`: the speed value, sprint, movement type, crouch, the heading rotation
      setters (the values are known live: `20_live_checks.md` row 6).
- [ ] The pad bands' thresholds (`ControlScheme::SetupStickThresholds`) and the damping of the relative stick.
