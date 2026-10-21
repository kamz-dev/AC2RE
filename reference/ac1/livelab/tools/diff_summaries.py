"""diff_summaries.py - compare two compare_all.py summaries recording by recording.

  py tools/diff_summaries.py <before.txt> <after.txt>

Per recording in both: the first divergence time (none = matches to the end). Better = later or none, worse = earlier
(by more than 0.02 s) or a new divergence. Prints the counts and every changed recording.
"""
import re
import sys


def load(path):
    out = {}
    for line in open(path, encoding="utf-8", errors="replace"):
        if ": FIRST DIVERGENCE: " not in line:
            continue
        name, d = line.rstrip("\n").split(": FIRST DIVERGENCE: ", 1)
        key = re.split(r"[\\/]", name)[-2] + "/" + re.split(r"[\\/]", name)[-1]
        t = re.search(r"at ([\d.]+) s", d)
        out[key] = (None if d.startswith("none") else (float(t.group(1)) if t else 0.0), d)
    return out


def main(a, b):
    before, after = load(a), load(b)
    common = sorted(set(before) & set(after))
    better, worse, same = [], [], 0
    for k in common:
        (tb, db), (ta, da) = before[k], after[k]
        if tb == ta or (tb is not None and ta is not None and abs(tb - ta) <= 0.02):
            same += 1
        elif ta is None or (tb is not None and ta > tb):
            better.append((k, db, da))
        else:
            worse.append((k, db, da))
    ends = lambda s: sum(1 for k in common if s[k][0] is None)
    print(f"{len(common)} in common; match to the end {ends(before)} -> {ends(after)}; better {len(better)}, worse {len(worse)}, same {same}")
    for title, rows in (("BETTER", better), ("WORSE", worse)):
        for k, db, da in rows:
            print(f"{title} {k}\n    before: {db}\n    after:  {da}")


if __name__ == "__main__":
    main(*sys.argv[1:3])
