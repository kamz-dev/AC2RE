# 20: Live checks

Questions only the running game can answer. Each `LIVE:` marker in RE docs or port code points to a row here.
Batch them, and answer them in one live session once the lab mod exists (`livelab/`). Keep row numbers stable;
close a row with its evidence (session folder, recording, probe log) and the date.

| # | Question | Raised by | How to check | Status |
|---|---|---|---|---|
| 1 | Is game space Z-up, and what is yaw 0? | `port/src/scimitar/math.h` | Read the player's matrix while walking north / turning | open |
| 2 | The movement tick: fixed rate or the frame dt? | `port/src/port/sim.h` | Log the frame dt in `HumanGround::Update` (PC 0x63B2C0) over a run. (Its `long` argument is the update phase, 0 = main / 1 = after physics: static, `_UpdateHumanGround` 0x6381D0, 2026-10-21.) | open |
| 3 | Which pad proxy fields hold the high-profile / legs / hand / head buttons and the sticks? | `port/src/port/input.h` | Lab input recording while pressing each button | open |
| 4 | Is the float at entity → +0x6C → +0x5D8 the frame dt (the speed ratio's step in `UpdateMoveCycleMain` 0x61BB60)? | `RE/11_ground.md` 5 | Read it next to the frame dt while running; check the ratio climbs by it per frame | open |
| 5 | Which input sets `HumanGroundData` +0xEE (the +0.2 speed band) and +0x181 (high profile)? | `RE/11_ground.md` 4 | Watch both while walking, pressing high profile, then high profile + legs | open |
