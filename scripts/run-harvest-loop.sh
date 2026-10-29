#!/usr/bin/env bash
# Boot the game, harvest the "unregistered function" FATAL address into hints/runtime.toml,
# regenerate + rebuild, repeat. Stops on a run without that FATAL, a build failure, a
# repeated address or after MAX_ITERS rounds.
# Usage: scripts/run-harvest-loop.sh [max_iters] [run_seconds]
set -u
cd "$(dirname "$0")/.."
MAX_ITERS=${1:-10}
RUN_SECONDS=${2:-90}
SDK=${REXGLUE_SDK:-C:/personal/tools/rexglue/0.10.0.9/win-amd64}
GAME=${GOW2_GAME:-C:\\personal\\gow2-gamedata\\retail}
HINTS=hints/runtime.toml
mkdir -p out/runs out/logs
[ -f "$HINTS" ] || printf '# Found by booting the game (FATAL: call to unregistered function).\n[functions]\n' > "$HINTS"

fatal_address() {
  grep -o -E "unregistered function at guest address 0x[0-9A-F]{8}" "$1" | tail -1 | grep -o -E "0x[0-9A-F]{8}"
}

regenerate_and_build() {
  local i=$1
  "$SDK/bin/rexglue.exe" codegen gow2_manifest.toml > "out/logs/loop$i.codegen" 2>&1 || return 1
  python -I tools/find_orphan_gotos.py generated/default >> "out/logs/loop$i.codegen" || return 1
  pwsh -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -Sdk "$SDK" -SkipCodegen > "out/logs/loop$i.build" 2>&1
}

for i in $(seq 1 "$MAX_ITERS"); do
  log="out/runs/loop$i.log"
  rm -f "$log"
  winlog=$(cygpath -w "$PWD/$log")
  (cd out/build/release && timeout "$RUN_SECONDS" ./gow2.exe --gow2_launcher=false --game_data_root="$GAME" --log_file="$winlog" > /dev/null 2>&1)
  addr=$(fatal_address "$log")
  if [ -z "$addr" ]; then
    echo "iter $i: no unregistered-function FATAL; last lines:"; tail -3 "$log"; exit 0
  fi
  if grep -q -i "^$addr " "$HINTS"; then
    echo "iter $i: $addr already hinted, still fatal - needs a manual look"; exit 2
  fi
  echo "iter $i: adding $addr"
  echo "$addr = {}  # boot run $i" >> "$HINTS"
  regenerate_and_build "$i" || { echo "iter $i: codegen/build failed, see out/logs/loop$i.*"; exit 3; }
done
echo "reached $MAX_ITERS iterations"
