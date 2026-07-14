#!/bin/bash
# build-settings.sh - cross-build xxri-settings (and reuses xxri-ui.h).
# Usage: build-settings.sh <fltk-include-dir> <tc-libs-dir> [outfile]
set -e
INC="$1"; LIBS="$2"; OUT="${3:-xxri-settings}"
HERE=$(cd "$(dirname "$0")" && pwd)
g++ -m32 -Os -fno-rtti -std=c++11 -I"$INC" "$HERE/xxri-settings.cxx" \
    -L"$LIBS" -Wl,-rpath-link,"$LIBS" -lfltk_images -lfltk -lX11 -lm \
    -mtls-dialect=gnu -lstdc++ -static-libgcc -o "$OUT"
strip "$OUT"; ls -l "$OUT"
