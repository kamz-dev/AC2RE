"""Export / import the shared IDA annotations of an IDB as text, so they can be merged in git.

IDBs are binary and never go in git. What people add to them (names, comments, function prototypes, local types)
goes through IDB/annotations/<idb file name>/ instead:

  names.tsv       addr<TAB>name                    user-set names (functions and data)
  comments.tsv    addr<TAB>kind<TAB>text           kind: c = comment, r = repeatable, f = function, F = function repeatable
  prototypes.tsv  addr<TAB>type                    function types set by hand
  types.h         local types (structs, enums, typedefs) as C declarations, one "// @type <name>" block each

Addresses are written as IDA shows them (hex, no 0x). Text escapes: \\t \\n \\\\.

Run it inside IDA (GUI: File > Script file..., it asks export / import) or headless through tools/ida/sync.py.
The rule: **import after every pull, export before every commit.** Export refuses to drop entries that are in the
repository but not in your IDB (you have not imported yet) unless called with force=True.

Inside IDA's Python, or an MCP py_eval:
    exec(open(r"<repo>/tools/ida/annotations.py").read(), g := {"__name__": "annotations"}); g["export_annotations"]()
"""
import pathlib
import re

import ida_bytes
import ida_funcs
import ida_name
import ida_nalt
import ida_typeinf
import idautils
import idc

KINDS = {"c": (False, False), "r": (True, False), "f": (False, True), "F": (True, True)}  # kind -> (repeatable, func)


def idb_path():
    return pathlib.Path(idc.get_idb_path())


def annotations_dir():
    """<repo>/IDB/annotations/<idb file name>; the IDB itself lives in <repo>/IDB/."""
    p = idb_path()
    return p.parent / "annotations" / p.name


def _esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\r", "").replace("\n", "\\n")


def _unesc(s):
    out, i = [], 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            out.append({"t": "\t", "n": "\n", "\\": "\\"}.get(n, "\\" + n))
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _read_tsv(path):
    if not path.exists():
        return []
    rows = []
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        r = line.split("\t")
        if len(r) < 2 or not re.fullmatch(r"[0-9A-Fa-f]+", r[0]) or (path.name == "comments.tsv" and r[1] not in KINDS):
            print(f"[annotations] {path.name}:{i}: skipped a malformed line: {line[:80]!r}")
            continue
        rows.append(r)
    return rows


def _write_tsv(path, header, rows):
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(header + "\n")
        for r in rows:
            f.write("\t".join(r) + "\n")


def _is_userti(ea):
    fn = getattr(ida_nalt, "is_userti", None)
    return bool(fn(ea)) if fn else False


def collect():
    """Read the annotations out of the open IDB."""
    names, comments, protos = {}, {}, {}
    for ea, name in idautils.Names():
        if ida_bytes.has_user_name(ida_bytes.get_flags(ea)):
            names[ea] = name
    for seg in idautils.Segments():
        for ea in idautils.Heads(seg, idc.get_segm_end(seg)):
            if not ida_bytes.has_cmt(ida_bytes.get_flags(ea)):
                continue
            for kind, rep in (("c", False), ("r", True)):
                t = idc.get_cmt(ea, rep)
                if t:
                    comments[(ea, kind)] = t
    for ea in idautils.Functions():
        for kind, rep in (("f", False), ("F", True)):
            t = idc.get_func_cmt(ea, rep)
            if t:
                comments[(ea, kind)] = t
        if _is_userti(ea):
            t = idc.get_type(ea)
            if t:
                protos[ea] = t
    return names, comments, protos


TYPE_MARK = "// @type "


def _clean_decl(text):
    lines = [ln.rstrip() for ln in text.replace("\r", "").split("\n")]
    lines = [ln for ln in lines if not ln.startswith("#define") and not re.fullmatch(r"/\* \d+ \*/", ln.strip())]
    return "\n".join(lines).strip()


def local_types():
    """{name: declaration} of every named local type. Anonymous nested types ('$' in the name) are left out: they
    come back with their parent."""
    limit = ida_typeinf.get_ordinal_limit(None) if hasattr(ida_typeinf, "get_ordinal_limit") else idc.get_ordinal_qty()
    out = {}
    for ordinal in range(1, limit):
        name = ida_typeinf.get_numbered_type_name(None, ordinal)
        if not name or "$" in name:
            continue
        text = _clean_decl(idc.print_decls(str(ordinal), 0) or "")
        if text:
            out[name] = text
    return out


def _read_types(path):
    blocks, name, buf = {}, None, []
    for line in path.read_text(encoding="utf-8").split("\n"):
        if line.startswith(TYPE_MARK):
            if name:
                blocks[name] = "\n".join(buf).strip()
            name, buf = line[len(TYPE_MARK):].strip(), []
        elif name:
            buf.append(line)
    if name:
        blocks[name] = "\n".join(buf).strip()
    return blocks


def _import_types(path):
    """Parse only the types that differ from the IDB's, retrying until dependencies resolve. Returns errors."""
    if not path.exists():
        return 0
    have = local_types()
    pending = {n: t for n, t in _read_types(path).items() if have.get(n) != t}
    flags = idc.PT_SIL | getattr(idc, "PT_REPLACE", getattr(ida_typeinf, "PT_REPLACE", 0))
    while pending:
        done = [n for n, t in pending.items() if (idc.parse_decls(t, flags) or 0) == 0]
        for n in done:
            del pending[n]
        if not done:
            break
    for n in pending:
        print(f"[annotations] could not import type {n}")
    return len(pending)


def export_annotations(force=False):
    d = annotations_dir()
    names, comments, protos = collect()
    missing = [r[0] for r in _read_tsv(d / "names.tsv") if len(r) >= 2 and int(r[0], 16) not in names]
    missing += [r[0] for r in _read_tsv(d / "comments.tsv") if len(r) >= 3 and (int(r[0], 16), r[1]) not in comments]
    if missing and not force:
        print(f"[annotations] {len(missing)} entries in {d} are not in this IDB (first: {missing[:5]}).")
        print("[annotations] Import first (import_annotations()), or export_annotations(force=True) to drop them.")
        return False
    _write_tsv(d / "names.tsv", "# addr\tname", [(f"{ea:X}", n) for ea, n in sorted(names.items())])
    _write_tsv(d / "comments.tsv", "# addr\tkind (c r f F)\ttext",
               [(f"{ea:X}", k, _esc(t)) for (ea, k), t in sorted(comments.items())])
    _write_tsv(d / "prototypes.tsv", "# addr\ttype", [(f"{ea:X}", _esc(t)) for ea, t in sorted(protos.items())])
    types = local_types()
    with open(d / "types.h", "w", encoding="utf-8", newline="\n") as f:
        f.write("// Local types of " + idb_path().name + ", one block per type, sorted by name. Written by\n")
        f.write("// tools/ida/annotations.py; edit types in IDA, not here.\n")
        for name in sorted(types):
            f.write(f"\n{TYPE_MARK}{name}\n{types[name]}\n")
    print(f"[annotations] exported {len(names)} names, {len(comments)} comments, {len(protos)} prototypes, "
          f"{len(types)} types to {d}")
    return True


def import_annotations():
    d = annotations_dir()
    if not d.exists():
        print(f"[annotations] nothing to import: {d} does not exist")
        return False
    errors = _import_types(d / "types.h")
    n = c = p = 0
    for r in _read_tsv(d / "names.tsv"):
        ea, name = int(r[0], 16), r[1]
        if idc.get_name(ea) != name:
            if ida_name.set_name(ea, name, ida_name.SN_NOWARN | ida_name.SN_NOCHECK):
                n += 1
            else:
                errors += 1
                print(f"[annotations] could not name {ea:X} {name}")
    for r in _read_tsv(d / "comments.tsv"):
        ea, kind, text = int(r[0], 16), r[1], _unesc(r[2] if len(r) > 2 else "")
        rep, func = KINDS[kind]
        if func:
            if idc.get_func_cmt(ea, rep) != text and ida_funcs.get_func(ea):
                idc.set_func_cmt(ea, text, rep)
                c += 1
        elif idc.get_cmt(ea, rep) != text:
            idc.set_cmt(ea, text, rep)
            c += 1
    for r in _read_tsv(d / "prototypes.tsv"):
        ea, t = int(r[0], 16), _unesc(r[1])
        if idc.get_type(ea) != t:
            if idc.SetType(ea, t):
                p += 1
            else:
                errors += 1
                print(f"[annotations] could not set type at {ea:X}: {t}")
    print(f"[annotations] imported from {d}: {n} names, {c} comments, {p} prototypes changed; {errors} errors")
    return errors == 0


if __name__ == "__main__":
    args = idc.ARGV[1:] if len(idc.ARGV) > 1 else []
    if args:
        ok = export_annotations(force="--force" in args) if args[0] == "export" else import_annotations()
    else:
        import ida_kernwin
        b = ida_kernwin.ask_buttons("Export", "Import", "Cancel", 1, "AC2RE annotations: export to or import from the repository?")
        if b == 1:
            export_annotations()
        elif b == 0:
            import_annotations()
