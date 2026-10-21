# 20: Live checks

Questions only the running game can answer. Each `LIVE:` marker in RE docs or port code points to a row here.
Batch them, and answer them in one live session once the lab mod exists (`livelab/`). Keep row numbers stable;
close a row with its evidence (session folder, recording, probe log) and the date.

| # | Question | Raised by | How to check | Status |
|---|---|---|---|---|
| 1 | What is yaw 0? (Z is up: `UpdateTurnAngle` 0x5FE6A0 and `UpdateOrientation` 0x5FFC40 turn about `Vector4::GetBaseK`, static, 2026-10-21.) | `port/src/scimitar/math.h` | Read the player's matrix while walking north / turning | open |
| 2 | The movement tick: fixed rate or the frame dt? | `port/src/port/sim.h` | Log the frame dt in `HumanGround::Update` (PC 0x63B2C0) over a run. (Its `long` argument is the update phase, 0 = main / 1 = after physics: static, `_UpdateHumanGround` 0x6381D0, 2026-10-21.) | open |
| 3 | Which pad proxy fields hold the high-profile / legs / hand / head buttons and the sticks? | `port/src/port/input.h` | Lab input recording while pressing each button | open |
| 4 | Is the float at entity → +0x6C → +0x5D8 the frame dt (the speed ratio's step in `UpdateMoveCycleMain` 0x61BB60)? | `RE/11_ground.md` 5 | Read it next to the frame dt while running; check the ratio climbs by it per frame | open |
| 5 | Which input sets `HumanGroundData` +0xEE (Sprint, through `IHumanGround::SetSprint` PC 0x632E80) and +0x181 (high profile)? | `RE/11_ground.md` 4 | Watch both while walking, pressing high profile, then high profile + legs | open |
| 6 | The player's heading rotation values: `HeadingMin/MaxRotationSpeed` (+0x8B8 / +0x8BC), `HeadingMin/MaxRotationAngle` (+0x8C0 / +0x8C4), `HeadingRotationDirection` (+0x8D0) and `TurnOnMovementMinAngle` (+0x990) on the player's HumanGround | `RE/11_ground.md` 7 | Read them while walking, running and sprinting; note whether they change between gaits | open |
| 7 | When does the player's controller call `IHumanGround::SetBreakHard` (PC 0x632E20, `HumanGroundData`+0xED)? It sends a running HumanGround into MoveStop. | `port/src/scimitar/ac2/human.cpp`, `RE/11_ground.md` 10 | Probe 0x632E20 (log the argument) while running, releasing the stick, reversing it, pressing high profile on and off | open |
