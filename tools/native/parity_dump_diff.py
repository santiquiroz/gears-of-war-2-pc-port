"""Diffs the PARITY GAMEDUMP lines (native_probe.cpp, GOW2_PARITY_DUMP_FRAME=<n>) against the PARITY CPDUMP lines
of the command processor. Draws are paired like parity_compare.py does (parse_line, pair_frame); for every paired
draw with a dump on both sides, each block (vsc: 0x4000-0x43FF, psc: 0x4400-0x47FF, fetch: 0x4800-0x48BF) is
compared dword by dword. Prints per function and block how many draws differ, the 20 most frequent differing vec4
registers (dword index // 4), and the first 16 differing (index, game, cp) triples of the first 3 differing draws.

  python -I tools/native/parity_dump_diff.py game.log
  python -I tools/native/parity_dump_diff.py --selftest
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import parity_compare  # noqa: E402

DUMP = re.compile(r"PARITY (GAMEDUMP|CPDUMP) f=(\d+) d=(\d+) blk=(\w+) hex=([0-9A-Fa-f]*)")
BLOCKS = ("vsc", "psc", "fetch")
TOP_REGISTERS = 20
FIRST_DRAWS = 3
FIRST_TRIPLES = 16


def parse(text):
    """Returns (games, cps, dumps): draws per frame for each side, and dumps[(side, f, d, blk)] = [dwords]."""
    games, cps, dumps = {}, {}, {}
    for line in text.splitlines():
        m = DUMP.search(line)
        if m:
            side = "GAME" if m.group(1) == "GAMEDUMP" else "CP"
            hexs = m.group(5)
            dumps[(side, int(m.group(2)), int(m.group(3)), m.group(4))] = [
                int(hexs[i:i + 8], 16) for i in range(0, len(hexs) - 7, 8)]
            continue
        parsed = parity_compare.parse_line(line)
        if parsed is None:
            continue
        side, record = parsed
        (games if side == "GAME" else cps).setdefault(record["f"], []).append(record)
    for frames in (games, cps):
        for draws in frames.values():
            draws.sort(key=lambda r: r["d"])
    return games, cps, dumps


def diff(text):
    """Returns stats[(fn, blk)] = {"draws", "differ", "hist", "examples"}."""
    games, cps, dumps = parse(text)
    stats = {}
    for frame in sorted(games):
        pairs, _, _, _ = parity_compare.pair_frame(games[frame], cps.get(frame, []))
        for g, c in pairs:
            for blk in BLOCKS:
                gd = dumps.get(("GAME", g["f"], g["d"], blk))
                cd = dumps.get(("CP", c["f"], c["d"], blk))
                if gd is None or cd is None:
                    continue
                s = stats.setdefault((g["fn"], blk), {"draws": 0, "differ": 0, "hist": {}, "examples": []})
                s["draws"] += 1
                n = max(len(gd), len(cd))
                bad = [i for i in range(n) if i >= len(gd) or i >= len(cd) or gd[i] != cd[i]]
                if not bad:
                    continue
                s["differ"] += 1
                for reg in {i // 4 for i in bad}:
                    s["hist"][reg] = s["hist"].get(reg, 0) + 1
                if len(s["examples"]) < FIRST_DRAWS:
                    triples = [(i, gd[i] if i < len(gd) else None, cd[i] if i < len(cd) else None)
                               for i in bad[:FIRST_TRIPLES]]
                    s["examples"].append((g["f"], g["d"], c["d"], len(bad), triples))
    return stats


def word(v):
    return "--------" if v is None else f"{v:08X}"


def report(text):
    stats = diff(text)
    out = []
    if not stats:
        out.append("no paired draws with dumps on both sides")
    for (fn, blk), s in sorted(stats.items()):
        out.append(f"{fn} {blk}: {s['differ']}/{s['draws']} paired draws differ")
        top = sorted(s["hist"].items(), key=lambda kv: (-kv[1], kv[0]))[:TOP_REGISTERS]
        if top:
            out.append("  vec4 registers (index//4: draws): " + " ".join(f"{r}:{n}" for r, n in top))
        for f, gd, cd, count, triples in s["examples"]:
            out.append(f"  f={f} game d={gd} cp d={cd}: {count} dwords differ")
            out.append("    " + " ".join(f"({i},{word(a)},{word(b)})" for i, a, b in triples))
    return "\n".join(out), stats


def hexs(values):
    return "".join(f"{v:08X}" for v in values)


def selftest():
    vsc = [0] * 1024
    vsc_cp = list(vsc)
    vsc_cp[5] = 0x3F800000
    vsc_cp[7] = 1
    lines = [
        "I PARITY GAME f=300 d=0 fn=sub_82980B20 prim=4 n=3 surf=00000001",
        f"I PARITY GAMEDUMP f=300 d=0 blk=vsc hex={hexs(vsc)}",
        f"I PARITY GAMEDUMP f=300 d=0 blk=fetch hex={hexs([1] * 192)}",
        "I PARITY CP f=300 d=0 prim=4 n=3 surf=00000001",
        f"I PARITY CPDUMP f=300 d=0 blk=vsc hex={hexs(vsc_cp)}",
        f"I PARITY CPDUMP f=300 d=0 blk=fetch hex={hexs([1] * 192)}",
    ]
    text, stats = report("\n".join(lines) + "\n")
    failures = []
    s = stats.get(("sub_82980B20", "vsc"))
    if s is None or s["draws"] != 1 or s["differ"] != 1 or s["hist"] != {1: 1}:
        failures.append(f"vsc stats {s}")
    elif s["examples"][0][4] != [(5, 0, 0x3F800000), (7, 0, 1)]:
        failures.append(f"vsc triples {s['examples']}")
    f = stats.get(("sub_82980B20", "fetch"))
    if f is None or f["draws"] != 1 or f["differ"] != 0:
        failures.append(f"fetch stats {f}")
    if ("sub_82980B20", "psc") in stats:
        failures.append("psc counted without dumps")
    if "1:1" not in text:
        failures.append("histogram missing from report")
    if failures:
        print("selftest FAILED: " + "; ".join(failures))
        return 1
    print("selftest OK")
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--selftest":
        return selftest()
    if len(argv) != 2:
        print("usage: python -I tools/native/parity_dump_diff.py game.log | --selftest", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8", errors="replace") as f:
        text, _ = report(f.read())
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
