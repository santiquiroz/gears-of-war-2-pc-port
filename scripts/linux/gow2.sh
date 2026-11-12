#!/bin/sh
# Starts the game from its own folder: gow2 reads gamecontrollerdb.txt from the working directory,
# and gow2.toml, logs and the remembered game path live beside it.
cd "$(dirname "$(readlink -f "$0")")" || exit 1
exec ./gow2 "$@"
