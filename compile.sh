#!/usr/bin/env sh

# Move to the app directory
cd "$(dirname "$(readlink -f "$0")")" || exit

OUTPUT="xorg-render"
SOURCES="src/main.c"
CONFIG="-std=c90 -pedantic -Wall -Wextra"
LIBS="-lX11 -lm -lpng"

# Compile the app
exec gcc $CONFIG -o $OUTPUT $SOURCES $LIBS "$@"