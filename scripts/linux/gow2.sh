#!/bin/sh
# Starts the game from its own folder: gow2 reads gamecontrollerdb.txt from the working directory,
# and gow2.toml, logs and the remembered game path live beside it.
cd "$(dirname "$(readlink -f "$0")")" || exit 1
# Files extracted on Windows lose the executable bit; the player may have fixed only this script.
[ -x ./gow2 ] || chmod +x ./gow2 2>/dev/null
exec ./gow2 "$@"
