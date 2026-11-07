#!/usr/bin/env python3
"""
Write docs/newopcodes.md and docs/cleo5.md: every function in
moonloader/lib/newopcodes.lua and cleo5.lua, from their def lines and
comments, with parameter names and links from Sanny Builder's library.

    git clone --depth 1 https://github.com/sannybuilder/library
    python3 tools/lib_docs.py library/sa/sa.json
"""
import json, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIB = os.path.join(ROOT, "moonloader", "lib") + os.sep
SA = sys.argv[1] if len(sys.argv) > 1 else sys.exit(__doc__)
OUT = os.path.join(ROOT, "docs") + os.sep

sa = json.load(open(SA))
cmds = {}
for ext in sa["extensions"]:
    for c in ext["commands"]:
        prev = cmds.get(c["id"])
        if prev is None or ext["name"] in ("NewOpcodes",) or prev[0] == "default":
            cmds[c["id"]] = (ext["name"], c)

LETTER = {"i": "int", "b": "bool", "f": "float", "s": "string", "x": "text", "v": "value", "u": "pointer"}
RENAME = {"self": None, "end": "finish", "function": "fn"}

def ident(p):
    n = p.get("name") or p.get("type", "value")
    if n in RENAME:
        n = RENAME[n] or p.get("type", "value")
        n = n[0].lower() + n[1:]
    return n

def parse(path):
    lines = open(path).read().split("\n")
    out, section, pending, i = [], None, [], 0
    defre = re.compile(r"^(def|audio)\('(\w+)',\s*0x([0-9A-F]{4}), '([^']*)', '([^']*)'(, true)?\)\s*(?:--\s*(.*))?$")
    fnre = re.compile(r"^function M\.(\w+)\(([^)]*)\)\s*(?:return.*?)?(?:--\s*(.*))?$")
    while i < len(lines):
        l = lines[i]
        m = defre.match(l)
        f = fnre.match(l)
        if m:
            kind, name, op, ins, outs, cond, note = m.groups()
            out.append(dict(section=section, name=name, op=op, ins=ins, outs=outs, cond=bool(cond),
                            note=(" ".join(pending) + (" " + note if note else "")).strip(), audio=kind == "audio"))
            pending = []
        elif f and f.group(1) != "_bind":
            name, args, note = f.groups()
            body = " ".join(lines[i:i + 4])
            om = re.search(r"call\(0x([0-9A-F]{4})", body)
            cm = re.search(r"call\(0x[0-9A-F]{4}, .*?, ('[^']*'|[^,]+), (true|false)", body)
            outs = cm.group(1).strip("'") if cm and cm.group(1).startswith("'") else "?"
            out.append(dict(section=section, name=name, op=om.group(1) if om else "", hand=args, outs=outs,
                            cond=bool(cm and cm.group(2) == "true"), note=(" ".join(pending) + " " + (note or "")).strip()))
            pending = []
        elif l.startswith("-- "):
            text = l[3:].strip()
            if (i == 0 or lines[i - 1].strip() == "") and not text.startswith("(") and not text.startswith("name, opcode"):
                section = text
                pending = []
            elif text.startswith("name, opcode"):
                section = None
            else:
                pending.append(text)
        elif l.strip() == "":
            pending = []
        i += 1
    return out

def row(e, base):
    ext, c = cmds.get(e["op"], (None, None))
    note = e["note"]
    names, rets = None, None
    m = re.match(r"^\(([^)]*)\)(?::\s*(.*))?$", note) if note else None
    if m:
        names = [n.strip() for n in m.group(1).split(",")]
        rets = m.group(2)
        note = ""
        if rets and not re.fullmatch(r"[\w ]+(, [\w ]+)*", rets.replace("x, y, z", "xyz")) or rets and len(rets.split()) > len(rets.split(",")) + 2:
            note, rets = rets, None
    elif note.startswith("(") and "):" in note:
        a, b = note.split("):", 1)
        names = [n.strip() for n in a[1:].split(",")]
        rets, note = b.strip(), ""
    if "hand" in e:
        args = e["hand"]
    else:
        consume = [t for t in e["ins"] if t not in "oe"]
        if e["audio"] and names and names[0] != "stream":
            names = ["stream"] + names
        if names and len(names) == len(consume):
            args = ", ".join(names)
        elif names and ("xyz" in " ".join(names)):
            args = ", ".join(names)
        elif c and len([p for p in c.get("input", []) if p.get("type") != "arguments"]) == len(consume):
            args = ", ".join(ident(p) for p in c.get("input", []) if p.get("type") != "arguments")
            if e["audio"]:
                args = "stream" + args[args.index(","):] if "," in args else "stream"
        else:
            args = ", ".join(LETTER.get(t, t) for t in consume)
            if e["audio"]:
                args = "stream" + args[args.index(","):] if "," in args else "stream"
    if rets is None:
        vals = e["outs"]
        if e["outs"] == "?":
            rets = "see the lib"
        elif c and vals and len(c.get("output", [])) == len(vals):
            rets = ", ".join(ident(p) for p in c["output"])
        else:
            rets = ", ".join(LETTER.get(t, t) for t in vals)
        if e["cond"]:
            rets = "ok" + (", " + rets if rets else "")
    op = e["op"]
    link = f"[{op}](https://library.sannybuilder.com/#/sa/script/extensions/{ext}/{op})" if ext else op
    scm = f"`{c['name']}`" if c else ""
    return f"| {link} | `{e['name']}({args})` | {rets or ''} | {scm} | {note} |"

def write(lib, title, intro, out):
    entries = parse(LIB + lib)
    text = [f"# {title}", "", intro, "",
            "Opcodes link to their page in [Sanny Builder's library](https://library.sannybuilder.com/#/sa); "
            "parameter names follow it. Functions with a condition return it first (`ok`), then their outputs.", ""]
    section = object()
    for e in entries:
        if e["section"] != section:
            section = e["section"]
            text += ["", f"## {(section or 'Matrices, entities, strings and game state')[0].upper() + (section or 'Matrices, entities, strings and game state')[1:]}", "",
                     "| Opcode | Function | Returns | SCM name | Notes |", "| --- | --- | --- | --- | --- |"]
        text.append(row(e, lib))
    open(OUT + out, "w").write("\n".join(text).replace("\n\n\n", "\n\n") + "\n")
    print(out, len(entries))

write("newopcodes.lua", "newopcodes.lua",
      "Every function in `moonloader/lib/newopcodes.lua`: NewOpcodes (0D00-0D7F) for MoonLoader scripts, "
      "run by moonsamp.asi with or without CLEO. `local no = require 'newopcodes'`.",
      "newopcodes.md")
write("cleo5.lua", "cleo5.lua",
      "Every function in `moonloader/lib/cleo5.lua`: CLEO 5's own opcodes for MoonLoader scripts, "
      "run by moonsamp.asi with or without CLEO. `local cleo5 = require 'cleo5'`.",
      "cleo5.md")
