# 20: Live checks

Questions only the running game can answer. Each `LIVE:` marker in RE docs or port code points to a row here.
Batch them, and answer them in one live session once the lab mod exists (`livelab/`). Keep row numbers stable;
close a row with its evidence (session folder, recording, probe log) and the date.

| # | Question | Raised by | How to check | Status |
|---|---|---|---|---|
| 1 | Is game space Z-up, and what is yaw 0? | `port/src/scimitar/math.h` | Read the player's matrix while walking north / turning | open |
| 2 | The movement tick: fixed rate or the frame dt? What does `HumanGround::Update(long)`'s argument hold? | `port/src/port/sim.h`, Mac 0x1544B74 | Probe the PC match of `HumanGround::Update`, log the argument and the frame dt | open |
| 3 | Which pad proxy fields hold the high-profile / legs / hand / head buttons and the sticks? | `port/src/port/input.h` | Lab input recording while pressing each button | open |
