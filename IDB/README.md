# IDB/: the IDA databases

The databases are **not in git** (0.4–0.75 GB each, binary, unmergeable). Get the baseline set from the project
owner once and put the files here under exactly these names. Everything in the repository refers to them as
`IDB/<file>`, relative to the repository root.

| File | Session id | What | Baseline sha256 (2026-10-21) |
|---|---|---|---|
| `AssassinsCreedIIGame.exe.i64` | `ac2pc` | AC2 PC, the target. 32-bit x86, rebased to image base 0. Made from the exe with sha256 `f4983add9f4c366c388f067355fad52e10138aff588c8db7cc1dc4d84c56dafa`. | `14280490e1c943372310a262968581a3876efa2ae12f1b416acd584f66a0eec4` |
| `scimitar_sf.i64` | `ac2mac` | AC2 Mac build, partial symbols. | `21fb7bad1b082f743492475b56bae9e37ed1719ac0ba53f3883fb64c7bdbba7e` |
| `scimitar_final_ltcg.xex.i64` | `acrxbox` | AC Revelations, Xbox 360 (PowerPC, big-endian), full symbols. | `82d7b72d3818de051128f81ddc16f014df3fa78db273faa26e26278ccb5d9873` |
| `scimitar_final_ltcg.pdb` | | The Revelations PDB the Xbox IDB's symbols came from. | `628b7ff0c76c87eb7adafc8483a9f3f5241d75ded44130451f3ab2d4875c7fb6` |

The hashes identify the shared starting point. Your copy's hash changes as soon as you save, and that's expected.

## Sharing work: `annotations/`
`annotations/<idb file name>/` holds what people add to an IDB, as text that git can merge:
`names.tsv`, `comments.tsv`, `prototypes.tsv` and `types.h` (format in `tools/ida/annotations.py`).

- **After every pull:** `py tools/ida/sync.py import IDB/AssassinsCreedIIGame.exe.i64`
- **Before every commit:** `py tools/ida/sync.py export IDB/AssassinsCreedIIGame.exe.i64`

`sync.py` needs IDA 9's idalib (`idapro` Python package) and no other IDA process on that IDB. Inside a running IDA,
or an MCP session that already has the IDB open, run `tools/ida/annotations.py` there instead (File > Script file,
or the `exec` line at the top of that file).

On a merge conflict in a `.tsv`, keep both sides' lines, sorted by address; for the same address, agree on one
name. Then import.

## Ignored here
IDA's working files (`*.id0 *.id1 *.id2 *.nam *.til *.$$$`), the databases and the PDB are in `.gitignore`.
