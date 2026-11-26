"""Generate an English PC checkpoint message from the player's own Coalesced bundle.
Output-only by default. Never commit generated game bundles.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
OLD = "Saving content. Please don't turn off your console."
NEW = "Saving content. Please don't turn off your PC."

def records(data):
    position = 0
    def integer():
        nonlocal position
        if position + 4 > len(data): raise ValueError('Truncated integer')
        value = struct.unpack_from('>i', data, position)[0]; position += 4
        return value
    def count():
        value = integer()
        if not 0 <= value <= 1000000: raise ValueError('Invalid entry count')
        return value
    def string():
        nonlocal position
        start = position; n = integer(); size = abs(n) * (2 if n < 0 else 1)
        if size > len(data) - position: raise ValueError('Truncated string')
        raw = data[position:position + size]; position += size
        unit = 2 if n < 0 else 1
        if n and raw[-unit:] != bytes(unit): raise ValueError('Missing terminator')
        value = raw[:-unit].decode('utf-16-le' if n < 0 else 'latin-1') if n else ''
        return value, start, position, n
    entries = []
    for _ in range(count()):
        filename = string()[0]
        for _ in range(count()):
            section = string()[0]
            for _ in range(count()):
                key = string()[0]; value, start, end, n = string()
                entries.append((filename, section, key, value, start, end, n))
    return entries

def patch(data):
    entries = records(data)
    hits = [e for e in entries if e[0].replace('\\', '/').lower().endswith('/geargameui.int')
            and e[1] == 'UpdatingStrings' and e[2] == 'SavingContent_Message']
    if len(hits) != 1: raise ValueError('Expected exactly one English checkpoint message')
    filename, section, key, value, start, end, n = hits[0]
    if value.rstrip() == NEW: return data
    if value.rstrip() != OLD: raise ValueError('Unknown message revision; unchanged')
    desired = NEW.ljust(len(value))
    encoding = 'utf-16-le' if n < 0 else 'latin-1'
    encoded = struct.pack('>i', n) + (desired + '\0').encode(encoding)
    if len(encoded) != end - start: raise ValueError('Serialized size mismatch')
    output = data[:start] + encoded + data[end:]
    after = records(output)
    expected = [e[:3] + ((desired if e is hits[0] else e[3]),) + e[4:] for e in entries]
    if after != expected: raise ValueError('Unexpected other entry changed')
    assert len(output) == len(data)
    return output

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve(); repo = Path(__file__).resolve().parents[2]
    if work == repo or repo in work.parents: raise SystemExit('Generated output must stay outside the source checkout')
    source = args.game_root.resolve() / 'GearGame/CookedXenon/Coalesced_int.bin'
    if args.apply:
        active = subprocess.check_output(['tasklist','/FI','IMAGENAME eq gow2.exe','/FO','CSV','/NH'], text=True, errors='replace')
        if '"gow2.exe"' in active.lower(): raise SystemExit('Close GoW2 before --apply')
    original = source.read_bytes(); fixed = patch(original)
    work.mkdir(parents=True, exist_ok=True)
    (work / 'Coalesced_int.bin').write_bytes(fixed)
    changed = fixed != original
    if args.apply and changed:
        backup = work / 'before-Coalesced_int.bin'
        if backup.exists() and backup.read_bytes() != original: raise SystemExit('Backup differs; choose a new work directory')
        if not backup.exists(): shutil.copy2(source, backup)
        temp = source.with_name(source.name + '.pc-text.tmp')
        with temp.open('xb') as f: f.write(fixed); f.flush(); os.fsync(f.fileno())
        os.replace(temp, source)
        stamp = Path.home() / 'AppData/Local/GearsOfWar2PC/cache/coalesced/Coalesced_int.bin.stamp'
        if stamp.exists():
            shutil.copy2(stamp, work / 'before-cache.stamp'); stamp.unlink()
    result = {'changed': changed, 'applied': args.apply, 'size_preserved': len(fixed) == len(original),
              'message': NEW, 'sha256': hashlib.sha256(fixed).hexdigest()}
    (work / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
if __name__ == '__main__': main()
