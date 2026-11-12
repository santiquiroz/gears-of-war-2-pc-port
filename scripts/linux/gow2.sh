#!/bin/sh
# Starts the game from its own folder: gow2 reads gamecontrollerdb.txt from the working directory,
# and gow2.toml, logs and the remembered game path live beside it.
cd "$(dirname "$(readlink -f "$0")")" || exit 1
# Files extracted on Windows lose the executable bit; the player may have fixed only this script.
[ -x ./gow2 ] || chmod +x ./gow2 2>/dev/null
# The binary is built for x86-64-v3 (Steam Deck, Legion Go, Intel 2013+, AMD 2015+); an older CPU
# would only crash with "Illegal instruction".
for flag in avx2 fma bmi2 movbe f16c; do
  grep -qw "$flag" /proc/cpuinfo || { echo "gow2 needs an x86-64-v3 CPU (AVX2, FMA, BMI2, MOVBE); this one lacks $flag." >&2; exit 1; }
done
exec ./gow2 "$@"
