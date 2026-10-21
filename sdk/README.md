# sdk/: the game's own classes, layout-exact

Headers that describe Assassin's Creed II's classes as they sit in the running PC game (32-bit). They hold the
classes' real names, field offsets and sizes, vftable slots, and the address table of the pinned build. Two
consumers:
- **the lab mod** (`livelab/`, 32-bit, injected into the game) reads and hooks the game through them;
- **the port** (`port/`) uses the same names and fields, so its classes line up with the game's one to one.

## Rules
- A class mirrored here uses the game's name and namespace (`scimitar::ac2::HumanGround`), from the Mac symbols
  (`IDB/scimitar_sf.i64`) or the Revelations PDB (`IDB/scimitar_final_ltcg.pdb`) and checked on the PC build.
- Every field has its offset checked with `AC2_ASSERT_OFFSET`, and every class its size with `AC2_ASSERT_SIZE`
  (`layout.h`). The checks compile in 32-bit builds only. Unknown bytes are `AC2_PAD(n)`, never guessed fields.
- Every field and address cites where it was found: the PC function that reads it, and the Mac / Xbox symbol if
  there is one. Mark guesses **(hypothesis)** in the comment.
- `addresses.h` is the only place addresses live. The lab and the docs quote it.
- Revelations (Xbox 360) layouts are a different game on a big-endian 32-bit PowerPC. Use them for names and
  structure only, and check every offset on the PC build before it goes here.

## Files
| File | What |
|---|---|
| `include/ac2sdk/layout.h` | `AC2_PAD`, `AC2_ASSERT_OFFSET`, `AC2_ASSERT_SIZE`, `AC2_GAME_ABI` |
| `include/ac2sdk/addresses.h` | the address table of the pinned PC build |
