"""Compares the PARITY GAME lines (native_probe.cpp, GOW2_PARITY=1) with the PARITY CP lines of the command
processor, pairing them by (frame, draw index). Prints, per hashed group, the share of paired draws whose
values match, the first 10 mismatches, and the draw counts of each side per frame.

Where a frame has a different number of draws on each side (Clear, Resolve and other draws reach DRAW_INDX
but are not among the four probed functions), the frame is aligned instead: each game draw is paired with the
next CP draw whose prim and count agree, and the CP draws skipped on the way are reported as CP-only.

  python -I tools/native/parity_compare.py game.log
  python -I tools/native/parity_compare.py --selftest
"""
import re
import sys

LINE = re.compile(r"PARITY (GAME|CP) (.*)$")
TOKEN = re.compile(r"(\w+)=(\S+)")
NOT_GROUPS = {"f", "d", "fn", "prim", "n", "raw"}
FIRST_MISMATCHES = 10


def parse_int(text, radix):
    try:
        return int(text, radix)
    except ValueError:
        return None


def parse_line(text):
    """Returns (side, record) for a PARITY line, or None. record holds f, d, prim, n (None when unknown),
    the group values as strings, and fn when the line has one."""
    m = LINE.search(text)
    if not m:
        return None
    side = m.group(1)
    fields = dict(TOKEN.findall(m.group(2)))
    if "f" not in fields or "d" not in fields:
        return None
    # The command processor prints prim and n in hex, the game probe in decimal.
    radix = 16 if side == "CP" else 10
    record = {
        "f": int(fields["f"]),
        "d": int(fields["d"]),
        "fn": fields.get("fn", "-"),
        "prim": parse_int(fields.get("prim", "-"), radix),
        "n": parse_int(fields.get("n", "-"), radix),
        "groups": {k: v for k, v in fields.items() if k not in NOT_GROUPS},
    }
    # sub_82980210 and sub_8297FD30 take prim in r4 and the count in r7, like sub_82980B20.
    raw = fields.get("raw", "").split(",")
    if record["prim"] is None and record["fn"] in ("sub_82980210", "sub_8297FD30") and len(raw) == 5:
        record["prim"] = int(raw[0], 16) & 0x3F
        record["n"] = int(raw[3], 16)
    return side, record


def same_draw(game, cp):
    """Prim and count must agree where both sides know them; unknown (None) matches anything."""
    for key in ("prim", "n"):
        if game[key] is not None and cp[key] is not None and game[key] != cp[key]:
            return False
    return True


def pair_frame(game, cp):
    """Returns (pairs, game_only, cp_only, aligned) for the draws of one frame."""
    if len(game) == len(cp):
        cp_by_d = {c["d"]: c for c in cp}
        pairs = [(g, cp_by_d[g["d"]]) for g in game if g["d"] in cp_by_d]
        return pairs, len(game) - len(pairs), len(cp) - len(pairs), False
    pairs, game_only, cp_only, j = [], 0, 0, 0
    for g in game:
        k = next((k for k in range(j, len(cp)) if same_draw(g, cp[k])), None)
        if k is None:
            game_only += 1
            continue
        cp_only += k - j
        pairs.append((g, cp[k]))
        j = k + 1
    cp_only += len(cp) - j
    return pairs, game_only, cp_only, True


def compare(text):
    games, cps = {}, {}
    for line in text.splitlines():
        parsed = parse_line(line)
        if parsed is None:
            continue
        side, record = parsed
        (games if side == "GAME" else cps).setdefault(record["f"], []).append(record)
    for frames in (games, cps):
        for draws in frames.values():
            draws.sort(key=lambda r: r["d"])

    totals, mismatches, groups = {}, {}, []
    frame_rows = []
    for frame in sorted(set(games) | set(cps)):
        game, cp = games.get(frame, []), cps.get(frame, [])
        pairs, game_only, cp_only, aligned = pair_frame(game, cp)
        frame_rows.append((frame, len(game), len(cp), len(pairs), game_only, cp_only, aligned))
        for g, c in pairs:
            for name, value in g["groups"].items():
                if name not in groups:
                    groups.append(name)
                if name not in c["groups"]:
                    continue
                ok = value.lower() == c["groups"][name].lower()
                matched, paired = totals.get(name, (0, 0))
                totals[name] = (matched + ok, paired + 1)
                if not ok:
                    mismatches.setdefault(name, []).append((frame, g["d"], g["fn"], value, c["groups"][name]))
    return frame_rows, groups, totals, mismatches


def report(text):
    frame_rows, groups, totals, mismatches = compare(text)
    out = []
    for name in groups:
        matched, paired = totals.get(name, (0, 0))
        pct = 100.0 * matched / paired if paired else 0.0
        out.append(f"group {name}: {matched}/{paired} paired draws match ({pct:.2f}%)")
        for frame, d, fn, gv, cv in mismatches.get(name, [])[:FIRST_MISMATCHES]:
            out.append(f"  mismatch f={frame} d={d} fn={fn} game={gv} cp={cv}")
    out.append("per frame: frame game_draws cp_draws paired game_only cp_only")
    for frame, ng, nc, paired, game_only, cp_only, aligned in frame_rows:
        note = " (aligned by prim/count)" if aligned else ""
        out.append(f"  {frame} {ng} {nc} {paired} {game_only} {cp_only}{note}")
    if any(nc > ng for _, ng, nc, *_ in frame_rows):
        out.append("CP has draws the game side does not: draws outside the four probed functions reach DRAW_INDX.")
    return "\n".join(out), frame_rows, totals


SELFTEST_GAME = (
    "I 0.1 PARITY GAME f=300 d=0 fn=sub_82980B20 prim=4 n=6 surf=0000AAAA prog=00000001 rs=00000002 vp=00000003 "
    "scis=00000004 fetch=00000005 vsc=00000006 psc=00000007 A=82000100 B=82000200 raw=4,0,0,6,0"
)
SELFTEST_CP = (
    "I 0.2 PARITY CP f=300 d=0 prim=4 n=6 surf=0000AAAA prog=00000009 rs=00000002 vp=00000003 "
    "scis=00000004 fetch=00000005 vsc=00000006 psc=00000007 A=82000100 B=82000200"
)


def selftest():
    text = SELFTEST_GAME + "\n" + SELFTEST_CP + "\n"
    _, frame_rows, totals = report(text)
    failures = []
    if totals.get("surf") != (1, 1):
        failures.append(f"surf totals {totals.get('surf')}")
    if totals.get("prog") != (0, 1):
        failures.append(f"prog totals {totals.get('prog')}")
    if frame_rows != [(300, 1, 1, 1, 0, 0, False)]:
        failures.append(f"frame rows {frame_rows}")
    if failures:
        print("selftest FAILED: " + "; ".join(failures))
        return 1
    print("selftest OK")
    return 0


def main(argv):
    if len(argv) == 2 and argv[1] == "--selftest":
        return selftest()
    if len(argv) != 2:
        print("usage: python -I tools/native/parity_compare.py game.log | --selftest", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8", errors="replace") as f:
        text, _, _ = report(f.read())
    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
