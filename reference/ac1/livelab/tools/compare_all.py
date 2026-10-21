"""compare_all.py - replay every recording of the sessions in the port and summarise the first divergences.

  py tools/compare_all.py [session glob, default 2026-*] [--out summary.txt] [--only label,label] [--set quick]
                          [--chunks N] [--each] [--port <port checkout>]

Replays every NNN_<label> folder (skipping the smoke tests) in the port and writes, per recording, the first divergence
and the first differing action pair (game | port, by base name without the foot / weights).
- By default all replays run in ONE port process (`replay_lab_traces_city`: Damascus loaded once, a fresh player per
  recording), then compare.py --no-run compares each; --each runs compare.py per recording (a process each, slow).
- --only keeps the recordings whose label contains one of the comma-separated words (e.g. --only climb,freerun_east).
- --set <name> replays only the recordings listed in `livelab/sets/<name>.txt` (one `session/NNN_label` per line,
  relative to livelab/sessions/, `#` starts a comment). `--set quick` is the set for checks during a group (68
  recordings). Its summary has the full summary's format, so diff_summaries.py compares it with a full one on the
  recordings they share.
- --chunks N splits the batch into N port processes run one after the other (each with its own list file), then
  writes one summary as a single run does. Each process loads Damascus again, so the order effects of a single batch
  (the city's streaming state carried from one recording to the next) restart at each chunk's first recording.
  Measured 2026-10-20 (318 recordings, one commit): one process 29.5 min, peak 19.0 GB; `--chunks 4` 16.5 min, chunk
  peaks 5.3 / 5.3 / 8.4 / 6.4 GB (the quick set, one process: 4.4 min, 7.4 GB). The split run differs from the single
  one only on the walling knife-edge twins (freestep50_walkon 10-15, climb10_courtyard 10-15).
- --port defaults to the `AC1-Movement-Rewritten-live` checkout next to this repository (the working checkout; the
  plain `AC1-Movement-Rewritten` one is the owner's and may sit on another branch).
"""
import glob
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SESSIONS = os.path.join(os.path.dirname(HERE), "sessions")
SETS = os.path.join(os.path.dirname(HERE), "sets")


def first_action_diff(compare_txt):
    lines = open(compare_txt, encoding="utf-8", errors="replace").read().splitlines()
    rows, on = [], False
    for ln in lines:
        if ln.startswith("ACTIONS"):
            on = True
            continue
        if ln.startswith("TABLE"):
            break
        if on and "|" in ln:
            g, p = ln.split("|", 1)
            rows.append((g.strip(), p.strip()))
    norm = lambda s: s.split(" ", 1)[-1].split(" + ")[0].replace(".man", "")[:48]
    for g, p in rows:
        if norm(g) != norm(p):
            return f"{g[:70]} | {p[:60]}"
    return "same action sequence"


def main(args):
    pattern = args[0] if args and not args[0].startswith("--") else "2026-*"
    out = args[args.index("--out") + 1] if "--out" in args else os.path.join(SESSIONS, "compare_summary.txt")
    only = args[args.index("--only") + 1].split(",") if "--only" in args else None
    chunks = int(args[args.index("--chunks") + 1]) if "--chunks" in args else 1
    candidates = sorted(glob.glob(os.path.join(SESSIONS, pattern, "[0-9][0-9][0-9]_*")))
    if "--set" in args:
        name = args[args.index("--set") + 1]
        listed = [ln.split("#", 1)[0].strip() for ln in open(os.path.join(SETS, name + ".txt"), encoding="utf-8")]
        candidates = [os.path.join(SESSIONS, *re.split(r"[\\/]", ln)) for ln in listed if ln]
        missing = [d for d in candidates if not os.path.isdir(d)]
        if missing:
            print("not in livelab/sessions:", *missing, sep="\n  ")
            return 1
    dirs = []
    for d in candidates:
        frames = os.path.join(d, "frames.csv")
        # the lab's own smoke test (check 00) records until it is stopped by hand: not a movement recording
        if any(k in d for k in ("smoke", "e2e", "check00")) or not os.path.exists(frames):
            continue
        with open(frames, encoding="utf-8", errors="replace") as f:
            if sum(1 for _ in zip(range(3), f)) < 3:   # an aborted recording: no frames
                continue
        if only and not any(w in os.path.basename(d) for w in only):
            continue
        dirs.append(d)
    batch = "--each" not in args
    if batch:
        port = os.path.normpath(os.path.join(HERE, "..", "..", "..", "AC1-Movement-Rewritten-live"))
        if "--port" in args:   # another port checkout (a worktree); its own list file, so two runs can share the sessions
            port = os.path.abspath(args[args.index("--port") + 1])
        size = -(-len(dirs) // max(chunks, 1))
        parts = [dirs[i:i + size] for i in range(0, len(dirs), size)]
        for n, part in enumerate(parts):
            suffix = f"_{n + 1}of{len(parts)}" if len(parts) > 1 else ""
            listing = os.path.join(SESSIONS, f"_replay_list_{os.path.basename(port)}{suffix}.txt")
            open(listing, "w", encoding="utf-8").write("".join(os.path.abspath(os.path.join(d, "frames.csv")) + "\n" for d in part))
            env = dict(os.environ, AC_LAB_TRACES=listing, CARGO_TARGET_DIR="target/claude")
            print(f"replaying {len(part)} recordings in one process" + (f" (chunk {n + 1} of {len(parts)})" if suffix else ""), flush=True)
            r = subprocess.run(["cargo", "test", "--release", "replay_lab_traces_city", "--", "--ignored", "--nocapture", "--exact",
                                "city::stream_tests::replay_lab_traces_city"], cwd=port, env=env, capture_output=True, text=True)
            if r.returncode != 0:
                print(r.stdout[-2000:], r.stderr[-3000:])
                return 1
    # the comparisons are independent: run them side by side
    from concurrent.futures import ThreadPoolExecutor
    extra = ["--no-run"] if batch else []
    with ThreadPoolExecutor(max_workers=8 if batch else 1) as pool:
        results = list(pool.map(lambda d: subprocess.run([sys.executable, os.path.join(HERE, "compare.py"), d] + extra, capture_output=True, text=True), dirs))
    report = []
    for d, r in zip(dirs, results):
        name = os.path.relpath(d, SESSIONS)
        txt = os.path.join(d, "compare.txt")
        if r.returncode != 0 or not os.path.exists(txt):
            report.append(f"{name}: compare failed")
            continue
        first = next((l for l in open(txt, encoding="utf-8", errors="replace") if l.startswith("FIRST DIVERGENCE")), "?").strip()
        report.append(f"{name}: {first}\n    actions: {first_action_diff(txt)}")
        print(report[-1], flush=True)
    open(out, "w", encoding="utf-8").write("\n".join(report) + "\n")
    print("written", out)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
